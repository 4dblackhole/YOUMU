#include "MusicSelectTestInput.h"
#include "Presentation/PlayOptionsPanel.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace
{
    void Check(bool value, const char *message)
    {
        if (!value) { std::cerr << message << '\n'; throw std::runtime_error(message); }
    }
    Visual2DNode &Find(Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name) return node;
        for (const auto &child : node.Children())
            try { return Find(*child, name); } catch (const std::out_of_range &) {}
        throw std::out_of_range(std::string(name));
    }
    Action ActionFor(Visual2DCanvas &canvas, std::string_view name, ActionType type = ActionType::Clicked)
    {
        Action action; action.type = type; action.source = Find(canvas.Root(), name).Id(); return action;
    }
    class Edit final : public ITextInput
    {
      public:
        std::wstring text; TextInputBounds bounds{}; bool visible{}, focused{}, dirty{}, cancel{}, submit{};
        void UserText(std::wstring value) { text = std::move(value); dirty = true; }
        void SetText(std::wstring_view value) override { text = value; dirty = false; }
        void SetPlaceholder(std::wstring_view) override {}
        void SetBounds(TextInputBounds value) override { bounds = value; }
        void SetFont(std::wstring_view, std::uint32_t) override {}
        void SetColors(std::uint32_t, std::uint32_t) override {}
        void SetVisible(bool value) noexcept override { visible = value; if (!value) focused = false; }
        void Focus(bool) noexcept override { focused = visible; }
        void Blur() noexcept override { focused = false; }
        bool HasFocus() const noexcept override { return focused; }
        bool TakeText(std::wstring &value) override { if (!std::exchange(dirty, false)) return false; value = text; return true; }
        bool TakeSubmit() noexcept override { return std::exchange(submit, false); }
        bool TakeCancel() noexcept override { return std::exchange(cancel, false); }
    };
    class Window final : public IWindowControl
    {
      public:
        std::vector<Edit *> edits;
        WindowResolution ClientSize() const noexcept override { return {1280, 720}; }
        std::vector<WindowResolution> DisplayResolutions() const override { return {}; }
        bool SetClientSize(WindowResolution, std::string &) override { return false; }
        std::unique_ptr<ITextInput> CreateTextInput() override
        {
            auto edit = std::make_unique<Edit>(); edits.push_back(edit.get()); return edit;
        }
    };
    void TestDraftValidationAndReset()
    {
        auto canvasOwner = std::make_unique<Visual2DCanvas>(Size{1280, 720}, CanvasScaleMode::Fixed);
        auto &canvas = *canvasOwner;
        TextCatalog texts("Client/Texts/Languages"); texts.SetLanguage(Language::Japanese);
        Window window;
        PlayOptionsPanel panel; panel.Initialize(canvas, texts, &window); panel.SetViewport(720);
        panel.Open({});
        const auto start = Find(canvas.Root(), "PlayOptions.Panel").BoundsInCanvas().y;
        panel.Update(.1);
        Check(Find(canvas.Root(), "PlayOptions.Panel").BoundsInCanvas().y > start && !window.edits[0]->visible,
              "Panel enters upward; native editors stay hidden during motion.");
        panel.Update(.1);
        Check(window.edits.size() == 5 && window.edits[0]->visible && window.edits[1]->visible &&
                  !window.edits[2]->visible && !window.edits[3]->visible, "Only active fields appear after opening.");
        const auto flag = [&](std::string_view name, bool value) {
            auto action = ActionFor(canvas, name, ActionType::ValueChanged); action.value = value ? 1.0F : 0;
            Check(panel.ProcessAction(action), "A visible flag can change.");
        };
        auto unit = ActionFor(canvas, "PlayOptions.Unit", ActionType::SelectionChanged); unit.selectedIndex = 1;
        Check(panel.ProcessAction(unit), "BPM mode can be selected.");
        flag("PlayOptions.random", true); flag("PlayOptions.mirror", true);
        flag("PlayOptions.ignoreScroll", true); flag("PlayOptions.metronome", true);
        Check(panel.ProcessAction(ActionFor(canvas, "PlayOptions.accelerate.Label")), "Accelerate reveals pitch settings.");
        flag("PlayOptions.musicPitch", false); flag("PlayOptions.hitPitch", true); flag("PlayOptions.overrideJudge", true);
        Check(window.edits[2]->visible && window.edits[3]->visible && window.edits[4]->visible,
              "Judge and Random reveal their own editors; all expanded content fits 1280x720.");
        for (std::size_t i = 0; i < window.edits.size(); ++i)
            window.edits[i]->UserText(std::array{L"240", L"1.5", L"60.00", L"18446744073709551615", L"35"}[i]);
        Check(!panel.TakeApplied(), "Editing never publishes before the panel closes.");
        panel.ProcessInput({}, std::nullopt);
        Check(panel.ProcessAction(ActionFor(canvas, "PlayOptions.Close")) && !panel.IsVisible(), "Close applies immediately.");
        auto applied = panel.TakeApplied();
        Check(applied && applied->scrollUnit == PlayOptions::ScrollUnit::Bpm && applied->scrollSpeed == 240 &&
                  applied->random && applied->mirror && applied->metronome && applied->ignoreScrollEffects &&
                  applied->randomSeed == UINT64_MAX && applied->randomAttemptProbability == .35 &&
                  applied->accelerate == 1.5 && !applied->preserveMusicPitch && applied->preserveHitSoundPitch &&
                  applied->judgeLevel == JudgeLevel{60}, "Close preserves every independent existing option and uint64 precision.");
        Check(!panel.TakeApplied(), "A successful close publishes once.");
        const auto open = [&] { panel.Open(*applied); panel.Update(.2); };
        open();
        for (const auto &value : {L"0", L"nan", L"inf", L"garbage"})
        {
            window.edits[0]->UserText(value); panel.Close();
            Check(panel.IsVisible() && !panel.TakeApplied(), "Invalid speed keeps the panel open without publishing.");
        }
        window.edits[0]->UserText(L"240"); window.edits[1]->UserText(L"2.01"); panel.Close();
        Check(panel.IsVisible(), "Accelerate keeps the original 0.5..2 range.");
        window.edits[1]->UserText(L"0.5"); window.edits[2]->UserText(L"60.001"); panel.Close();
        Check(panel.IsVisible(), "JudgeLevel still requires exact hundredths.");
        window.edits[2]->UserText(L"60.00"); window.edits[3]->UserText(L"-1");
        flag("PlayOptions.random", false); panel.Close();
        Check(panel.IsVisible() && !panel.TakeApplied() && window.edits[3]->visible &&
                  !Find(canvas.Root(), "PlayOptions.random").GetComponent<ToggleBehaviorComponent>()->IsChecked(),
              "Invalid hidden Random input is repairable without silently enabling Random.");
        window.edits[3]->UserText(L"18446744073709551616"); panel.Close();
        Check(panel.IsVisible(), "Seed overflow is rejected.");
        window.edits[3]->UserText(L"0"); window.edits[4]->UserText(L"101"); panel.Close();
        Check(panel.IsVisible(), "Probability range remains 0..100 percent.");
        Check(Find(canvas.Root(), "PlayOptions.Status").GetComponent<TextVisualComponent>()->Text().find(L"確認") != std::wstring::npos,
              "Error guidance uses the chosen explanation language.");
        Check(panel.ProcessAction(ActionFor(canvas, "PlayOptions.Reset")) && panel.IsVisible() && !panel.TakeApplied(),
              "Reset restores the draft; it is applied on close.");
        window.edits[0]->cancel = true; panel.ProcessInput({}, std::nullopt);
        const auto defaults = panel.TakeApplied();
        Check(defaults && defaults->scrollUnit == PlayOptions::ScrollUnit::Multiplier && defaults->scrollSpeed == 1 &&
                  defaults->accelerate == 1 && !defaults->random && !defaults->mirror && !defaults->metronome &&
                  !defaults->ignoreScrollEffects && defaults->randomSeed == 0 && defaults->randomAttemptProbability == 1 &&
                  defaults->preserveMusicPitch && defaults->preserveHitSoundPitch && !defaults->judgeLevel,
              "Native Esc applies every existing default, including hidden fields and chart JudgeLevel.");
        open();
        auto hidden = ActionFor(canvas, "PlayOptions.hitPitch", ActionType::ValueChanged); hidden.value = 0;
        Check(!panel.ProcessAction(hidden), "Hidden pitch controls reject stale actions.");
        Check(panel.ProcessAction(ActionFor(canvas, "PlayOptions.accelerate.Label")), "Expand pitch again.");
        canvas.SetFixedLogicalSize({900, 400}); panel.SetViewport(400);
        const auto previous = Find(canvas.Root(), "PlayOptions.Content").BoundsInCanvas().y;
        const auto bounds = Find(canvas.Root(), "PlayOptions.Viewport").BoundsInCanvas();
        InputState wheel; InputStateTestAccess::Mouse(wheel, {}, -10);
        panel.ProcessInput(wheel, Point{bounds.x + 10, bounds.y + 10});
        Check(Find(canvas.Root(), "PlayOptions.Content").BoundsInCanvas().y > previous, "Small viewports scroll one three-column body.");
        panel.Dismiss(); Check(!panel.TakeApplied(), "Scene/lifetime dismissal never publishes unfinished values.");
        panel.Shutdown();
        canvasOwner.reset();
        panel.Shutdown(); // Cleanup remains safe after the borrowed Canvas has gone away.
    }

    struct Report { bool tested{}, rendered{}, cleaned{}; };
    class NativeClient final : public IGameClient
    {
      public:
        explicit NativeClient(Report &report) : report_(report) {}
        EngineConfig GetEngineConfig() const override
        {
            EngineConfig config; config.windowTitle = L"YOUMU.PlayOptions.Test"; config.showWindow = false;
            config.audio.preferredBackend = AudioOutputBackend::NoSound; return config;
        }
        void Initialize(const EngineServices &services) override try
        {
            visuals_.Initialize(services.visual2DRendering, {1280, 720});
            SongCatalogEntry song; song.music.names = {"Alpha"}; song.patterns.resize(1);
            song.patterns[0].noteCount = 10; song.patterns[0].pattern.mode = "Taiko";
            selection_.catalog_.songs.push_back(song); selection_.searchText_ = L"Alpha";
            view_ = std::make_unique<MusicSelectView>(visuals_, selection_, SongSelectPurpose::Gameplay, texts_, options_,
                "build/play-options-panel-20261105/native-records.sqlite3");
            view_->Initialize(services); canvas_ = visuals_.FindCanvas(1);
            Check(canvas_ != nullptr, "Native play options share the song selection Canvas.");
            parent_ = FindWindowW(nullptr, L"YOUMU.PlayOptions.Test");
            Click("OptionSelect");
            Check(Find(canvas_->Root(), "PlayOptions.Overlay").IsVisible(), "Footer opens the new in-game panel.");
            Check(view_->Update({}, .2) == SongSelectCommand::None, "Opening does not launch a chart.");
            const auto speed = Editor("PlayOptions.scrollSpeed.Field");
            if (!speed) throw std::runtime_error("Real native numeric editor is not visible.");
            firstEdit_ = speed;
            SetWindowTextW(speed, L"2.75");
            SetFocus(speed);
            SendMessageW(speed, EM_SETSEL, 1, 1);
            Click("PlayOptions.scrollSpeed.Field");
            DWORD begin{}, end{};
            SendMessageW(speed, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
            Check(begin == 1 && end == 1, "Canvas field routing preserves the native caret and selection.");
            InputState controls; InputStateTestAccess::Key(controls, VK_LCONTROL, true); InputStateTestAccess::Key(controls, 'O', true);
            Check(view_->Update(controls, 0) == SongSelectCommand::None &&
                      !Find(canvas_->Root(), "Options.Panel").IsVisible(), "Play panel isolates Ctrl+O from global options.");
            Click("Go");
            Check(Find(canvas_->Root(), "PlayOptions.Overlay").IsVisible() && selection_.playOptions_.scrollSpeed == 1,
                  "Backdrop blocks underlying launch and edits remain unpublished.");
            SendMessageW(speed, WM_KEYDOWN, VK_ESCAPE, 0);
            InputState escape; InputStateTestAccess::Key(escape, VK_ESCAPE, true);
            Check(view_->Update(escape, 0) == SongSelectCommand::None && selection_.playOptions_.scrollSpeed == 2.75 &&
                      !Find(canvas_->Root(), "PlayOptions.Overlay").IsVisible(),
                  "Actual native Esc closes/applies once without also going Back.");
            Check(selection_.searchText_ == L"Alpha" && selection_.focusedSongPosition_ == 0 && selection_.selectedPatternIndex_ == 0,
                  "Closing preserves search and selected song/chart.");
            Click("OptionSelect"); static_cast<void>(view_->Update({}, .2));
            Check(GetWindowTextLengthW(Editor("PlayOptions.scrollSpeed.Field")) == 4, "Reopening shows the applied value.");
            Click("PlayOptions.Reset"); Click("PlayOptions.Close");
            Check(selection_.playOptions_.scrollSpeed == 1 && !Find(canvas_->Root(), "PlayOptions.Overlay").IsVisible(),
                  "Actual pointer Reset then Close applies the default.");
            Click("OptionSelect"); static_cast<void>(view_->Update({}, .2));
            report_.tested = true;
        }
        catch (const std::exception &error) { std::cerr << "Native Play options: " << error.what() << '\n'; throw; }
        bool Update(const UpdateContext &context) override
        {
            if (context.totalSeconds > 10) throw std::runtime_error("Native play option render timed out.");
            return !report_.rendered;
        }
        void Render(const RenderContext &context) override { visuals_.Render(context); report_.rendered = true; }
        void OnResize(std::uint32_t width, std::uint32_t height) override
        { visuals_.OnResize(width, height); if (view_) view_->OnResize(width, height); }
        void Shutdown() noexcept override
        {
            if (view_) view_->Shutdown();
            view_.reset(); report_.cleaned = !IsWindow(firstEdit_) && visuals_.CanvasCount() == 0;
            visuals_.Shutdown();
        }
      private:
        void Click(std::string_view name)
        {
            const auto b = Find(canvas_->Root(), name).BoundsInCanvas();
            Visual2DInputRouter router; PointerInput p;
            p.available = true; p.position = {b.x + b.width / 2, b.y + b.height / 2};
            p.leftButtonDown = p.leftButtonPressed = true; router.Process(*canvas_, p);
            p.leftButtonDown = p.leftButtonPressed = false; p.leftButtonReleased = true; router.Process(*canvas_, p);
            Check(view_->Update({}, 0) == SongSelectCommand::None, "Panel clicks never launch or leave selection.");
        }
        HWND Editor(std::string_view name)
        {
            const auto bounds = Find(canvas_->Root(), name).BoundsInCanvas();
            HWND child = nullptr;
            while ((child = FindWindowExW(parent_, child, L"EDIT", nullptr)) != nullptr)
            {
                if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE)) continue;
                RECT rect{}; GetWindowRect(child, &rect);
                POINT center{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2}; ScreenToClient(parent_, &center);
                const auto point = MapScreenPointer({static_cast<float>(center.x), static_cast<float>(center.y)}, {1280,720}, *canvas_);
                if (point && bounds.Contains(*point)) return child;
            }
            return nullptr;
        }
        Report &report_; HWND parent_{}, firstEdit_{}; Visual2DCanvas *canvas_{};
        ScreenVisual2DManager visuals_; SongSelectionState selection_; TextCatalog texts_{"Client/Texts/Languages"};
        OptionSettings options_{"build/play-options-panel-20261105/unused-options.ini"}; std::unique_ptr<MusicSelectView> view_;
    };
}

void TestPlayOptionsPanel()
{
    TestDraftValidationAndReset();
    Report report;
    // The preceding native-search Run destroys its window and posts WM_QUIT.
    MSG message{};
    while (PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {}
    const auto result = Run(std::make_unique<NativeClient>(report));
    if (result != 0 || !report.tested || !report.rendered || !report.cleaned)
        std::cerr << "Native Play options result/tested/rendered/cleaned: " << result << '/' << report.tested
                  << '/' << report.rendered << '/' << report.cleaned << '\n';
    Check(result == 0 && report.tested && report.rendered && report.cleaned,
          "Native play option editing, rendering and cleanup must pass.");
    std::cout << "Play options close/apply, Reset, validation, expansion, scrolling and native isolation passed.\n";
}
