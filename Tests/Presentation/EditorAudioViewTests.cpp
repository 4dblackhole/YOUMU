#include <Windows.h>
#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "Editing/ChartEditor.h"
#include "EditorMenuTests.h"
#include "EditorAudioAnalysisIntegrationTests.h"
#include "EditorMetronomeTests.h"
#include "EditorModeTests.h"
#include "EditorPlaybackTests.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
#include "GameScene/MusicSelectScene/Submodules/SongPreviewController.h"
#include <chrono>
#include <atomic>
#include <iostream>
#include <fstream>
#include <thread>

using namespace PhantomCore;

namespace PhantomCore
{
struct InputStateTestAccess
{
    static void Wheel(InputState &input, float delta)
    {
        input.Reset();
        input.AddMouseWheel(delta, 1);
    }
    static void Key(InputState &input, std::uint16_t key)
    {
        input.Reset();
        input.SetKey(key, true, 1);
    }
    static void Reset(InputState &input) { input.Reset(); }
    static void Click(InputState &input, int x, int y)
    {
        input.Reset();
        input.SetMousePosition(x, y, true);
        input.SetMouseButton(MouseButton::Left, true, 1);
    }
    // The frame after Click: the button comes up over the same point.
    static void Release(InputState &input, int x, int y)
    {
        input.Reset();
        input.SetMousePosition(x, y, true);
        input.mouseButtonsDown_[static_cast<std::size_t>(MouseButton::Left)] = true;
        input.SetMouseButton(MouseButton::Left, false, 2);
    }
};
} // namespace PhantomCore


namespace
{

    std::size_t previewLoadCalls{};
    std::unique_ptr<IAudioClipBackend> CountClipLoads(IAudioBackend &backend,
        const std::filesystem::path &path, AudioLoadMode mode, std::string &error)
    {
        ++previewLoadCalls;
        return CreateFmodAudioClipBackend(backend, path, mode, error);
    }
    void Check(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    class RecordingRenderer final : public Visual2DRenderSystem
    {
      public:
        std::vector<DrawPacket> packets;
        ImageHandle LoadImageFile(const std::filesystem::path &) override
        {
            return {++imageId};
        }
        std::uint64_t imageId{};
        Size GetImageSize(ImageHandle) const noexcept override
        {
            return {};
        }
        void SubmitScreen(const Visual2DCanvas &canvas, const RenderContext &, Point,
                          std::uint32_t) override
        {
            packets.clear();
            const auto batches = canvas.BuildDrawList();
            ForEachDrawPrimitive(batches, [this](const DrawPacket &packet) { packets.push_back(packet); });
        }
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const Float4x4 &,
                         Size, const Float4x4 &) override
        {
        }
        RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override
        {
            return {};
        }
        void RenderToTexture(const Visual2DCanvas &, const RenderTargetTextureHandle &,
                             const RenderContext &) override
        {
        }
    };
    Visual2DNode *FindNode(Visual2DNode &node, const std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (auto *found = FindNode(*child, name))
                return found;
        return nullptr;
    }
    std::vector<DrawPacket> SpectrumGeometry(Visual2DNode &root)
    {
        std::vector<DrawPacket> result;
        for (const auto name : {"Audio.MusicSpectrum", "Audio.HitSpectrum"})
            if (auto *track = FindNode(root, name))
                for (const auto &tile : track->Children())
                    if (tile->Name() == "Spectrum.Tile")
                        if (auto *geometry = tile->GetComponent<EditorVisual>()) geometry->AppendDrawPackets(result);
        return result;
    }

    // Save publishes the YME it actually wrote, so the session keeps opening that file.
    void TestSavePublishesEffectPath()
    {
        const auto root = std::filesystem::absolute("build/fixtures/EditorSaveEffectPath");
        std::filesystem::create_directories(root);
        std::ofstream(root / "Song.ymm") << "Version: 1\nFile: Music.wav\nMusic Name 1: Editor Test\n";
        std::ofstream(root / "Pattern.ymp") <<
            "Version: 1\nMusic metadata: Song.ymm\nPattern Name: Editor Test\nMode: Taiko\n"
            "Base BPM: 120\nJudgeLevel: 50\nHP: 9\n[Pattern]\n0/1,1,0\n1/2,2,0\n";
        GameplayLaunchRequest request{root / "Pattern.ymp", {}, root / "Music.wav", "Taiko"};
        request.songsRoot = root;
        auto workspace = std::make_unique<EditorWorkspace>(request);
        workspace->Initialize();
        workspace->AddNote({2, {}}, 1);
        workspace->Save();
        Check(!workspace->Document().Dirty() && workspace->Request().effectPath.has_value() &&
                  *workspace->Request().effectPath == workspace->Document().Effects().sourcePath,
              "A successful save must publish the actual YME selected by Save.");
    }

    void TestAudioRangeSelection(Visual2DCanvas &canvas, EditorView &view, EditorWorkspace &state,
                                 RecordingRenderer &renderer, const TextCatalog &texts)
    {
        auto *track = FindNode(canvas.Root(), "Audio.Selection.0");
        if (!track)
            throw std::runtime_error("Waveform selection input missing.");
        Check(track->IsVisible(), "Waveform selection input must be visible in the audio tab.");
        const auto bounds = track->BoundsInCanvas();
        const auto revision = state.Document().Revision();
        const double firstMs = state.TimeMilliseconds();
        const double lastMs = firstMs + state.audioWindow * 500;
        const auto oldGeometry = SpectrumGeometry(canvas.Root());
        Visual2DInputRouter router;
        PointerInput pointer;
        pointer.position = {bounds.x + bounds.width * .75F, bounds.y + bounds.height * .5F};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(canvas, pointer);
        pointer.leftButtonPressed = false;
        pointer.position.x = bounds.x + bounds.width * .25F;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
        const auto selected = state.SelectedAudioRange();
        if (!selected)
            throw std::runtime_error("Dragging did not create an audio range.");
        Check(std::abs(selected->begin.count() / 1000.0 - firstMs) <= .005 &&
                  std::abs(selected->end.count() / 1000.0 - lastMs) <= .005,
              "Reverse dragging must produce ordered source times through the transformed track bounds.");
        view.Build();
        renderer.packets.clear();
        // Expand the same draw packets as the recording renderer without a physical window.
        ForEachDrawPrimitive(canvas.BuildDrawList(), [&](const auto &packet) { renderer.packets.push_back(packet); });
        const double begin = selected->begin.count() / 1000.0, end = selected->end.count() / 1000.0;
        const auto label = texts.Format("Editor.audioSelectionFormat", begin, end, end - begin);
        Check(std::ranges::any_of(renderer.packets, [&](const auto &packet) {
                  return packet.text && packet.text->value == label;
              }), "Selection start/end/duration must be displayed with localized millisecond formatting.");
        Check(std::ranges::count_if(renderer.packets, [](const auto &packet) {
                  return packet.type == DrawPacketType::Rectangle && std::abs(packet.color.alpha - .24F) < .001F;
              }) == 3, "The same selected interval must shade all three audio tracks.");
        const auto newGeometry = SpectrumGeometry(canvas.Root());
        Check(std::ranges::all_of(newGeometry, [&](const auto &packet) {
                  return std::ranges::any_of(oldGeometry, [&](const auto &old) { return old.rectangles == packet.rectangles; });
              }), "Dragging a selection must reuse FFT tiles instead of rebuilding analysis geometry.");

        // Captured drags keep updating outside the hit rectangle and clamp to the viewport.
        pointer.leftButtonReleased = false;
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointer.position.x = bounds.x + bounds.width * .75F;
        router.Process(canvas, pointer);
        pointer.leftButtonPressed = false;
        pointer.position.x = bounds.x - 200;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
        Check(std::abs(state.SelectedAudioRange()->begin.count() / 1000.0 -
                       (state.TimeMilliseconds() - state.audioWindow * 250)) <= .005 &&
                  state.Document().Revision() == revision,
              "Selection must clamp outside drags and leave chart content and revision unchanged.");
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string assetError;
        Check(InitializeBuiltInAssets(assetError), assetError.c_str());
        TestEditorMenu();
        TestEditorPlayback();
        TestEditorMetronome();

        PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.patternOffsetMilliseconds = -500;
        EditorWorkspace range(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        range.AddNote({0, {}}, 1);
        range.AddNote({1, {}}, 2);
        Check(range.TimelineRangeMilliseconds() == std::pair{-500.0, 1500.0},
              "Timeline must include negative notes and the last note.");

        const auto bundledSongs = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
                                  "YOUMU.Assets/Assets/Songs";
        RunEditorAudioAnalysisIntegrationTests(bundledSongs);
        // Default regressions use a fixed shipped catalog. An arbitrary first
        // user song can exceed the analysis budget in an unoptimized build.
        // An explicit filter still exercises the requested user song.
        const auto songsRoot = argc > 1 ? UserSongs() : bundledSongs;
        const auto songs = SongCatalog{}.Load(songsRoot);
        Check(!songs.songs.empty() && !songs.songs.front().patterns.empty(), "Real catalog missing");
        auto songIt = songs.songs.begin();
        if (argc > 1)
        {
            const std::u8string_view filter{reinterpret_cast<const char8_t*>(argv[1])};
            songIt = std::ranges::find_if(songs.songs, [&](const auto& item) {
                return item.metadataPath.generic_u8string().find(filter) != std::u8string::npos;
            });
            Check(songIt != songs.songs.end(), "Requested timing song is missing.");
        }
        const auto &song = *songIt;
        TestEditorPlaybackWithFmod(song.audioPath);
        const auto &selected = song.patterns.front();
        GameplayLaunchRequest launch{selected.patternPath, selected.effectPath, song.audioPath, selected.pattern.mode};
        launch.songsRoot = songsRoot;
        {
            AudioSystem previewAudio;
            AudioConfig config;
            config.preferredBackend = AudioOutputBackend::NoSound;
            std::string error;
            Check(previewAudio.Initialize(config, CreateFmodAudioBackend,
                                           CountClipLoads, error), error.c_str());
            auto clip = previewAudio.LoadSound(song.audioPath, AudioLoadMode::StreamAsync, error);
            Check(clip != nullptr, error.c_str());
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            auto state = clip->LoadState(error);
            while (state == AudioClipLoadState::Loading && std::chrono::steady_clock::now() < deadline)
            {
                previewAudio.Update();
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
                state = clip->LoadState(error);
            }
            Check(state == AudioClipLoadState::Ready, "Async MP3 never became ready");
            auto voice = clip->Play({}, nullptr, error);
            Check(voice != nullptr, "Ready async stream must play");
            voice.reset();
            clip.reset();

            AudioPlaybackManager playback;
            SongPreviewController preview(playback);
            preview.Initialize(previewAudio);
            // The preview follows a song by its audio path, as song select and the title player drive it.
            const std::filesystem::path missing = song.audioPath / "not-a-file";
            std::filesystem::path focused = song.audioPath;
            bool resume = true;
            previewLoadCalls = 0;
            const auto poll = [&] {
                previewAudio.Update();
                playback.Update();
                preview.SyncTrack(focused, resume);
                static_cast<void>(preview.UpdatePreviewAudio(.02, SongPreviewController::EndBehavior::Repeat));
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            };
            const auto awaitVoice = [&] {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
                do { poll(); } while (!playback.PlaybackCount() && std::chrono::steady_clock::now() < deadline);
                Check(playback.PlaybackCount() == 1, "Prepared preview must start one managed voice");
            };
            const auto awaitPosition = [&](const std::uint64_t beyond) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
                do { poll(); } while (preview.State().PositionMicroseconds() <= beyond && std::chrono::steady_clock::now() < deadline);
                return preview.State().PositionMicroseconds() > beyond;
            };
            awaitVoice();
            Check(previewLoadCalls == 1, "Focused ready preview must not reload");
            Check(preview.State().IsPlaying() && preview.State().DurationMicroseconds().value_or(0) > 2'000'000,
                  "An opened song plays and reports its length");
            Check(awaitPosition(0), "A playing song advances its position");

            resume = false;
            preview.Pause();
            const auto paused = preview.State().PositionMicroseconds();
            for (int i = 0; i < 20; ++i) poll();
            Check(preview.State().Transport() == MusicTransport::Paused && preview.State().PositionMicroseconds() == paused &&
                      playback.PlaybackCount() == 1,
                  "Pause keeps the voice and its position without resuming on the title");
            const auto middle = *preview.State().DurationMicroseconds() / 2;
            preview.Seek(middle);
            for (int i = 0; i < 20; ++i) poll();
            Check(preview.State().PositionMicroseconds() == middle && playback.PlaybackCount() == 1 && previewLoadCalls == 1,
                  "Seeking a paused song moves it in place without reopening the file");
            preview.Play();
            Check(awaitPosition(middle), "A seeked song resumes from the new position");
            preview.Stop();
            for (int i = 0; i < 20; ++i) poll();
            Check(preview.State().Transport() == MusicTransport::Stopped && preview.State().PositionMicroseconds() == 0 &&
                      playback.PlaybackCount() == 1 && previewLoadCalls == 1,
                  "Stop rewinds and keeps the voice ready without reopening the file");
            resume = true;
            Check(awaitPosition(0) && previewLoadCalls == 1, "Song select resumes a stopped song in place");
            preview.RestartTrack();
            Check(preview.State().PositionMicroseconds() == 0 && previewLoadCalls == 1,
                  "Restarting the same song rewinds it without reopening the file");

            // The end of a song: the title is told so it can step on; song select starts it over.
            const auto nearEnd = *preview.State().DurationMicroseconds() - 300'000;
            // Report: true once the song played out. Repeat: true once it was reopened from the start.
            const auto awaitEnd = [&](const SongPreviewController::EndBehavior behavior) {
                const auto loads = previewLoadCalls;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
                while (std::chrono::steady_clock::now() < deadline)
                {
                    previewAudio.Update();
                    playback.Update();
                    if (preview.UpdatePreviewAudio(.02, behavior))
                        return true;
                    if (previewLoadCalls > loads)
                        return true;
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                }
                return false;
            };
            preview.Seek(nearEnd);
            Check(awaitEnd(SongPreviewController::EndBehavior::Report) &&
                      preview.State().Transport() == MusicTransport::Stopped && playback.PlaybackCount() == 0,
                  "A song that plays out reports its end once and stops");
            preview.RestartTrack();
            awaitVoice();
            Check(previewLoadCalls == 2, "Restarting a song that played out reopens it once");
            preview.Seek(nearEnd);
            Check(awaitEnd(SongPreviewController::EndBehavior::Repeat) && preview.State().IsPlaying() &&
                      preview.State().PositionMicroseconds() == 0 && previewLoadCalls == 3,
                  "Song select starts a song that played out again from its beginning");
            awaitVoice();

            focused = missing;
            for (int i = 0; i < 100; ++i) poll();
            Check(previewLoadCalls == 4 && preview.TrackFailed(), "Failed preview must not reopen its file every Update");
            focused = song.audioPath;
            awaitVoice();
            Check(previewLoadCalls == 5, "Changing selection must allow preview loading again");
            preview.ReleaseAudio();
            Check(playback.PlaybackCount() == 0 && preview.CurrentTrack() == song.audioPath &&
                      preview.State().Transport() == MusicTransport::Stopped,
                  "Leaving for play or the editor releases the voice but keeps the song");
            awaitVoice();
            Check(previewLoadCalls == 6, "Returning to song select reopens the released song");
            preview.StopPreviewAudio();
            Check(playback.PlaybackCount() == 0 && preview.CurrentTrack().empty(), "Stopping preview must stop its own voice");
            for (int i = 0; i < 10; ++i) static_cast<void>(preview.UpdatePreviewAudio(.02, SongPreviewController::EndBehavior::Repeat));
            preview.Shutdown();
        }
        // Cancellation must leave the shared queue available for the next real
        // analysis, including when the requesting editor has been destroyed.
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            EditorWorkspace cancelled(launch);
            cancelled.Initialize();
            cancelled.UpdateAnalysis();
            cancelled.Analysis().Stop();
            Check(!cancelled.Analysis().Running(), "Stopped worker must detach its editor immediately");
        }
        EditorWorkspace state(launch);
        state.Initialize();
        Check(state.MusicMetadataPath() == song.metadataPath.lexically_normal() &&
                  state.Document().SourcePath() == selected.patternPath &&
                  (!selected.effectPath || state.Document().Effects().sourcePath == *selected.effectPath),
              "Folder actions must resolve the currently edited YMM/YMP/YME paths.");
        const auto analysisStarted = std::chrono::steady_clock::now();
        state.UpdateAnalysis();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{120};
        while (state.Analysis().Running() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            state.UpdateAnalysis();
        }
        Check(!state.Analysis().Running(), "Audio worker did not finish within 120 seconds");
        Check(state.Analysis().Data().errors.empty(), state.Analysis().Data().errors.c_str());
        std::cout << "Editor source analysis elapsed_ms="
                  << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - analysisStarted).count()
                  << '\n';
        if (argc > 1)
        {
            const auto& music = state.Analysis().Data().sounds.at("Music");
            const auto attack = std::ranges::find_if(music.frames, [](const auto& frame) {
                return frame.peak > .0316227766F;
            });
            Check(attack != music.frames.end(), "Requested timing song has no measurable attack.");
            const double onsetBucket = static_cast<double>(attack - music.frames.begin()) *
                                       music.secondsPerFrame * 1000;
            state.SeekFirstNote();
            const double firstNote = state.TimeMilliseconds();
            Check(firstNote >= onsetBucket && firstNote < onsetBucket + music.secondsPerFrame * 1000,
                  "The first note must fall inside the editor waveform's first attack interval.");
            std::cout << "Canonical editor timing: first_note_ms=" << firstNote
                      << " waveform_attack_bucket_ms=" << onsetBucket
                      << " spectrum_frame_ms=" << music.secondsPerFrame * 1000 << '\n';
            state.Seek(0);
        }
        const auto musicDuration = state.Analysis().MusicDurationSeconds();
        if (!musicDuration)
            throw std::runtime_error("Current music duration is unavailable after analysis.");
        const double musicEndMs = *musicDuration * 1000;
        const double lastNoteMs = state.Document().NoteTimeRange().second.count() / 1000.0;
        Check(std::abs(lastNoteMs - musicEndMs) > 1,
              "The real chart must have distinct music/note ends for X testing.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == lastNoteMs, "First X must seek the real chart's last note.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == musicEndMs, "Second X must seek the music end.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == lastNoteMs, "Third X must return to the last note.");
        state.Analysis().Invalidate();
        Check(!state.Analysis().MusicDurationSeconds(), "Invalidated analysis must not provide a stale end position.");
        bool pending = false;
        try
        {
            state.SeekLastNote();
        }
        catch (const std::runtime_error &)
        {
            pending = true;
        }
        Check(pending && state.TimeMilliseconds() == lastNoteMs, "An unavailable end must not move the cursor.");
        state.UpdateAnalysis(); // Unchanged files can reuse their completed analysis.
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == musicEndMs, "Retry must retain the second-X destination.");
        state.Seek(0);
        const auto [begin, end] = state.TimelineRangeMilliseconds();
        Check(end >= state.Analysis().Data().sounds.at("Music").durationSeconds * 1000,
              "Timeline must include the entire music file.");

        RecordingRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1920, 1080});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "Client/Texts/Languages");
        EditorView view(visuals, state, texts);
        view.Initialize({meshes, textRendering, renderer, audio, 1920, 1080});
        const auto hasText = [&](const std::wstring_view text)
        {
            return std::ranges::any_of(renderer.packets,
                                       [&](const auto &packet) { return packet.text && packet.text->value == text; });
        };
        visuals.Render({});
        Check(hasText(L"File"), "Editor view must compose the English top menu.");
        Check(hasText(std::wstring(state.Document().Dirty() ? Editor(texts).saveDirty
                                                            : Editor(texts).save)),
              "Existing footer Save must remain available.");
        Check(hasText(L"Compose") && hasText(L"Notes") && hasText(L"Scroll speed") && !hasText(L"Waveform"),
              "Compose must stack the channel heads without the audio analysis tracks.");
        state.Seek(state.Document().Notes().front().timing.count() / 1000.0 + 1000);
        state.SelectTab(EditorTab::Audio);
        view.Build();
        visuals.Render({});
        Check(hasText(L"Waveform") && hasText(L"Music") && hasText(L"Hit sounds"), "Three audio tracks missing");
        const auto coloredIn = [&](float top, float bottom)
        {
            return std::ranges::count_if(
                renderer.packets,
                [&](const auto &packet)
                {
                    const auto corner = TransformPoint(
                        {packet.bounds.x, packet.bounds.y + packet.bounds.height, 0}, packet.nodeTransform);
                    const float y = 360 - corner.y;
                    return packet.type == DrawPacketType::Rectangle && y >= top && y < bottom &&
                           corner.x >= 24 - 640 && corner.x < 1256 - 640 && packet.color.red > .3F &&
                           packet.color.green < .95F;
                });
        };
        // The frame's track rows: waveform 1.2, music 1, expected hits .8 of the body below the tool row.
        const float trackUnit = (642.0F - 138.0F - 20.0F - 24.0F) / 3;
        const float musicTop = 138 + trackUnit * 1.2F + 36, hitTop = musicTop + trackUnit + 8;
        Check(coloredIn(musicTop, musicTop + trackUnit) > 10 && coloredIn(hitTop, hitTop + trackUnit * .8F) > 10,
              "Music and expected hits must produce separate visible spectrum "
              "cells.");

        auto *canvas = visuals.FindCanvas(1);
        if (canvas == nullptr)
            throw std::runtime_error("Editor Canvas missing");
        const auto geometryBefore = SpectrumGeometry(canvas->Root());
        state.Seek(state.TimeMilliseconds() + 16);
        view.Build();
        const auto geometryAfter = SpectrumGeometry(canvas->Root());
        const auto reused = std::ranges::count_if(
            geometryAfter,
            [&](const auto &packet)
            {
                return packet.rectangles && std::ranges::any_of(geometryBefore, [&](const auto &old)
                                                                { return old.rectangles == packet.rectangles; });
            });
        Check(reused >= 8, "Moving audio time must reuse both tracks' cached tile geometry.");
        const auto waitDetails = [&] {
            const auto began = std::chrono::steady_clock::now();
            const auto until = began + std::chrono::seconds{90};
            while (state.Analysis().SpectrumRunning() && std::chrono::steady_clock::now() < until)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{5});
                state.UpdateAnalysis();
            }
            Check(!state.Analysis().SpectrumRunning(), "Visible spectrum analysis did not finish.");
            std::cout << "Editor detail analysis wait_ms="
                      << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count()
                      << '\n';
            view.Build();
        };
        waitDetails();
        const auto visibleColumns = static_cast<std::size_t>(std::ceil((1280.0F - 48) * 1.5F));
        const auto detailBands = static_cast<std::size_t>(std::ceil(trackUnit * 1.5F));
        const double tileSeconds = state.audioWindow / visibleColumns * 128;
        const double viewBegin = state.TimeMilliseconds() / 1000 - state.audioWindow * .25;
        const auto tileIndex = static_cast<std::int64_t>(std::floor(viewBegin / tileSeconds));
        const EditorSpectrumRequest detailRequest{0, {tileIndex * tileSeconds, tileSeconds,
            128, detailBands, 20, state.Analysis().Data().sounds.at("Music").maximumFrequencyHz}};
        const auto detail = state.Analysis().Spectrum(detailRequest);
        Check(detail && detail->columns == 128 && detail->bands == detailBands &&
                  detail->levels.size() == 128 * detailBands,
              "The Client must display PCM spectrum tiles at physical pixel resolution.");
        state.Seek(state.TimeMilliseconds() + 1);
        view.Build();
        Check(state.Analysis().Spectrum(detailRequest) == detail,
              "A small seek must reuse the same completed PCM tile.");
        const auto oldWindow = state.audioWindow;
        state.audioWindow /= 2;
        view.Build();
        const auto zoomed = SpectrumGeometry(canvas->Root());
        Check(std::ranges::none_of(zoomed,
                                   [&](const auto &packet)
                                   {
                                       return packet.rectangles &&
                                              std::ranges::any_of(geometryAfter, [&](const auto &old)
                                                                  { return old.rectangles == packet.rectangles; });
                                   }),
              "Changing audio zoom must invalidate the cached tile resolution.");
        state.audioWindow = oldWindow;
        view.Build();
        waitDetails();
        TestAudioRangeSelection(*canvas, view, state, renderer, texts);
        auto *menuBar = FindNode(canvas->Root(), "Editor.MenuBar");
        if (menuBar == nullptr)
            throw std::runtime_error("Editor top menu is missing.");
        Check(menuBar->BoundsInCanvas().height == EditorMenuBar::Height, "Editor top menu geometry is wrong.");
        auto *slider = FindNode(canvas->AnchorNode(Anchor::Center), "Editor timeline");
        if (slider == nullptr)
            throw std::runtime_error("Audio timeline slider missing");
        Check(slider->IsVisible(), "Audio timeline slider hidden");
        InputState input;
        const auto navigationRevision = state.Document().Revision();
        state.Seek(0);
        state.SetDivision(4);
        InputStateTestAccess::Wheel(input, -1);
        Check(!view.Update({0, 0, 1, input, audio, {}}), "Audio wheel unexpectedly left the editor.");
        const auto nextTime = state.Document().Timeline().Compile({0, {1, 16}}).count() / 1000.0;
        Check(state.TimeMilliseconds() == nextTime, "Audio wheel must advance the selected beat grid.");
        InputStateTestAccess::Wheel(input, 1);
        static_cast<void>(view.Update({0, 0, 1, input, audio, {}}));
        Check(state.TimeMilliseconds() == state.Document().Timeline().Compile({0, {}}).count() / 1000.0,
              "Audio wheel must return to the chart's grid origin.");
        InputStateTestAccess::Key(input, VK_F10);
        static_cast<void>(view.Update({0, 0, 1, input, audio, {}}));
        const auto menuTime = state.TimeMilliseconds();
        InputStateTestAccess::Wheel(input, -1);
        static_cast<void>(view.Update({0, 0, 1, input, audio, {}}));
        Check(state.TimeMilliseconds() == menuTime, "An open menu must block wheel navigation below it.");
        InputStateTestAccess::Key(input, VK_ESCAPE);
        Check(!view.Update({0, 0, 1, input, audio, {}}), "Escape must dismiss the menu without leaving.");
        InputStateTestAccess::Reset(input);
        Check(state.Document().Revision() == navigationRevision, "Navigation must not edit the chart.");
        Visual2DInputRouter pointerRouter;
        PointerInput pointer{};
        pointer.available = true;
        const auto timelineBounds = slider->BoundsInCanvas();
        pointer.position = {timelineBounds.x + timelineBounds.width * .5F, timelineBounds.y + timelineBounds.height * .5F};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointerRouter.Process(*canvas, pointer);
        Check(!view.Update({0, 0, 1, input, audio, {}}), "Unexpected scene exit");
        Check(std::abs(state.TimeMilliseconds() - (begin + end) * .5) <= 1,
              "Slider action must seek the editor time to its track position.");
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        pointerRouter.Process(*canvas, pointer);
        state.SelectTab(EditorTab::Metadata);
        view.Build();
        Check(slider->IsVisible(), "The footer timeline is shared by every tab.");
        auto *hiddenSelection = FindNode(canvas->Root(), "Audio.Selection.0");
        if (!hiddenSelection)
            throw std::runtime_error("Audio controls missing after tab switch.");
        Check(!hiddenSelection->IsVisible(), "Metadata must hide audio selection hit areas.");
        state.SelectTab(EditorTab::Audio);
        texts.SetLanguage(Language::English);
        view.Build();
        visuals.Render({});
        Check(hasText(L"Audio") && hasText(L"Waveform") && hasText(L"File"), "Audio and menu labels not localized");
        Check(hasText(L"Metronome") && hasText(L"−1") && hasText(L"+1"),
              "Audio tab must expose offset adjustment and metronome controls.");
        // Footer rate: − and + step 0.05 within 0.25..2.00; the value resets to 1.00x.
        const auto clickFrame = [&](float x, float y) {
            InputStateTestAccess::Click(input, static_cast<int>(x * 1.5F), static_cast<int>(y * 1.5F));
            Check(!view.Update({0, 0, 1, input, audio, {}}), "Rate control unexpectedly left the editor.");
            InputStateTestAccess::Reset(input);
            static_cast<void>(view.Update({0, 0, 1, input, audio, {}}));
        };
        const float rateLeft = 172 + 12 + 44 + 12 + 128 + 12, rateY = 656 + 32;
        for (int i = 0; i < 25; ++i)
            clickFrame(rateLeft + 30 + 62 + 15, rateY);
        Check(state.PlaybackRate() == 2.0, "The + step must stop at double speed.");
        visuals.Render({});
        Check(hasText(L"2.00x"), "Current rate label missing.");
        for (int i = 0; i < 40; ++i)
            clickFrame(rateLeft + 15, rateY);
        Check(state.PlaybackRate() == .25, "The − step must stop at quarter speed.");
        clickFrame(rateLeft + 30 + 31, rateY);
        Check(state.PlaybackRate() == 1.0, "Clicking the rate value must return to 1.00x.");
        state.SetPlaybackRate(1);
        // Leaving with unsaved changes asks through the shared alert. Nothing here saves the bundled chart.
        {
            const auto frame = [&](std::uint16_t key) {
                if (key)
                    InputStateTestAccess::Key(input, key);
                else
                    InputStateTestAccess::Reset(input);
                return view.Update({0, 0, 1, input, audio, {}});
            };
            const auto alertOpen = [&] {
                // The scrim carries the alert's visibility.
                const auto *scrim = FindNode(canvas->Root(), "Alert");
                return scrim != nullptr && scrim->IsVisible();
            };
            state.SelectNotes({});
            state.ClosePanel();
            state.AddNote({7, {1, 8}}, 1);
            Check(state.Document().Dirty(), "The alert fixture needs an unsaved edit.");
            bool left = false;
            for (int i = 0; i < 3 && !alertOpen(); ++i)
                left = left || frame(VK_ESCAPE);
            Check(!left && alertOpen(), "Escape with unsaved changes must ask instead of leaving.");
            visuals.Render({});
            Check(hasText(L"Unsaved changes") && hasText(L"Discard") && hasText(L"Cancel") && hasText(L"Save"),
                  "The unsaved alert offers Discard, Cancel and Save.");
            const auto timeBefore = state.TimeMilliseconds();
            Check(!frame('Z') && state.TimeMilliseconds() == timeBefore, "Editor shortcuts must wait behind the alert.");
            Check(!frame(VK_ESCAPE) && !alertOpen() && state.Document().Dirty(),
                  "Escape on the alert cancels and keeps the edit.");
            for (int i = 0; i < 3 && !alertOpen(); ++i)
                static_cast<void>(frame(VK_ESCAPE));
            static_cast<void>(frame(0));
            const auto *discard = FindNode(canvas->Root(), "Alert.Button0");
            Check(discard != nullptr && alertOpen(), "The alert must reopen with its Discard button.");
            const auto bounds = discard->BoundsInCanvas();
            const auto logical = canvas->LogicalSize();
            const int x = static_cast<int>((bounds.x + bounds.width * .5F + logical.width * .5F) * 1.5F);
            const int y = static_cast<int>((logical.height * .5F - bounds.y - bounds.height * .5F) * 1.5F);
            InputStateTestAccess::Click(input, x, y);
            left = view.Update({0, 0, 1, input, audio, {}});
            InputStateTestAccess::Release(input, x, y);
            left = view.Update({0, 0, 1, input, audio, {}}) || left;
            Check(left && !alertOpen(), "Discard on the alert must leave the editor.");
            state.Undo();
            Check(!state.Document().Dirty(), "The alert fixture must restore the bundled chart's saved state.");
            static_cast<void>(frame(0));
        }
        visuals.OnResize(900, 720);
        view.Resize(900, 720);
        view.Build();
        Check(state.TimelineRangeMilliseconds() == std::pair{begin, end}, "Resize changed time range");
        Check(menuBar->BoundsInCanvas().width == canvas->LogicalSize().width,
              "Editor view must resize the composed menu.");
        waitDetails();
        TestAudioRangeSelection(*canvas, view, state, renderer, texts);
        state.SelectTab(EditorTab::Compose);
        state.SetRealtime(true);
        view.Build();
        visuals.Render({});
        Check(hasText(L"−") && hasText(L"+") && hasText(L"Spacing") && hasText(L"50%"),
              "Realtime view must expose the note spacing buttons and current spacing.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "Editor Canvas survived shutdown");
        visuals.OnResize(1920, 1080);
        TestEditorModes(visuals, {meshes, textRendering, renderer, audio, 1920, 1080}, texts,
                        [&renderer]() -> const std::vector<DrawPacket> & { return renderer.packets; });
        TestSavePublishesEffectPath();
        std::cout << "Editor audio view tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
