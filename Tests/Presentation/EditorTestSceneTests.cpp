#include <Windows.h>
#include "YOUMUAssets.h"
#include "EditorScene/EditorScene.h"
#include "EditorScene/Submodules/EditorWorkspace.h"
#include "EditorScene/Submodules/EditorWorkspaceStore.h"
#include "GameFlow/GameplaySessionController.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Mode/PlayGameModeFactory.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>

using namespace PhantomCore;

namespace PhantomCore
{
struct InputStateTestAccess
{
    static void Key(InputState &input, std::uint16_t key)
    {
        input.Reset();
        LARGE_INTEGER ticks{};
        QueryPerformanceCounter(&ticks);
        input.SetKey(key, true, ticks.QuadPart);
    }
    static void Mouse(InputState &input, int x, int y, bool down)
    {
        input.Reset();
        input.SetMousePosition(x, y, true);
        if (!down)
        {
            input.SetMouseButton(MouseButton::Left, true, 0);
            input.BeginUpdate();
        }
        input.SetMouseButton(MouseButton::Left, down, 1);
    }
    static void Undo(InputState &input)
    {
        Key(input, VK_LCONTROL);
        LARGE_INTEGER ticks{}; QueryPerformanceCounter(&ticks);
        input.SetKey('Z', true, ticks.QuadPart);
    }
};
} // namespace PhantomCore

namespace
{
    void Check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
    class Renderer final : public Visual2DRenderSystem
    {
        ImageHandle LoadImageFile(const std::filesystem::path &) override { return {++image_}; }
        Size GetImageSize(ImageHandle) const noexcept override { return {64, 64}; }
        void SubmitScreen(const Visual2DCanvas &, const RenderContext &, Point, std::uint32_t) override {}
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const Float4x4 &, Size,
                         const Float4x4 &) override {}
        RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override { return {}; }
        void RenderToTexture(const Visual2DCanvas &, const RenderTargetTextureHandle &, const RenderContext &) override {}
        std::uint64_t image_{};
    };
    std::size_t clipLoads{};
    std::unique_ptr<IAudioClipBackend> LoadClip(IAudioBackend &backend, const std::filesystem::path &path,
                                             AudioLoadMode mode, std::string &error)
    {
        ++clipLoads;
        return CreateFmodAudioClipBackend(backend, path, mode, error);
    }
    GameplayLaunchRequest MakeFixture()
    {
        const auto root = std::filesystem::absolute("build/fixtures/EditorTestScene");
        std::filesystem::create_directories(root);
        std::ofstream(root / "Song.ymm") << "Version: 1\nFile: Music.wav\nMusic Name 1: Editor Test\n";
        std::ofstream(root / "Pattern.ymp") <<
            "Version: 1\nMusic metadata: Song.ymm\nPattern Name: Editor Test\nMode: Taiko\nBase BPM: 120\n"
            "JudgeLevel: 50\nHP: 9\n[Pattern]\n0/1,1,0\n1/4,11,1\n3/4,11,2\n--\n0/1,2,0\n1/2,1,0\n--\n1/2,2,0\n";
        std::ofstream wav(root / "Music.wav", std::ios::binary);
        const auto word = [&](std::uint32_t value, unsigned bytes)
        { for (unsigned i = 0; i < bytes; ++i) wav.put(static_cast<char>((value >> (8 * i)) & 255)); };
        constexpr unsigned rate = 22050, frames = rate * 6, size = frames * 2;
        wav.write("RIFF", 4); word(36 + size, 4); wav.write("WAVEfmt ", 8); word(16, 4);
        word(1, 2); word(1, 2); word(rate, 4); word(rate * 2, 4); word(2, 2); word(16, 2);
        wav.write("data", 4); word(size, 4);
        for (unsigned i = 0; i < frames; ++i)
            word(static_cast<std::uint16_t>(static_cast<std::int16_t>(8000 * std::sin(i * 440.0 * 6.283185307179586 / rate))), 2);
        GameplayLaunchRequest request{root / "Pattern.ymp", {}, root / "Music.wav", "Taiko"};
        request.songsRoot = root;
        request.playOptions.preserveMusicPitch = request.playOptions.preserveHitSoundPitch = false;
        request.editorTestStart = RhythmTime{1'250'000};
        return request;
    }
    struct Harness
    {
        Renderer renderer;
        ScreenVisual2DManager visuals;
        MeshRenderSystem meshes;
        TextRenderSystem text;
        AudioSystem audio;
        AudioPlaybackManager playback;
        TextCatalog texts{"Client/Texts/Languages"};
        EngineServices services{meshes, text, renderer, audio, 1920, 1080};
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        Harness()
        {
            visuals.Initialize(renderer, {1920, 1080});
            AudioConfig config;
            config.preferredBackend = AudioOutputBackend::NoSound;
            std::string error;
            Check(audio.Initialize(config, CreateFmodAudioBackend, LoadClip, error), error.c_str());
            texts.SetLanguage(Language::English);
        }
        UpdateContext Context(const InputState &input)
        {
            audio.Update(); playback.Update();
            return {.deltaSeconds = .002, .totalSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(),
                    .updateIndex = 1, .input = input, .audio = audio, .performance = {}};
        }
    };
    void TestTransport(Harness &h, GameplayLaunchRequest request)
    {
        auto loaded = CreatePlayGameMode("Taiko", {{}, {}, request.playOptions, request.editorTestStart})
                          ->LoadSession(request.patternPath);
        Check(loaded.Succeeded() && loaded.session->PlayableNoteCount() == 3,
              "The partial Test must omit earlier tap and spanning roll.");
        GameplaySessionController controller(request, h.playback, false, std::move(loaded.session));
        controller.InitializeSession(); controller.Start(h.services, *request.editorTestStart);
        InputState idle;
        const auto wait = [&]
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            while (controller.CurrentState() != PlayState::Gameplay && std::chrono::steady_clock::now() < deadline)
            { controller.Update(h.Context(idle)); std::this_thread::sleep_for(std::chrono::milliseconds{2}); }
            Check(controller.CurrentState() == PlayState::Gameplay, "Actual Test music did not become ready.");
        };
        wait();
        const auto loads = clipLoads;
        controller.SetEditorTestRate(.25, h.Context(idle));
        const auto slowBegin = controller.Time();
        std::this_thread::sleep_for(std::chrono::milliseconds{160});
        controller.Update(h.Context(idle));
        const auto slowDelta = (controller.Time() - slowBegin).count();
        Check(slowDelta >= 30'000 && slowDelta < 85'000 && controller.PlaybackRate() == .25 && clipLoads == loads,
              "Quarter speed must retime the common clock without reloading music.");
        InputState key;
        InputStateTestAccess::Key(key, VK_SPACE); controller.Update(h.Context(key));
        Check(controller.CurrentState() == PlayState::Pause, "Test Space must pause in both configurations.");
        const auto frozen = controller.Time();
        controller.SetEditorTestRate(2, h.Context(idle));
        InputStateTestAccess::Key(key, '4'); controller.Update(h.Context(key));
        std::this_thread::sleep_for(std::chrono::milliseconds{20}); controller.Update(h.Context(idle));
        Check(controller.Time() == frozen && controller.CurrentState() == PlayState::Pause,
              "Changing paused Test rate and Debug seek keys must not move the cursor.");
        InputStateTestAccess::Key(key, VK_SPACE); controller.Update(h.Context(key)); wait();
        const auto fastBegin = controller.Time();
        std::this_thread::sleep_for(std::chrono::milliseconds{80}); controller.Update(h.Context(idle));
        Check((controller.Time() - fastBegin).count() >= 120'000 && controller.PlaybackRate() == 2 && clipLoads == loads,
              "Test resume must preserve chosen double speed and reuse prepared clips.");
        bool rejected = false;
        try { controller.SetEditorTestRate(.2, h.Context(idle)); } catch (const std::invalid_argument &) { rejected = true; }
        Check(rejected && controller.PlaybackRate() == 2, "Out-of-range Test rates must leave the active rate unchanged.");
        controller.Shutdown();
        Check(h.playback.PlaybackCount() == 0, "Test shutdown must stop every owned voice.");
        request.editorTestStart = RhythmTime{5'900'000};
        auto empty = CreatePlayGameMode("Taiko", {{}, {}, request.playOptions, request.editorTestStart})
                         ->LoadSession(request.patternPath);
        Check(empty.Succeeded() && empty.session->PlayableNoteCount() == 0 && !empty.session->CanUseHp(),
              "A Test with no remaining notes is valid and has no HP judgement.");
        GameplaySessionController ending(request, h.playback, false, std::move(empty.session));
        ending.InitializeSession(); ending.Start(h.services, *request.editorTestStart);
        const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (ending.CurrentState() == PlayState::Loading && std::chrono::steady_clock::now() < readyDeadline)
        { ending.Update(h.Context(idle)); std::this_thread::sleep_for(std::chrono::milliseconds{2}); }
        if (ending.CurrentState() != PlayState::Gameplay && !ending.ShouldShowResult())
            std::fprintf(stderr, "Empty Test preparation: state=%d, error=%.*s\n",
                static_cast<int>(ending.CurrentState()), static_cast<int>(ending.ErrorMessage().size()),
                ending.ErrorMessage().data());
        Check(ending.CurrentState() == PlayState::Gameplay || ending.ShouldShowResult(),
              "Empty Test music must become ready before its transport deadline starts.");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
        while (!ending.ShouldShowResult() && std::chrono::steady_clock::now() < deadline)
        { ending.Update(h.Context(idle)); std::this_thread::sleep_for(std::chrono::milliseconds{2}); }
        const bool completed = ending.ShouldShowResult();
        auto ended = completed ? ending.TakeResult() : nullptr;
        if (!completed || ending.Time() < RhythmTime{6'000'000} || !ended || ended->Summary().CanUseHp())
            std::fprintf(stderr, "Empty Test: state=%d, time=%lld, result=%d, HP=%d, error=%.*s\n",
                static_cast<int>(ending.CurrentState()), static_cast<long long>(ending.Time().count()),
                ended != nullptr, ended ? ended->Summary().CanUseHp() : false,
                static_cast<int>(ending.ErrorMessage().size()), ending.ErrorMessage().data());
        Check(completed && ending.Time() >= RhythmTime{6'000'000} && ended &&
                  !ended->Summary().CanUseHp(),
              "Empty Test must finish at the actual song end and remain outside HP qualification.");
        ending.Shutdown();
    }
    Visual2DNode *Find(Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name) return &node;
        for (const auto &child : node.Children()) if (auto *found = Find(*child, name)) return found;
        return nullptr;
    }
    std::pair<Visual2DCanvas *, Visual2DNode *> TryFind(Harness &h, std::string_view name)
    {
        for (std::uint64_t id = 1; id < 128; ++id)
            if (auto *canvas = h.visuals.FindCanvas(id))
                if (auto *node = Find(canvas->Root(), name)) return {canvas, node};
        return {};
    }
    std::pair<Visual2DCanvas *, Visual2DNode *> Find(Harness &h, std::string_view name)
    {
        if (auto found = TryFind(h, name); found.second) return found;
        throw std::runtime_error("Expected Test visual missing: " + std::string(name));
    }
    void TestRoundTrip(Harness &h, GameplayLaunchRequest request)
    {
        auto launches = std::make_shared<GameplayLaunchStore>();
        auto workspaces = std::make_shared<EditorWorkspaceStore>();
        auto workspace = std::make_unique<EditorWorkspace>(request);
        workspace->Initialize(); workspace->Seek(1250); workspace->SetPlaybackRate(.5);
        workspace->AddNote({3, {}}, 1); workspace->Save();
        workspaces->Suspend(std::move(workspace)); launches->Set(request);
        std::unique_ptr<IPlayResult> result;
        SceneManager scenes; scenes.Initialize(h.services);
        Check(scenes.RegisterScene<EditorScene>(std::string(EditorSceneId), SceneRetention::DestroyOnExit,
                  std::ref(h.visuals), launches, std::ref(h.texts), workspaces) &&
              scenes.RegisterScene<RhythmTestScene>(std::string(RhythmTestSceneId), SceneRetention::DestroyOnExit,
                  launches, std::ref(h.playback), std::ref(h.visuals), false, std::ref(h.texts), std::ref(result)),
              "Editor Test routes must register.");
        Check(scenes.Start(EditorSceneId), "Editor Test return state must start.");
        InputState idle;
        const auto tick = [&](const InputState &input)
        {
            Check(scenes.Update(h.Context(input)), "Test route unexpectedly quit.");
            scenes.Render({});
        };
        const auto launch = [&]
        {
            InputState click; InputStateTestAccess::Mouse(click, 1655, 1053, true); tick(click);
            Check(scenes.CurrentSceneId() == RhythmTestSceneId && launches->Snapshot().editorTestStart == RhythmTime{1'250'000} &&
                      launches->Snapshot().playOptions.accelerate == 1, "Test must use current editor time and fresh 1x.");
        };
        const auto escape = [&]
        {
            InputState key; InputStateTestAccess::Key(key, VK_ESCAPE); tick(key);
            Check(scenes.CurrentSceneId() == EditorSceneId && !result, "Esc must restore the editor without any Result transfer.");
        };
        const auto waitForGameplay = [&]
        {
            const auto readyDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
            while (scenes.CurrentSceneId() == RhythmTestSceneId && std::chrono::steady_clock::now() < readyDeadline)
            {
                tick(idle);
                const auto help = TryFind(h, "EditorTest.Help").second;
                if (help && help->GetComponent<TextVisualComponent>()->Text() == h.texts.Get("Editor.testControls")) return;
                std::this_thread::sleep_for(std::chrono::milliseconds{2});
            }
            throw std::runtime_error("Editor Test did not enter Gameplay: " + launches->TakeEditorTestError());
        };
        launch(); escape(); // Include an early return while resources are preparing.
        launch();
        waitForGameplay();
        InputState key; InputStateTestAccess::Key(key, VK_SPACE); tick(key);
        unsigned rateCheck{};
        const auto setRate = [&](float edge)
        {
            std::printf("Test rate lookup %u: Scene=%s, Canvas count=%zu\n", ++rateCheck,
                std::string(scenes.CurrentSceneId()).c_str(), h.visuals.CanvasCount());
            auto [canvas, slider] = Find(h, "EditorTest.Rate");
            const auto bounds = slider->BoundsInCanvas();
            const auto logical = canvas->LogicalSize();
            const auto x = static_cast<int>((bounds.x + 8 + (bounds.width - 16) * edge + logical.width / 2) * canvas->PixelScale());
            const auto y = static_cast<int>((logical.height / 2 - bounds.y - bounds.height / 2) * canvas->PixelScale());
            InputState click; InputStateTestAccess::Mouse(click, x, y, true); tick(click);
            InputStateTestAccess::Mouse(click, x, y, false); tick(click);
            const auto [ignored, label] = Find(h, "EditorTest.RateLabel");
            static_cast<void>(ignored);
            Check(label->GetComponent<TextVisualComponent>()->Text() == h.texts.Format("Editor.playbackRateFormat", edge == 0 ? .25 : 2.0),
                  "The Test slider must reach both editor range endpoints.");
        };
        for (const float edge : {0.0F, 1.0F}) setRate(edge);
        h.texts.SetLanguage(Language::Korean); tick(idle);
        Check(Find(h, "EditorTest.Help").second->GetComponent<TextVisualComponent>()->Text() == h.texts.Get("Editor.testPaused"),
              "Test controls must rebuild their paused help and font after a language change.");
        h.texts.SetLanguage(Language::English);
        escape(); launch(); escape();
        Check(h.playback.PlaybackCount() == 0 && !result, "Repeated Test round trips must not retain voices or results.");
        launch();
        waitForGameplay();
        setRate(1);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{8};
        while (scenes.CurrentSceneId() == RhythmTestSceneId && std::chrono::steady_clock::now() < deadline)
        { tick(idle); std::this_thread::sleep_for(std::chrono::milliseconds{2}); }
        if (scenes.CurrentSceneId() == RhythmTestSceneId)
            for (const auto name : {"Progress.CurrentTime", "Progress.TotalTime", "EditorTest.Help", "EditorTest.RateLabel", "Hud.Error"})
            {
                const auto label = Find(h, name).second->GetComponent<TextVisualComponent>()->Text();
                std::fwprintf(stderr, L"%hs: %ls\n", name, std::wstring(label).c_str());
            }
        Check(scenes.CurrentSceneId() == EditorSceneId && !result,
              "Natural Test completion must return straight to the editor without a Result Scene.");
        std::ofstream(request.songsRoot / "Song.ymm") << "Version: 1\nFile: Missing.wav\nMusic Name 1: Editor Test\n";
        launch();
        const auto failureDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (scenes.CurrentSceneId() == RhythmTestSceneId && std::chrono::steady_clock::now() < failureDeadline)
        { tick(idle); std::this_thread::sleep_for(std::chrono::milliseconds{2}); }
        Check(scenes.CurrentSceneId() == EditorSceneId && !result,
              "Failed Test music preparation must return to the preserved editor rather than leave a stuck play screen.");
        std::ofstream(request.songsRoot / "Song.ymm") << "Version: 1\nFile: Music.wav\nMusic Name 1: Editor Test\n";
        InputStateTestAccess::Undo(key); tick(key);
        Check(Find(h, "Editor.Menu.Caption.0").second->GetComponent<TextVisualComponent>()->Text() == L"Save *",
              "The editor returned from Test must retain its actual Undo history.");
        scenes.Shutdown();
        Check(h.visuals.CanvasCount() == 0, "Test/editor canvases must all be released.");
    }
}
int main()
{
    try
    {
        std::string error; Check(InitializeBuiltInAssets(error), error.c_str());
        auto fixture = MakeFixture(); Harness harness;
        TestTransport(harness, fixture); TestRoundTrip(harness, fixture);
        std::puts("Editor Test transport, native audio and scene round trips passed."); return 0;
    }
    catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
