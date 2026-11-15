#include "App/OptionSettings.h"
#include "Mode/PlayGameModeFactory.h"
#include "Presentation/ModePresentation.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiDropdown.h"
#include "Presentation/UiScrollbar.h"
#include "Presentation/UiTooltip.h"
#include "YOUMUAssets.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>

using namespace PhantomCore;

namespace PhantomCore
{
struct InputStateTestAccess
{
    static void Wheel(InputState &input, float delta) { input.Reset(); input.AddMouseWheel(delta, 1); }
    static void Key(InputState &input, std::uint16_t key) { input.Reset(); input.SetKey(key, true, 1); }
    static void Move(InputState &input) { input.Reset(); input.AddMouseDelta(1, 0, 1); }
    static void Press(InputState &input) { input.Reset(); input.SetMouseButton(MouseButton::Left, true, 1); }
    static void Release(InputState &input) { input.BeginUpdate(); input.SetMouseButton(MouseButton::Left, false, 2); }
};
} // namespace PhantomCore


namespace
{

    void Check(bool condition, const char *message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    // Registry entries are data that the options panel, gameplay audio and the
    // editor read without mode-specific branches, so they must match the domain
    // layouts they describe.
    void TestModePresentationRegistry()
    {
        TextCatalog texts;
        const auto layouts = AvailableInputLayouts();
        std::set<std::string_view> modeIds;
        for (const auto *presentation : ModePresentations())
        {
            if (presentation == nullptr)
                throw std::runtime_error("Registered presentation entries are never null.");
            Check(modeIds.insert(presentation->modeId).second, "Each mode has one presentation entry.");
            Check(FindModePresentation(presentation->modeId) == presentation, "Lookup returns the registered entry.");
            const auto layout = std::ranges::find(layouts, presentation->modeId, &ModeInputLayout::modeId);
            Check(layout != layouts.end(), "A presentation entry must describe a registered play mode.");
            for (const auto &caption : presentation->bindingCaptions)
                Check(std::ranges::find(layout->bindings, caption.id, &ModeInputBinding::id) != layout->bindings.end() &&
                          !texts.Get(Language::English, caption.textKey).empty(),
                      "Binding captions must name a layout binding and an existing text key.");
            std::set<std::string_view> soundIds;
            for (const auto &sound : presentation->hitSounds)
            {
                Check(!sound.soundIds.empty() && !sound.file.empty() && !presentation->hitSoundFolder.empty(),
                      "Hit sound entries need a skin folder, a file and at least one sound id.");
                for (const char *id : sound.soundIds)
                    Check(id && *id && soundIds.insert(id).second, "A sound id is registered by one file only.");
            }
            if (const auto *overlay = presentation->keyOverlay)
            {
                Check(overlay->faces.size() == layout->bindings.size(), "A key overlay has one face per binding.");
                const bool positiveFaces = std::ranges::all_of(overlay->faces, [](const ModeKeyOverlayFace &face) {
                    return face.sourceBounds.width > 0 && face.sourceBounds.height > 0;
                });
                Check(!overlay->nodePrefix.empty() && positiveFaces &&
                          !texts.Get(Language::English, overlay->titleTextKey).empty(),
                      "A key overlay needs node names, a title text and sized key faces.");
            }
        }
        Check(FindModePresentation("Unknown") == nullptr && FindModePresentation("") == nullptr,
              "Unregistered modes use the default inline key rows without built-in hit sounds.");
    }

    class TestBackend final : public IAudioBackend
    {
      public:
        bool rejectAsio{};
        AudioOutputBackend requested{AudioOutputBackend::Automatic};
        int driver{};
        int lastSetDriver{-1};
        std::vector<AudioDeviceInfo> devices;
        void SetDevices()
        {
            if (requested == AudioOutputBackend::Asio)
                devices = {{requested, 5, "ASIO device", 48000, 2}};
            else devices = {{AudioOutputBackend::Wasapi, 0, "Speakers", 48000, 2},
                            {AudioOutputBackend::Wasapi, 2, "Headphones", 48000, 2}};
            driver = devices.front().driverIndex;
        }
        bool Initialize(const AudioConfig &config, std::string &error) override
        {
            requested = config.preferredBackend;
            SetDevices();
            error.clear();
            return true;
        }
        void Update() override {}
        void Shutdown() noexcept override {}
        std::string_view Name() const noexcept override { return "Test"; }
        AudioOutputBackend RequestedOutput() const noexcept override { return requested; }
        AudioOutputBackend ActiveOutput() const noexcept override
        {
            return requested == AudioOutputBackend::Automatic ? AudioOutputBackend::Wasapi : requested;
        }
        bool SetOutputBackend(AudioOutputBackend value, std::string &error) override
        {
            if (value == AudioOutputBackend::Asio && rejectAsio)
            {
                error = "ASIO device unavailable";
                return false;
            }
            if (value != requested) { requested = value; SetDevices(); }
            error.clear();
            return true;
        }
        int DriverCount() const noexcept override { return 6; }
        const std::vector<AudioDeviceInfo> &OutputDrivers() const noexcept override { return devices; }
        int ActiveDriverIndex() const noexcept override { return driver; }
        bool SetOutputDriver(int index, std::string &error) override
        {
            for (const auto &entry : devices)
            {
                if (entry.driverIndex != index) continue;
                lastSetDriver = driver = index;
                error.clear();
                return true;
            }
            error = "Missing device";
            return false;
        }
        int RequestedSampleRate() const noexcept override { return 0; }
        int SampleRate() const noexcept override { return 48000; }
        std::uint32_t DspBufferLength() const noexcept override { return 256; }
        int DspBufferCount() const noexcept override { return 4; }
        double EstimatedDspLatencyMilliseconds() const noexcept override { return 0; }
        bool SetSampleRate(int, std::string &) override { return false; }
        bool SetDspBufferSize(std::uint32_t, int, std::string &) override { return false; }
        std::uint64_t DspClock() const noexcept override { return 0; }
        std::unique_ptr<IAudioBusBackend> CreateBus(std::string_view,
            IAudioBusBackend *, std::string &) override { return {}; }
    };
    TestBackend *backend{};
    int backendCreations{};
    std::unique_ptr<IAudioBackend> CreateBackend()
    {
        auto value = std::make_unique<TestBackend>();
        backend = value.get();
        ++backendCreations;
        return value;
    }
    class TestClip final : public IAudioClipBackend
    {
        bool GetDurationMicroseconds(std::uint64_t& duration, std::string&) const override
        { duration = 0; return true; }
        std::unique_ptr<IAudioVoiceBackend> Play(const AudioPlaybackSettings &,
            IAudioBusBackend *, std::string &) override { return {}; }
    };
    std::unique_ptr<IAudioClipBackend> CreateClip(IAudioBackend &,
        const std::filesystem::path &, AudioLoadMode, std::string &) { return std::make_unique<TestClip>(); }

    Visual2DNode &FindNode(Visual2DNode &root, std::string_view name)
    {
        if (root.Name() == name) return root;
        for (const auto &child : root.Children())
        {
            try { return FindNode(*child, name); }
            catch (const std::out_of_range &) {}
        }
        throw std::out_of_range("Node not found");
    }
    UiDropdownComponent &Combo(Visual2DCanvas &canvas, std::string_view name)
    {
        return RequireDropdown(FindNode(canvas.Root(), name));
    }
    Action Select(Visual2DCanvas &canvas, std::string_view name, std::size_t index)
    {
        Action action;
        action.type = ActionType::SelectionChanged;
        action.source = FindNode(canvas.Root(), name).Id();
        action.selectedIndex = index;
        return action;
    }
    // A rail click scrolls the continuous body to that section.
    void Section(OptionsPanel &panel, Visual2DCanvas &canvas, std::string_view name)
    {
        Action action;
        action.type = ActionType::Clicked;
        action.source = FindNode(canvas.Root(), std::string("Options.Rail.") + std::string(name)).Id();
        Check(panel.ProcessAction(action), "An options rail click must be consumed.");
    }
    bool Lit(Visual2DCanvas &canvas, std::string_view name)
    {
        return FindNode(FindNode(canvas.Root(), std::string("Options.Rail.") + std::string(name)), "Marker").IsVisible();
    }
    float Top(Visual2DNode &node)
    {
        const auto bounds = node.BoundsInCanvas();
        return bounds.y + bounds.height;
    }

    void TestPanelAppearanceAndClosing(OptionsPanel &panel, Visual2DCanvas &canvas,
                                       Visual2DInputRouter &router)
    {
        const auto luminance = [](Color c) { return .2126F * c.red + .7152F * c.green + .0722F * c.blue; };
        const auto background =
            FindNode(canvas.Root(), "Options.Panel").GetComponent<SpriteVisualComponent>()->Style().normal;
        const auto rail = FindNode(canvas.Root(), "Options.Rail").GetComponent<SpriteVisualComponent>()->Style().normal;
        Check(background.alpha == 1 && rail.alpha == 1 && luminance(rail) + .5F < luminance(background),
              "The opaque drawer is edged by its navy category rail.");
        const auto field = FindNode(canvas.Root(), "Options.Output.Combo").BoundsInCanvas();
        const auto line = FindNode(canvas.Root(), "Options.Output.Line").BoundsInCanvas();
        Check(line.height == 2 && line.y == field.y && line.width == field.width,
              "Fields carry the redesign's two-pixel underline.");
        const auto drawer = FindNode(canvas.Root(), "Options.Panel").BoundsInCanvas();
        Check(drawer.width == 432 && drawer.height == 720 &&
                  FindNode(canvas.Root(), "Options.Rail").BoundsInCanvas().width == 64,
              "The drawer is 432 wide with a 64px rail.");
        const auto b = FindNode(canvas.Root(), "Options.Output.Combo").BoundsInCanvas();
        PointerInput pointer;
        pointer.position = {b.x + b.width / 2, b.y + b.height / 2};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        router.Process(canvas, pointer);
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        router.Process(canvas, pointer);
        static_cast<void>(canvas.TakeActions());
        Check(Combo(canvas, "Options.Output.Combo").IsExpanded(), "The close test must begin with a visible popup.");
        Check(panel.HandleKey(VK_ESCAPE, router) && !Combo(canvas, "Options.Output.Combo").IsExpanded(),
              "ESC must consume the command and collapse popups without requiring a pointer inside the panel.");
        Check(!panel.ProcessAction(Select(canvas, "Options.Language.Combo", 1)),
              "Pending selections must be ignored during panel closing.");
        panel.Update(.05);
        Check(panel.IsVisible() && panel.HandleKey(VK_ESCAPE, router), "Repeated ESC during closing must be consumed.");
        panel.Update(.2);
        Check(!panel.IsVisible() && !panel.HandleKey(VK_ESCAPE, router),
              "A closed panel must release ESC to its host.");
        panel.Toggle(); // Opening and immediately closing also consumes the command.
        Check(panel.HandleKey(VK_ESCAPE, router), "ESC at zero animation progress must still be consumed.");
        panel.Update(.2);
        Check(!panel.IsVisible(), "Closing at zero progress must not reopen the panel.");
        panel.Toggle();
        panel.Update(.2);
        Check(panel.IsVisible() && !Combo(canvas, "Options.Output.Combo").IsExpanded(),
              "Reopening must begin with collapsed fields.");
    }

    void TestInvalidOptionFallback(const std::filesystem::path &folder)
    {
        const auto path = folder / L"설정 오류.ini";
        OptionSettings settings(path);
        std::string error;
        for (const std::string content : {"[General]\nLanguage=zz\n", "missing separator\n",
                                          "[Audio]\nMiddleware=uninstalled\n"})
        {
            auto previous = settings.Values();
            previous.driverIndex = 7;
            Check(settings.Save(previous, error), "Could not prepare valid option values.");
            { std::ofstream file(path); file << content; }
            Check(!settings.Load(error) && !settings.LoadWarning().empty() && settings.Values().driverIndex == -1 &&
                      settings.Values().audioMiddleware == "FMOD",
                  "Failed reloads must publish defaults and retain a visible warning.");
            std::ifstream file(path);
            const std::string preserved{std::istreambuf_iterator<char>(file), {}};
            Check(preserved == content, "Invalid option files must remain available for user repair.");
        }
        Check(settings.Save(settings.Values(), error) && settings.LoadWarning().empty(),
              "A successful user save must clear the startup warning.");
        OptionSettings unavailable(folder / "missing-directory" / "Option.ini");
        Check(!unavailable.Load(error) && unavailable.Values().driverIndex == -1 && !unavailable.LoadWarning().empty(),
              "Failure creating a first-run INI must still provide defaults.");
    }

    void TestOptions(const std::filesystem::path &folder)
    {
        std::string error;
        OptionSettings settings(folder / "Option.ini");
        Check(settings.Load(error), error.c_str());
        Check(std::filesystem::exists(settings.Path()), "First load must create Option.ini.");
        const auto systemLanguage = PRIMARYLANGID(GetUserDefaultUILanguage());
        const auto expectedLanguage = systemLanguage == LANG_KOREAN ? Language::Korean :
                                      systemLanguage == LANG_JAPANESE ? Language::Japanese : Language::English;
        Check(settings.Values().language == expectedLanguage,
              "First-run explanation language must follow supported Windows languages, otherwise English.");
        Check(settings.Values().skinSet == L"Default Skin" && settings.Values().driverIndex == -1 &&
                  settings.Values().audioOutput == AudioOutputBackend::Automatic,
              "First-run settings must use only the current defaults.");
        auto values = settings.Values();
        values.skinSet = L"한국어 Skin";
        values.language = Language::English;
        values.audioOutput = AudioOutputBackend::Wasapi;
        values.driverIndex = 2;
        values.driverName = "Headphones";
        values.songsPath = folder / L"한국어 Songs";
        Check(settings.Save(values, error), error.c_str());
        OptionSettings restored(settings.Path());
        Check(restored.Load(error), error.c_str());
        Check(restored.Values().songsPath == values.songsPath && restored.SongsRoot() == values.songsPath,
              "Unicode absolute Songs paths must persist in UTF-8 Option.ini.");
        auto relative = restored.Values();
        relative.songsPath = L"custom/Songs";
        Check(
            restored.Save(relative, error) &&
                restored.SongsRoot() ==
                    (OptionSettings::NextToExecutable().parent_path() / L"custom/Songs").lexically_normal(),
            "Relative Songs paths must use the executable directory, not the current working directory.");
        Check(restored.Save(values, error), error.c_str());
        Check(restored.Values().skinSet == values.skinSet && restored.Values().driverIndex == 2 &&
              restored.Values().language == values.language && restored.Values().driverName == values.driverName,
              "Unicode skin, language and audio settings must survive another instance.");
        {
            std::ofstream file(folder / "invalid.ini");
            file << "[Audio]\nDriverIndex=-5\n";
        }
        OptionSettings invalid(folder / "invalid.ini");
        Check(!invalid.Load(error) && invalid.Values().driverIndex == -1,
              "Malformed settings must not publish a partial candidate.");
        values.driverName = "bad\nentry";
        Check(!restored.Save(values, error) && restored.Values().driverName == "Headphones",
              "INI line injection must be rejected without changing values.");

        // The panel uses the existing audio system and real driver indices, not row indices.
        values = restored.Values();
        values.skinSet = L"Default Skin";
        values.driverIndex = 2;
        Check(settings.Save(values, error), error.c_str());
        TextCatalog texts;
        texts.SetLanguage(values.language);
        AudioSystem audio;
        AudioConfig config;
        config.preferredBackend = values.audioOutput;
        Check(audio.Initialize(config, &CreateBackend, &CreateClip, error), error.c_str());
        auto clip = audio.LoadSound("test.wav", error);
        Check(clip != nullptr, "Test clip must load.");

        Visual2DCanvas canvas;
        OptionsPanel panel(texts, settings);
        panel.Initialize(canvas, audio);
        panel.Toggle();
        panel.Update(0.2);
        Check(panel.IsVisible(), "The panel must finish its slide animation.");
        { std::ofstream file(settings.Path()); file << "[General]\nLanguage=zz\n"; }
        Check(!settings.Load(error), "Invalid language must trigger startup fallback.");
        panel.Update(0);
        Check(FindNode(canvas.Root(), "Options.Status").GetComponent<TextVisualComponent>()->Text().find(L"Using defaults") != std::wstring::npos,
              "The panel must expose the retained startup warning.");
        Check(settings.Save(values, error), "Could not restore the option test fixture.");
        panel.Update(0);
        Section(panel, canvas, "Audio");
        Check(Combo(canvas, "Options.Middleware.Combo").Items() == std::vector<std::wstring>{L"FMOD"},
              "Only installed middleware may appear.");
        Check(panel.ProcessAction(Select(canvas, "Options.Driver.Combo", 0)) && backend->lastSetDriver == 0,
              "Driver selection must apply immediately.");
        Check(panel.ProcessAction(Select(canvas, "Options.Driver.Combo", 1)) && backend->lastSetDriver == 2,
              "Sparse driver numbers must use the device's index rather than ComboBox row.");
        backend->rejectAsio = true;
        Check(panel.ProcessAction(Select(canvas, "Options.Output.Combo", 2)) && audio.ActiveDriverIndex() == 2 &&
              settings.Values().audioOutput == AudioOutputBackend::Wasapi,
              "Rejected output must preserve the previous selection and persisted preferences.");
        backend->rejectAsio = false;
        // Scroll-position geometry: the ASIO popup row overlaps the driver field.
        // Exercise the real hit router so the lower field cannot steal this click.
        Visual2DInputRouter router;
        auto click = [&](Point position) {
            PointerInput input;
            input.position = position;
            input.leftButtonDown = input.leftButtonPressed = true;
            router.Process(canvas, input);
            input.leftButtonDown = input.leftButtonPressed = false;
            input.leftButtonReleased = true;
            router.Process(canvas, input);
            return canvas.TakeActions();
        };
        const auto bounds = FindNode(canvas.Root(), "Options.Output.Combo").BoundsInCanvas();
        static_cast<void>(click({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}));
        Check(Combo(canvas, "Options.Output.Combo").IsExpanded(), "Output list must open through pointer input.");
        const auto actions = click({bounds.x + bounds.width / 2, bounds.y - 2.5F * 34.0F});
        Check(actions.size() == 1 && actions.front().source == FindNode(canvas.Root(), "Options.Output.Combo").Id() &&
              actions.front().selectedIndex == 2 && panel.ProcessAction(actions.front()),
              "The ASIO popup row must receive clicks ahead of the driver field below it.");
        Check(audio.ActiveOutput() == AudioOutputBackend::Asio && backendCreations == 1 && clip->IsValid(),
              "Output switching must reuse the system and retain loaded clips without restart.");
        Check(Combo(canvas, "Options.Driver.Combo").Items() == std::vector<std::wstring>{L"5: ASIO device"},
              "Drivers must refresh from the selected output API only.");
        Check(settings.Values().driverIndex == 5 && settings.Values().driverName == "ASIO device",
              "Successful switching must persist the new output's actual driver.");

        Check(SetFileAttributesW(settings.Path().c_str(), FILE_ATTRIBUTE_READONLY) != FALSE,
              "Test file must be made read-only.");
        Check(panel.ProcessAction(Select(canvas, "Options.Output.Combo", 1)) &&
              audio.ActiveOutput() == AudioOutputBackend::Asio && audio.ActiveDriverIndex() == 5 &&
              settings.Values().audioOutput == AudioOutputBackend::Asio,
              "Saving failure must restore runtime audio and preserve persisted values.");
        Section(panel, canvas, "General");
        Check(panel.ProcessAction(Select(canvas, "Options.Language.Combo", 0)) &&
              texts.CurrentLanguage() == Language::English,
              "Saving failure must preserve the current language.");
        Check(SetFileAttributesW(settings.Path().c_str(), FILE_ATTRIBUTE_NORMAL) != FALSE,
              "Test file attributes must be restored.");
        Check(panel.ProcessAction(Select(canvas, "Options.Language.Combo", 0)) &&
              texts.CurrentLanguage() == Language::Korean,
              "Language must apply after a successful save.");
        OptionSettings nextRun(settings.Path());
        Check(nextRun.Load(error) && nextRun.Values().language == Language::Korean &&
              nextRun.Values().audioOutput == AudioOutputBackend::Asio && nextRun.Values().driverIndex == 5,
              "The next run must read the most recent successful settings.");
        backend->devices.clear();
        backend->driver = -1;
        panel.RefreshTexts();
        Check(!FindNode(canvas.Root(), "Options.Driver.Combo").IsEnabled() &&
              Combo(canvas, "Options.Driver.Combo").Items().size() == 1,
              "An empty device list must be explicit and non-selectable.");
        Section(panel, canvas, "Audio");
        TestPanelAppearanceAndClosing(panel, canvas, router);
        panel.Shutdown();
        Check(!panel.ProcessAction(actions.front()), "A closed panel must reject stale Canvas actions.");
        clip.reset();
    }

    class TestWindow final : public IWindowControl
    {
      public:
        WindowResolution size{1280, 720};
        bool reject{};
        WindowResolution ClientSize() const noexcept override
        {
            return size;
        }
        std::vector<WindowResolution> DisplayResolutions() const override
        {
            return {{1280, 720}, {960, 540}};
        }
        bool SetClientSize(WindowResolution value, std::string &error) override
        {
            if (reject)
            {
                error = "Injected resize failure";
                return false;
            }
            size = value;
            error.clear();
            return true;
        }
    };
    void TestExtraOptions(const std::filesystem::path &folder)
    {
        OptionSettings settings(folder / "Extra.ini");
        std::string error;
        Check(settings.Load(error), "Create extra options fixture.");
        AudioSystem audio;
        Check(audio.Initialize({}, &CreateBackend, &CreateClip, error), "Initialize extra option audio fixture.");
        TextCatalog texts;
        Visual2DCanvas canvas;
        TestWindow window;
        OptionsPanel panel(texts, settings);
        panel.Initialize(canvas, audio, &window);
        panel.Toggle();
        panel.Update(.2);
        Visual2DInputRouter router;
        const auto click = [&](std::string_view name)
        {
            Action action;
            action.source = FindNode(canvas.Root(), name).Id();
            action.type = ActionType::Clicked;
            Check(panel.ProcessAction(action), "Option field click is consumed.");
        };
        Section(panel, canvas, "Display");
        Check(panel.ProcessAction(Select(canvas, "Options.Resolution.Combo", 1)) && window.size.width == 960 &&
                  settings.Values().windowHeight == 540,
              "Resolution applies and persists immediately.");
        SetFileAttributesW(settings.Path().c_str(), FILE_ATTRIBUTE_READONLY);
        Check(panel.ProcessAction(Select(canvas, "Options.Resolution.Combo", 0)) && window.size.width == 960 &&
                  settings.Values().windowWidth == 960,
              "INI write failure rolls the runtime window back.");
        SetFileAttributesW(settings.Path().c_str(), FILE_ATTRIBUTE_NORMAL);
        window.reject = true;
        Check(panel.ProcessAction(Select(canvas, "Options.Resolution.Combo", 0)) &&
                  settings.Values().windowWidth == 960,
              "Native resize failure preserves saved size.");
        window.reject = false;
        Section(panel, canvas, "Audio");
        click("Options.Offset.Field");
        Check(panel.OwnsKeyboard(), "Offset field captures numeric input.");
        for (const auto key : {'1', '0', '0'})
            Check(panel.HandleKey(static_cast<std::uint16_t>(key), router), "Capture offset digit.");
        panel.HandleKey(VK_RETURN, router);
        Check(settings.Values().globalOffsetMs == 100 && !panel.OwnsKeyboard(),
              "Enter publishes integer milliseconds.");
        click("Options.Offset.Field");
        panel.HandleKey(VK_SUBTRACT, router);
        panel.HandleKey('5', router);
        panel.HandleKey(VK_RETURN, router);
        Check(settings.Values().globalOffsetMs == -5, "Negative offsets persist.");
        click("Options.Offset.Field");
        panel.HandleKey(VK_DELETE, router);
        panel.HandleKey(VK_RETURN, router);
        Check(settings.Values().globalOffsetMs == -5 && panel.OwnsKeyboard(), "Invalid draft preserves saved offset.");
        Section(panel, canvas, "Input");
        Check(!panel.OwnsKeyboard() && settings.Values().globalOffsetMs == -5,
              "Switching away from an invalid offset draft cancels only its edit state.");
        click("Options.Taiko.OpenKeys");
        canvas.Update(0);
        {
            const auto bounds = FindNode(canvas.Root(), "Options.Key.Taiko.LeftKat.0").BoundsInCanvas();
            PointerInput pointer{};
            pointer.available = true;
            pointer.position = {bounds.x + 10.0F, bounds.y + bounds.height / 2};
            pointer.leftButtonDown = pointer.leftButtonPressed = true;
            router.Process(canvas, pointer);
            Check(router.HoveredNode() == FindNode(canvas.Root(), "Options.Key.Taiko.LeftKat.0").Id(),
                  "The key modal receives pointer input above its scrim and the options drawer.");
            pointer.leftButtonDown = pointer.leftButtonPressed = false;
            pointer.leftButtonReleased = true;
            router.Process(canvas, pointer);
            static_cast<void>(canvas.TakeActions());
            router.Reset(canvas);
        }
        click("Options.Key.Taiko.LeftDon.0");
        panel.HandleKey('K', router);
        Check(panel.OwnsKeyboard() && !settings.Values().modeKeys.contains("Taiko"),
              "Duplicate key rejected against untouched defaults.");
        panel.HandleKey(VK_SPACE, router);
        Check(panel.OwnsKeyboard(), "Reserved game control is rejected.");
        panel.HandleKey('S', router);
        Check(settings.Values().modeKeys.at("Taiko").at("LeftDon")[0] == 'S' && !panel.OwnsKeyboard(),
              "Primary key saved.");
        click("Options.Key.Taiko.LeftDon.1");
        panel.HandleKey('A', router);
        Check(settings.Values().modeKeys.at("Taiko").at("LeftDon")[1] == 'A', "Auxiliary key saved independently.");
        click("Options.Key.Taiko.LeftDon.3");
        panel.HandleKey('G', router);
        Check(settings.Values().modeKeys.at("Taiko").at("LeftDon")[3] == 'G', "Third auxiliary slot saved.");
        auto &clear = FindNode(canvas.Root(), "Options.Key.Taiko.LeftDon.3.Clear");
        Check(!clear.IsVisible(), "The clear button stays hidden until its slot is pointed at.");
        {
            const auto slot = FindNode(canvas.Root(), "Options.Key.Taiko.LeftDon.3").BoundsInCanvas();
            PointerInput hover{};
            hover.available = true;
            hover.position = {slot.x + 8.0F, slot.y + slot.height / 2};
            router.Process(canvas, hover);
            static_cast<void>(panel.ProcessInput(InputState{}, hover.position, router.HoveredNode(), router));
        }
        Check(clear.IsVisible(), "Pointing at an assigned auxiliary slot shows its clear button.");
        click("Options.Key.Taiko.LeftDon.3.Clear");
        Check(settings.Values().modeKeys.at("Taiko").at("LeftDon")[3] == 0, "Auxiliary slot can be cleared independently.");
        Check(FindNode(canvas.Root(), "Options.Key.Taiko.LeftDon.3").GetComponent<TextVisualComponent>()->Text() == L"+",
              "An unassigned auxiliary slot shows only a faint plus.");
        click("Options.Key.Taiko.RightDon.0");
        panel.HandleKey(VK_ESCAPE, router);
        panel.HandleKey(VK_ESCAPE, router);
        panel.HandleKey(VK_ESCAPE, router);
        panel.Update(.2);
        Check(!panel.IsVisible() && !panel.OwnsKeyboard() &&
                  !settings.Values().modeKeys.at("Taiko").contains("RightDon"),
              "ESC closes panel and cancels key capture without applying Escape as a game key.");
        auto candidate = settings.Values();
        candidate.modeKeys["FutureMode"]["Lane1"] = {'L'};
        Check(settings.Save(candidate, error),
              "Other mode settings can be preserved before loading its implementation.");
        OptionSettings restored(settings.Path());
        Check(restored.Load(error) && restored.Values().globalOffsetMs == -5 && restored.Values().windowWidth == 960 &&
                  restored.Values().modeKeys == candidate.modeKeys,
              "Resolution, offset and all mode profiles survive restart.");
        candidate.windowWidth = 0;
        Check(!settings.Save(candidate, error), "Invalid saved resolution rejected.");
        candidate = settings.Values();
        candidate.modeKeys["Bad]\nsection"]["Lane1"] = {'L'};
        Check(!settings.Save(candidate, error), "Mode names cannot inject INI sections.");
        panel.Shutdown();
    }

    void TestActualAudio()
    {
        AudioSystem audio;
        AudioConfig config;
        config.allowNoSoundFallback = false;
        std::string error;
        Check(audio.Initialize(config, nullptr, nullptr, error), error.c_str());
        auto clip = audio.LoadSound(std::filesystem::path(L"YOUMU.Assets/Assets/Skins/Default Skin/HitSounds/TaikoMode/pop.wav"), error);
        Check(clip != nullptr, error.c_str());
        Check(audio.SetOutputBackend(AudioOutputBackend::Wasapi, error), error.c_str());
        Check(audio.ActiveOutput() == AudioOutputBackend::Wasapi && !audio.OutputDrivers().empty(),
              "Actual FMOD must report WASAPI and its current drivers after switching.");
        Check(audio.SetOutputDriver(audio.ActiveDriverIndex(), error), error.c_str());
        Check(clip->IsValid() && clip->LoadState(error) == AudioClipLoadState::Ready,
              "A loaded native clip must remain usable after output/driver selection.");
        std::cout << "Actual FMOD Automatic -> WASAPI and driver selection passed (loaded clip retained).\n";
    }
    void TestSectionsAndJapanese(const std::filesystem::path &folder)
    {
        OptionSettings settings(folder / "Sections.ini");
        std::string error;
        Check(settings.Load(error), "Create section settings fixture.");
        AudioSystem audio;
        Check(audio.Initialize({}, &CreateBackend, &CreateClip, error), "Initialize section audio fixture.");
        TextCatalog texts("Client/Texts/Languages");
        Visual2DCanvas canvas;
        Visual2DInputRouter router;
        OptionsPanel panel(texts, settings);
        panel.Initialize(canvas, audio);
        panel.Toggle(); panel.Update(.2);
        auto &viewport = FindNode(canvas.Root(), "Options.Viewport");
        auto &general = FindNode(canvas.Root(), "Options.Section.General");
        auto &audioHeader = FindNode(canvas.Root(), "Options.Section.Audio");
        Check(general.IsVisible() && audioHeader.IsVisible() && Lit(canvas, "General") && !Lit(canvas, "Audio"),
              "All sections share one body and the first section starts lit.");
        Check(std::abs(Top(viewport) - Top(general) - 22) < .01F, "The first section starts at the top of the body.");

        // A rail click brings its section to the top and lights it until the body is scrolled by hand.
        Section(panel, canvas, "Audio");
        Check(Lit(canvas, "Audio") && !Lit(canvas, "General") && std::abs(Top(viewport) - Top(audioHeader) - 22) < .01F,
              "A rail click scrolls its section to the top of the body.");
        Section(panel, canvas, "Input");
        Check(Lit(canvas, "Input"), "The last section stays lit even though the body cannot scroll it to the top.");
        InputState wheel;
        InputStateTestAccess::Wheel(wheel, 1);
        static_cast<void>(panel.ProcessInput(wheel, Point{-500, 0}, {}, router));
        static_cast<void>(panel.ProcessInput(wheel, Point{-500, 0}, {}, router));
        Check(Lit(canvas, "Audio") && !Lit(canvas, "Input"), "Scrolling by hand lights the section hanging at the top.");
        InputStateTestAccess::Wheel(wheel, 10);
        for (int i = 0; i < 10; ++i)
            static_cast<void>(panel.ProcessInput(wheel, Point{-500, 0}, {}, router));
        Check(Lit(canvas, "General") && std::abs(Top(viewport) - Top(general) - 22) < .01F,
              "Scrolling back to the top lights the first section.");
        InputStateTestAccess::Wheel(wheel, -100);
        static_cast<void>(panel.ProcessInput(wheel, Point{-500, 0}, {}, router));
        Check(Lit(canvas, "Input"), "At the very bottom the last section is lit.");

        // Up/Down step through the rail and expand its names; a pointer move folds them again.
        auto &rail = FindNode(canvas.Root(), "Options.Rail");
        InputState keys;
        InputStateTestAccess::Key(keys, VK_UP);
        Check(panel.ProcessInput(keys, Point{0, 0}, {}, router) && Lit(canvas, "Audio"), "Up moves to the previous section.");
        panel.Update(.016);
        Check(rail.BoundsInCanvas().width == 184 &&
                  FindNode(FindNode(canvas.Root(), "Options.Rail.Audio"), "Label").IsVisible(),
              "Keyboard navigation expands the rail with section names.");
        InputState move;
        InputStateTestAccess::Move(move);
        static_cast<void>(panel.ProcessInput(move, Point{0, 0}, {}, router));
        panel.Update(.016);
        Check(rail.BoundsInCanvas().width == 64, "A pointer move folds the keyboard expansion.");
        const auto railBounds = rail.BoundsInCanvas();
        const Point onRail{railBounds.x + 20, railBounds.y + 300};
        static_cast<void>(panel.ProcessInput(move, onRail, {}, router));
        panel.Update(.2);
        Check(rail.BoundsInCanvas().width == 64, "The rail waits before expanding under the pointer.");
        panel.Update(.15);
        Check(rail.BoundsInCanvas().width == 184, "Resting on the rail expands it over the body.");
        static_cast<void>(panel.ProcessInput(move, Point{railBounds.x + 300, railBounds.y + 300}, {}, router));
        panel.Update(.016);
        Check(rail.BoundsInCanvas().width == 64, "Leaving the expanded rail folds it at once.");

        // Errors stay in the footer outside the scrolling body; reopening keeps the place.
        backend->rejectAsio = true;
        Check(panel.ProcessAction(Select(canvas, "Options.Output.Combo", 2)), "Produce a recoverable audio error.");
        auto &footer = FindNode(canvas.Root(), "Options.Footer");
        Check(footer.IsVisible() && Top(footer) <= viewport.BoundsInCanvas().y + .01F,
              "An error remains visible below the scrolling body.");
        Section(panel, canvas, "Display");
        const auto shown = Top(FindNode(canvas.Root(), "Options.Section.Display"));
        panel.Toggle();
        panel.Update(.2);
        panel.Toggle();
        panel.Update(.2);
        Check(Lit(canvas, "Display") && Top(FindNode(canvas.Root(), "Options.Section.Display")) == shown,
              "Reopening keeps the scroll position and the lit section.");
        Section(panel, canvas, "General");
        Check(panel.ProcessAction(Select(canvas, "Options.Language.Combo", 2)) && texts.CurrentLanguage() == Language::Japanese,
              "Japanese explanations can be selected and persisted.");
        Check(panel.ProcessAction(Select(canvas, "Options.Output.Combo", 2)), "Produce the audio error in Japanese.");
        backend->rejectAsio = false;
        const auto status = FindNode(canvas.Root(), "Options.Status").GetComponent<TextVisualComponent>()->Text();
        Check(FindNode(canvas.Root(), "Options.Title").GetComponent<TextVisualComponent>()->Text() == L"Options" &&
                  FindNode(FindNode(canvas.Root(), "Options.Rail.Display"), "Label")
                          .GetComponent<TextVisualComponent>()->Text() == L"Display" &&
                  status.find(texts.Get(Language::Japanese, "Options.audioError")) != std::wstring_view::npos,
              "Japanese changes explanations while captions and section names remain English.");
        OptionSettings restored(settings.Path());
        Check(restored.Load(error) && restored.Values().language == Language::Japanese,
              "The ja setting survives restart with the existing INI field.");
        // Rail buttons keep their expanded width; only the icon column shows while the rail is folded.
        const auto click = [&](std::string_view name, const bool railButton = false)
        {
            const auto b = FindNode(canvas.Root(), name).BoundsInCanvas();
            PointerInput pointer;
            pointer.available = true;
            pointer.position = {railButton ? b.x + 32 : b.x + b.width / 2, b.y + b.height / 2};
            pointer.leftButtonDown = pointer.leftButtonPressed = true;
            router.Process(canvas, pointer);
            pointer.leftButtonDown = pointer.leftButtonPressed = false;
            pointer.leftButtonReleased = true;
            router.Process(canvas, pointer);
            for (const auto &action : canvas.TakeActions()) static_cast<void>(panel.ProcessAction(action));
        };
        click("Options.Language.Combo");
        Check(Combo(canvas, "Options.Language.Combo").IsExpanded(), "Open the actual General popup.");
        click("Options.Rail.Display", true);
        Check(!Combo(canvas, "Options.Language.Combo").IsExpanded() && !Lit(canvas, "Display"),
              "A press outside an open list only closes it; the control underneath does not act.");
        click("Options.Rail.Display", true);
        Check(Lit(canvas, "Display"), "With the list closed the rail click moves to its section.");
        static_cast<void>(panel.ProcessInput(InputState{}, std::nullopt, {}, router));
        const auto skin = FindNode(canvas.Root(), "Options.Skin.Combo").BoundsInCanvas();
        PointerInput hover;
        hover.available = true;
        hover.position = {skin.x + skin.width / 2, skin.y + skin.height / 2};
        router.Process(canvas, hover);
        Check(router.HoveredNode() == FindNode(canvas.Root(), "Options.Skin.Combo").Id(),
              "The next hit test must target the field now under the pointer.");
        panel.Shutdown();
    }

    // The shared tooltip: delay, ancestor lookup, warm switching, dismissal and placement.
    void TestTooltip()
    {
        Visual2DCanvas canvas;
        UiTooltip tooltip;
        tooltip.Initialize(canvas);
        auto &button = CreateButton(canvas.Root(), {-100, -20, 80, 40}, L"", "Target");
        auto &child = CreateSprite(button, {0, 0, 10, 10}, {}, "Child");
        auto &other = CreateButton(canvas.Root(), {100, -20, 80, 40}, L"", "Other");
        tooltip.Set(button, {L"Options", L"Ctrl+O"});
        tooltip.Set(other, {L"", L"Esc / Enter"});
        const InputState idle;
        const auto visibleCount = [&](std::string_view name) {
            const auto *root = &FindNode(canvas.Root(), "Tooltip");
            std::size_t count = 0;
            for (const auto &node : root->Children())
                count += node->Name() == name && node->IsVisible() ? 1 : 0;
            return count;
        };
        tooltip.Update(.3, idle, child.Id());
        Check(!tooltip.IsShown(), "A tooltip waits while the pointer rests.");
        tooltip.Update(.25, idle, child.Id());
        Check(tooltip.IsShown() && tooltip.ShownTarget() == button.Id(),
              "After half a second a pointed child shows its registered ancestor's hint.");
        Check(visibleCount("KeyChip") == 2 && visibleCount("KeySeparator") == 1,
              "Ctrl+O is two key chips joined by a plus.");
        const auto plate = FindNode(canvas.Root(), "Plate").BoundsInCanvas();
        const auto target = button.BoundsInCanvas();
        Check(std::abs(plate.y + plate.height - (target.y - 8)) < .01F && plate.height == 32,
              "The plate sits 8px under the control and grows to fit the key chips.");
        tooltip.Update(.016, idle, other.Id());
        Check(!tooltip.IsShown(), "Moving to another control hides the old hint first.");
        tooltip.Update(.07, idle, other.Id());
        Check(tooltip.ShownTarget() == other.Id() && visibleCount("KeyChip") == 2,
              "Moving straight to the next control shows its hint almost at once.");
        InputState press;
        InputStateTestAccess::Press(press);
        tooltip.Update(.016, press, other.Id());
        tooltip.Update(1.0, idle, other.Id());
        Check(!tooltip.IsShown(), "A press hides the hint until the pointer moves to another control.");
        tooltip.Update(.016, idle, 0);
        tooltip.Update(.6, idle, other.Id());
        Check(tooltip.IsShown(), "Coming back to the control shows the hint again.");
        tooltip.Update(.016, idle, other.Id(), false);
        Check(!tooltip.IsShown(), "A host can rest the hints while a modal covers the controls.");
        tooltip.Set(other, {});
        tooltip.Update(1.0, idle, other.Id());
        Check(!tooltip.IsShown(), "An empty hint removes the tooltip.");
        auto &low = CreateButton(canvas.Root(), {0, -358, 80, 30}, L"", "Low");
        tooltip.Set(low, {L"Low"});
        tooltip.Update(.6, idle, low.Id());
        const auto above = FindNode(canvas.Root(), "Plate").BoundsInCanvas();
        Check(tooltip.IsShown() && std::abs(above.y - (low.BoundsInCanvas().y + 30 + 8)) < .01F && above.height == 30,
              "Without room below, the tooltip opens above the control.");
        tooltip.Shutdown();
    }

    // A line inside a control that keeps the pointer (a song card title) hints only while pointed.
    void TestTooltipLines()
    {
        Visual2DCanvas canvas;
        UiTooltip tooltip;
        tooltip.Initialize(canvas);
        auto &card = CreateButton(canvas.Root(), {-200, 0, 300, 60}, L"", "Card");
        auto &title = CreateLabel(card, {20, 30, 200, 20}, L"Title", "Title");
        auto &other = CreateButton(canvas.Root(), {200, 0, 80, 40}, L"", "Other");
        tooltip.SetLine(title, {L"Title · Alias"});
        const InputState idle;
        const Point overTitle{-150, 40}, belowTitle{-150, 10};
        tooltip.Update(.6, idle, card.Id());
        Check(!tooltip.IsShown(), "A line hint needs the pointer position.");
        tooltip.Update(.6, idle, card.Id(), true, overTitle);
        Check(tooltip.IsShown() && tooltip.ShownTarget() == title.Id(),
              "Resting on the line inside the pointed card shows its hint.");
        tooltip.Update(.016, idle, card.Id(), true, belowTitle);
        Check(!tooltip.IsShown(), "Leaving the line hides its hint while the card stays pointed.");
        tooltip.Update(.6, idle, other.Id(), true, overTitle);
        Check(!tooltip.IsShown(), "A line shows nothing when the pointed control does not hold it.");
        tooltip.Clear();
        tooltip.Update(.6, idle, card.Id(), true, overTitle);
        Check(!tooltip.IsShown(), "Clearing forgets every hint.");
        // A long name breaks into lines inside the 300-wide plate instead of running past it.
        tooltip.Set(other, {L"Blue Hour Sketch of a Very Long Morning Over the Harbour and the Quiet Sea"});
        tooltip.Update(.6, idle, other.Id());
        const auto plate = FindNode(canvas.Root(), "Plate").BoundsInCanvas();
        const auto text = FindNode(canvas.Root(), "Label").GetComponent<TextVisualComponent>()->Text();
        const auto lines = 1 + std::ranges::count(text, L'\n');
        Check(tooltip.IsShown() && lines >= 2 && plate.width <= 300 && plate.height == 12 + 18 * static_cast<float>(lines),
              "A long name wraps within the 300-wide plate and the plate grows by a line each.");
        tooltip.Shutdown();
    }

    float Alpha(Visual2DNode &node)
    {
        return node.GetComponent<SpriteVisualComponent>()->Style().normal.alpha;
    }
    PointerInput Pointer(Point position, bool down, bool pressed, bool released = false)
    {
        PointerInput pointer;
        pointer.position = position;
        pointer.leftButtonDown = down;
        pointer.leftButtonPressed = pressed;
        pointer.leftButtonReleased = released;
        return pointer;
    }

    // The shared scrollbar: a 12px strip with a centred 6px bar, drags, rail presses and states.
    void TestScrollbar()
    {
        Visual2DCanvas canvas;
        Visual2DInputRouter router;
        UiScrollbar bar;
        bar.Attach(canvas.Root(), "Scroll");
        bar.SetRail({100, -150, UiScrollbar::GrabWidth, 300});
        bar.SetRange(300, 300, 0);
        Check(!bar.IsShown(), "Nothing to scroll hides the scrollbar.");
        bar.SetRange(300, 1200, 0);
        auto &rail = FindNode(canvas.Root(), "Scroll");
        auto &track = FindNode(rail, "Rail");
        auto &thumb = FindNode(rail, "Thumb");
        const auto bounds = thumb.BoundsInCanvas();
        Check(bar.IsShown() && rail.BoundsInCanvas().width == 12 && track.BoundsInCanvas().width == 6,
              "A 12px grab strip holds a 6px rail.");
        Check(bounds.width == 6 && bounds.x == 103 && std::abs(bounds.height - 75) < .01F,
              "The 6px bar is centred and its length shows the visible share.");
        Check(std::abs(bounds.y + bounds.height - 150) < .01F, "Offset zero puts the bar at the top.");
        Check(std::abs(Alpha(track) - .07F) < .001F && std::abs(Alpha(thumb) - .42F) < .001F,
              "A faint rail under a flat translucent cobalt bar.");
        bar.SetRange(300, 3000, 0);
        Check(std::abs(thumb.BoundsInCanvas().height - UiScrollbar::MinimumThumb) < .01F, "The bar is at least 32px long.");
        bar.SetRange(300, 1200, 0);

        static_cast<void>(bar.Track(Point{106, 140}, false));
        Check(std::abs(Alpha(thumb) - .62F) < .001F, "Pointing at the bar darkens it.");
        static_cast<void>(bar.Track(Point{106, 0}, false));
        Check(std::abs(Alpha(thumb) - .42F) < .001F, "Leaving the bar restores it.");
        // Press the bar 10px below its top: the grab point stays under the pointer.
        router.Process(canvas, Pointer({106, 140}, true, true));
        auto offset = bar.Track(Point{106, 140}, true);
        Check(bar.IsDragging() && offset && std::abs(*offset) < .01F && Alpha(thumb) == 1.0F,
              "Pressing the bar holds it in solid cobalt without moving the list.");
        offset = bar.Track(Point{106, 65}, true);
        Check(offset && std::abs(*offset - 300) < .01F, "Dragging the bar a third of its travel scrolls a third.");
        offset = bar.Track(Point{106, -400}, true);
        Check(offset && std::abs(*offset - 900) < .01F, "A drag past the end stops at the last offset.");
        router.Process(canvas, Pointer({106, -400}, false, false, true));
        Check(!bar.Track(Point{106, -400}, false) && !bar.IsDragging(), "Releasing ends the drag.");
        bar.SetRange(300, 1200, 0);
        // A rail press brings the bar's centre under the pointer and keeps following it.
        router.Process(canvas, Pointer({106, -50}, true, true));
        offset = bar.Track(Point{106, -50}, true);
        // 200px down the rail: the bar's top lands at 162.5 of its 225px travel.
        Check(offset && std::abs(*offset - 650) < .01F, "A rail press centres the bar under the pointer.");
        router.Process(canvas, Pointer({106, -50}, false, false, true));
        static_cast<void>(bar.Track(Point{106, -50}, false));

        UiScrollbar tabs;
        tabs.Attach(canvas.Root(), "Tabs", UiScrollbar::Axis::Horizontal);
        tabs.SetRail({-300, 200, 400, UiScrollbar::HorizontalGrabWidth});
        tabs.SetRange(400, 800, 400);
        const auto tabThumb = FindNode(FindNode(canvas.Root(), "Tabs"), "Thumb").BoundsInCanvas();
        Check(tabThumb.height == 4 && tabThumb.y == 202 && tabThumb.width == 200 && tabThumb.x == -100,
              "A horizontal scrollbar is 8px with a 4px bar at its offset.");

        // A host without a per-frame pointer follows the strip's own pointer events.
        UiScrollbar list;
        std::optional<float> followed;
        float wheel = 0;
        list.Attach(canvas.Root(), "List");
        list.FollowPointerEvents([&followed](float value) { followed = value; }, [&wheel](float delta) { wheel += delta; });
        list.SetRail({-500, -150, UiScrollbar::GrabWidth, 300});
        list.SetRange(300, 1200, 0);
        router.Process(canvas, Pointer({-494, 140}, true, true));
        router.Process(canvas, Pointer({-494, 65}, true, false));
        Check(followed && std::abs(*followed - 300) < .01F, "Pointer events alone drag an event-following bar.");
        router.Process(canvas, Pointer({-494, 65}, false, false, true));
        PointerInput turn = Pointer({-494, 0}, false, false);
        turn.wheelDelta = 1;
        router.Process(canvas, turn);
        Check(!list.IsDragging() && wheel == 1, "The wheel over the strip reaches its host.");
    }

    void TestVolumes(const std::filesystem::path &folder)
    {
        constexpr AudioVolumes half{50, 50, 100};
        Check(half.Music() == .25F && half.Effects() == .5F && AudioVolumes{}.Music() == 1,
              "Master scales music and effects linearly.");
        OptionSettings settings(folder / "Volumes.ini");
        std::string error;
        Check(settings.Load(error) && settings.Values().volumes == AudioVolumes{}, "First-run volumes are all 100%.");
        AudioSystem audio;
        Check(audio.Initialize({}, &CreateBackend, &CreateClip, error), "Initialize volume audio fixture.");
        TextCatalog texts;
        Visual2DCanvas canvas;
        Visual2DInputRouter router;
        OptionsPanel panel(texts, settings);
        panel.Initialize(canvas, audio);
        panel.Toggle();
        panel.Update(.2);
        Section(panel, canvas, "Audio");
        auto &track = FindNode(canvas.Root(), "Options.Volume.Music");
        auto &value = *FindNode(canvas.Root(), "Options.Volume.Music.Value").GetComponent<TextVisualComponent>();
        const auto bounds = track.BoundsInCanvas();
        Check(value.Text() == L"100%", "Each volume shows its saved percentage.");
        InputState input;
        InputStateTestAccess::Press(input);
        const Point quarter{bounds.x + 5 + (bounds.width - 10) * .25F, bounds.y + bounds.height / 2};
        static_cast<void>(panel.ProcessInput(input, quarter, track.Id(), router));
        Check(value.Text() == L"25%" && settings.Values().volumes.music == 100,
              "Dragging shows the new value but saves only on release.");
        InputStateTestAccess::Release(input);
        static_cast<void>(panel.ProcessInput(input, Point{bounds.x + bounds.width + 40, bounds.y}, {}, router));
        Check(value.Text() == L"100%" && settings.Values().volumes.music == 100,
              "A drag released past the end keeps the clamped maximum.");
        InputStateTestAccess::Press(input);
        static_cast<void>(panel.ProcessInput(input, quarter, track.Id(), router));
        InputStateTestAccess::Release(input);
        static_cast<void>(panel.ProcessInput(input, quarter, {}, router));
        Check(settings.Values().volumes == AudioVolumes{100, 25, 100}, "Releasing saves only the dragged volume.");
        OptionSettings restored(settings.Path());
        Check(restored.Load(error) && restored.Values().volumes.music == 25, "Volumes survive restart.");
        auto candidate = settings.Values();
        candidate.volumes.effects = 101;
        Check(!settings.Save(candidate, error) && settings.Values().volumes.effects == 100,
              "Volumes outside 0..100 are rejected.");
        {
            std::ofstream file(folder / "BadVolume.ini");
            file << "[Audio]\nMasterVolume=-1\n";
        }
        OptionSettings invalid(folder / "BadVolume.ini");
        Check(!invalid.Load(error) && invalid.Values().volumes == AudioVolumes{} && !invalid.LoadWarning().empty(),
              "A volume outside its range falls back to defaults with a warning.");
        panel.Shutdown();
    }
}

int main()
{
    const auto folder = std::filesystem::temp_directory_path() /
        (L"YOUMUOptionsTest-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        std::filesystem::create_directory(folder);
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        TestModePresentationRegistry();
        TestInvalidOptionFallback(folder);
        TestOptions(folder);
        TestExtraOptions(folder);
        TestSectionsAndJapanese(folder);
        TestVolumes(folder);
        TestTooltip();
        TestTooltipLines();
        TestScrollbar();
        TestActualAudio();
        std::filesystem::remove_all(folder);
        std::cout << "Options panel and persistence tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        SetFileAttributesW((folder / L"Option.ini").c_str(), FILE_ATTRIBUTE_NORMAL);
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
