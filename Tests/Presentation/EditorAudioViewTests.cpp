#include <Windows.h>
#include "App/AssetPaths.h"
#include "Automation/InterpolationExpression.h"
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
#include "ScopedTestFolder.h"
#include <chrono>
#include <cmath>
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
    // A left press stamped with QPC ticks, for double-click timing.
    static void ClickAt(InputState &input, int x, int y, std::int64_t ticks, std::int64_t frequency)
    {
        input.Reset();
        input.SetPerformanceCounterFrequency(frequency);
        input.SetMousePosition(x, y, true);
        input.SetMouseButton(MouseButton::Left, true, ticks);
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
        const ScopedTestFolder fixture(std::filesystem::absolute("build/fixtures"), L"EditorSaveEffectPath");
        const auto &root = fixture.Path();
        const auto music = root / "Song/Music", set = root / "Song/Chart/Set";
        std::filesystem::create_directories(music);
        std::filesystem::create_directories(set);
        std::ofstream(music / "Song.ymm") << "Version: 1\nFile: Music.wav\nMusic Name 1: Editor Test\n";
        std::ofstream(set / "Pattern.ymp") <<
            "Version: 1\nPattern Name: Editor Test\nMode: Taiko\n"
            "Base BPM: 120\nJudgeLevel: 50\nHP: 9\n[Pattern]\n0/1,1,0\n1/2,2,0\n";
        GameplayLaunchRequest request{set / "Pattern.ymp", {}, music / "Music.wav", "Taiko"};
        request.songsRoot = root;
        auto workspace = std::make_unique<EditorWorkspace>(request);
        workspace->Initialize();
        workspace->AddNote({2, {}}, 1);
        workspace->Save();
        Check(!workspace->Document().Dirty() && workspace->Request().effectPath.has_value() &&
                  *workspace->Request().effectPath == workspace->Document().Effects().sourcePath,
              "A successful save must publish the actual YME selected by Save.");
    }

    // Finished tiles stay until the byte limit; then the ones shown longest ago leave first.
    void TestSpectrumCache()
    {
        const auto request = [](std::size_t track, double begin) {
            return EditorSpectrumRequest{track, {begin, 1, 128, 64, 20, 20'000}};
        };
        const auto tile = [] {
            auto result = std::make_shared<SpectrumTile>();
            result->columns = 128;
            result->bands = 64;
            result->levels.resize(128 * 64);
            return result;
        };
        EditorSpectrumCache one;
        one.Store(request(0, 0), tile());
        const auto tileBytes = one.Bytes();
        Check(tileBytes >= 128 * 64 && EditorSpectrumCache::DefaultByteLimit == 32 * 1024 * 1024,
              "A stored tile must count at least its levels against the 32 MiB limit.");

        EditorSpectrumCache cache(tileBytes * 3);
        const auto first = tile();
        cache.Store(request(0, 0), first);
        cache.Store(request(0, 1), tile());
        cache.Store(request(1, 0), tile());
        Check(cache.Find(request(0, 0)) == first, "A stored tile must be found by its request.");
        cache.Store(request(1, 1), tile());
        Check(cache.Count() == 3 && cache.Bytes() <= tileBytes * 3, "The cache must stay within its byte limit.");
        Check(cache.Contains(request(0, 0)) && !cache.Contains(request(0, 1)),
              "The tile shown longest ago must leave first; a tile just shown must stay.");
        cache.ClearTrack(1);
        Check(cache.Count() == 1 && cache.Contains(request(0, 0)) && cache.Bytes() == tileBytes,
              "Clearing the expected-hit track must keep the music tiles.");
        cache.Clear();
        Check(cache.Count() == 0 && cache.Bytes() == 0 && !cache.Find(request(0, 0)),
              "A cleared cache must hold nothing.");
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
        TestSpectrumCache();

        PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.patternOffsetMilliseconds = -500;
        EditorWorkspace range(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        range.AddNote({0, {}}, 1);
        range.AddNote({1, {}}, 2);
        Check(range.TimelineRangeMilliseconds() == std::pair{-500.0, 1500.0},
              "Timeline must include negative notes and the last note.");

        // The tracked built-in song shares the development Songs folder with local songs.
        const auto repo = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
        const auto songsRoot = repo / "Client/Assets/Songs";
        constexpr std::u8string_view BuiltInSong = u8"Takahashi yoko - Angel Dream Handshaking";
        RunEditorAudioAnalysisIntegrationTests(
            songsRoot / BuiltInSong / "Music/angel dream hand shaking.mp3",
            repo / "YOUMU.Assets/Assets/Skins/Default Skin");
        // Default regressions use the built-in song. An arbitrary local song
        // can exceed the analysis budget in an unoptimized build.
        // An explicit filter still exercises the requested local song.
        const auto songs = SongCatalog{}.Load(songsRoot);
        const std::u8string_view filter =
            argc > 1 ? std::u8string_view{reinterpret_cast<const char8_t *>(argv[1])} : BuiltInSong;
        const auto songIt = std::ranges::find_if(songs.songs, [&](const auto &item) {
            return item.metadataPath.generic_u8string().find(filter) != std::u8string::npos;
        });
        Check(songIt != songs.songs.end() && !songIt->patterns.empty(), "Requested timing song is missing.");
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
                  playback.PlaybackCount() == 1, "Stop rewinds and keeps the voice ready without reopening the file");
            Check(previewLoadCalls == 1, "Stop rewinds and keeps the voice ready without reopening the file");
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
                  preview.State().PositionMicroseconds() == 0,
                  "Song select starts a song that played out again from its beginning");
            Check(previewLoadCalls == 3, "Song select starts a song that played out again from its beginning");
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
              state.Document().SourcePath() == selected.patternPath,
              "Folder actions must resolve the currently edited YMM/YMP/YME paths.");
        Check((!selected.effectPath || state.Document().Effects().sourcePath == *selected.effectPath),
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
        Check(hasText(L"Compose") && hasText(L"Notes") && hasText(L"Scroll speed"),
              "Compose must stack the channel heads without the audio analysis tracks.");
        Check(!hasText(L"Waveform"), "Compose must stack the channel heads without the audio analysis tracks.");
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
                    const bool inBand = y >= top && y < bottom;
                    const bool inFrame = inBand && corner.x >= 24 - 640 && corner.x < 1256 - 640;
                    const bool tinted = packet.color.red > .3F && packet.color.green < .95F;
                    return packet.type == DrawPacketType::Rectangle && inFrame && tinted;
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
        Check(detail && detail->columns == 128 && detail->bands == detailBands,
              "The Client must display PCM spectrum tiles at physical pixel resolution.");
        Check(detail->levels.size() == 128 * detailBands,
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
        Check(state.Analysis().Spectrum(detailRequest) == detail && !state.Analysis().SpectrumRunning(),
              "Returning to an earlier visible range must reuse its finished tiles without analysing again.");
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
        // Channel heads: the caption beside the icon opens and closes the panel like the icon does.
        {
            namespace layout = editor_layout;
            state.SelectTab(EditorTab::Compose);
            state.ClosePanel();
            const auto clickAt = [&](float x, float y) {
                view.Build();
                clickFrame(x, y);
            };
            const auto middle = [](EditorPanel channel) { return layout::ChannelTop(channel) + layout::ChannelHeight(channel) * .5F; };
            clickAt(layout::HeadButtonWidth + 40, middle(EditorPanel::Volume));
            Check(state.Panel() == EditorPanel::Volume, "Clicking a channel caption must open its panel.");
            clickAt(layout::HeadWidth - 20, middle(EditorPanel::Volume));
            Check(!state.Panel(), "Clicking the open channel's head again must close its panel.");
            clickAt(18, middle(EditorPanel::Timing));
            Check(state.Panel() == EditorPanel::Timing, "The head icon must still open its panel.");
            clickAt(layout::HeadButtonWidth + 30, layout::LaneTop + 20);
            Check(state.Panel() == EditorPanel::Notes, "The lane caption must open the Notes panel.");
            const double zoom = state.Score().realtimeZoom;
            clickAt(layout::HeadButtonWidth + 8 + 15, layout::LaneTop + layout::LaneHeight - 28);
            Check(state.Panel() == EditorPanel::Notes && state.Score().realtimeZoom < zoom,
                  "The lane head's spacing − must keep its own click.");
            state.SetRealtimeZoom(zoom);
            state.ClosePanel();
        }
        // The clock button beside a panel's position fields types in the Beat tick nearest the cursor.
        {
            const auto clickClock = [&](std::wstring_view measureLabel, float buttonOffset) {
                view.Build();
                visuals.Render({});
                const auto label = std::ranges::find_if(renderer.packets, [measureLabel](const auto &packet) {
                    return packet.text && packet.text->value == measureLabel;
                });
                Check(label != renderer.packets.end(), "The panel must label its measure field.");
                const auto left = TransformPoint({label->bounds.x, label->bounds.y + label->bounds.height * .5F, 0},
                                                 label->nodeTransform);
                // The caption sits 10px above the 32px field's top edge; the button is 32px wide.
                clickFrame(left.x + 640 + buttonOffset + 16, 360 - left.y + 10 + 16);
            };
            const auto seek = [&](MusicalPosition position) {
                state.Seek(state.Document().Timeline().Compile(position).count() / 1000.0);
            };
            // Effect forms: measure and beat each give up 20px of their third, so the button starts 170.67px in.
            const float effectOffset = ((editor_layout::InspectorWidth - 40 - 16) / 3 - 20 + 8) * 2;
            state.OpenPanel(EditorPanel::Volume);
            state.effectForm.endMeasure = "9";
            state.effectForm.endFraction = "0/1";
            seek({3, {1, 8}});
            clickClock(L"Measure", effectOffset);
            Check(state.effectForm.startMeasure == "4" && state.effectForm.startFraction == "1/8",
                  "The start clock button must type the cursor's measure and beat.");
            seek({5, {3, 4}});
            clickClock(L"End measure", effectOffset);
            Check(state.effectForm.endMeasure == "6" && state.effectForm.endFraction == "3/4",
                  "The end clock button must type the cursor's measure and beat.");
            state.effectForm.endMeasure.clear();
            state.effectForm.endFraction.clear();
            state.OpenPanel(EditorPanel::Timing);
            state.timingForm.kind = EditorTimingKind::Bpm;
            seek({2, {1, 4}});
            clickClock(L"Measure", ((editor_layout::InspectorWidth - 40 - 16 - 32) * .5F + 8) * 2);
            Check(state.timingForm.measure == "3" && state.timingForm.fraction == "1/4",
                  "The Timing panel's clock button must type the cursor's measure and beat.");
            state.ClosePanel();
            state.Seek(0);
        }
        // A hit-sound Volume Area: the Curve button cycles Linear → Exponential → Harmonic and wraps.
        // Its click runs after the frame that listed the curves, so it must not hold an iterator into it.
        {
            state.SelectTab(EditorTab::Compose);
            state.OpenPanel(EditorPanel::Volume);
            auto &form = state.effectForm;
            form.selection = EffectCommandType::BusVolume;
            form.audioBus = "HitSound";
            form.endMeasure = "3";
            form.endFraction = "0/1";
            form.curve = AutomationCurve::Linear;
            form.curveName.clear();
            const auto clickCurve = [&] {
                view.Build();
                visuals.Render({});
                const auto label = std::ranges::find_if(
                    renderer.packets, [](const auto &packet) { return packet.text && packet.text->value == L"Curve"; });
                Check(label != renderer.packets.end(), "The Volume Area panel must show its Curve row.");
                const auto center = TransformPoint({label->bounds.x + label->bounds.width * .5F,
                                                    label->bounds.y + label->bounds.height * .5F, 0},
                                                   label->nodeTransform);
                // The button starts 26px right of the 44px label's center.
                clickFrame(center.x + 640 + 56, 360 - center.y);
            };
            for (const auto expected : {AutomationCurve::Exponential, AutomationCurve::Harmonic, AutomationCurve::Linear,
                                        AutomationCurve::Exponential})
            {
                clickCurve();
                Check(form.curve == expected && form.curveName.empty(),
                      "The Curve button must step to the next built-in curve and wrap to Linear.");
            }
            form.startMeasure = "2";
            form.startFraction = "0/1";
            form.beginValue = "0.25";
            form.endValue = "1";
            state.ApplyEffect();
            const auto &commands = state.Document().Effects().commands;
            const auto volume = std::ranges::find_if(commands, [](const EffectCommand &command) {
                return command.type == EffectCommandType::BusVolume && command.position == MusicalPosition{1, {}};
            });
            Check(volume != commands.end() && volume->curve == AutomationCurve::Exponential,
                  "Applying the Volume Area must keep the chosen curve.");
            Check(std::abs(EvaluateInterpolation(*volume, .5) - .5) < 1e-9,
                  "The Volume Area must follow its curve (Exponential .25 to 1 is .5 halfway).");
            state.Undo();
            state.ClosePanel();
            state.SelectTab(EditorTab::Audio);
            view.Build();
        }
        // Double-click on a realtime note: the second press moves the cursor to the note; one press does not.
        {
            state.SelectTab(EditorTab::Compose);
            state.SetRealtime(true);
            state.ClosePanel();
            state.SelectTool(0);
            const double noteMs = static_cast<double>(state.Document().Notes().front().timing.count()) / 1000.0;
            state.Seek(noteMs + 5);
            view.Build();
            // Judge line and lane centre of the 16:9 frame with no panel open, in window pixels.
            const int judgeX = static_cast<int>((184 + 152 * .62F) * 1.5F), laneY = static_cast<int>(236 * 1.5F);
            const auto press = [&](std::int64_t ticks) {
                InputStateTestAccess::ClickAt(input, judgeX, laneY, ticks, 10'000'000);
                static_cast<void>(view.Update({0, 0, 1, input, audio, {}}));
                InputStateTestAccess::Release(input, judgeX, laneY);
                static_cast<void>(view.Update({0, 0, 1, input, audio, {}}));
            };
            press(10'000'000);
            Check(state.TimeMilliseconds() == noteMs + 5, "A single click on a note must not move the cursor.");
            press(10'000'000 + 1'000'000);
            Check(state.TimeMilliseconds() == noteMs, "A double-click on a note must move the cursor to it.");
            state.Seek(noteMs + 5);
            press(10'000'000 + 2'000'000);
            press(10'000'000 + 2'000'000 + 20'000'000);
            Check(state.TimeMilliseconds() == noteMs + 5, "Presses two seconds apart must not count as a double-click.");
            state.SelectNotes({});
            static_cast<void>(state.Mode().CancelInteraction());
            state.Seek(0);
            state.SelectTab(EditorTab::Audio);
            view.Build();
        }
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
            Check(hasText(L"Unsaved changes") && hasText(L"Discard") && hasText(L"Cancel"),
                  "The unsaved alert offers Discard, Cancel and Save.");
            Check(hasText(L"Save"), "The unsaved alert offers Discard, Cancel and Save.");
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
        Check(hasText(L"−") && hasText(L"+") && hasText(L"Spacing"),
              "Realtime view must expose the note spacing buttons and current spacing.");
        Check(hasText(L"50%"), "Realtime view must expose the note spacing buttons and current spacing.");
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
