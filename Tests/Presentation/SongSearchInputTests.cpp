#include "MusicSelectTestInput.h"
#include "SongSearchInputTests.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "Presentation/ScrollableTextComponent.h"
#include <Windows.h>
#include <iostream>
#include <stdexcept>

namespace
{
    void Check(bool value, const char *message)
    {
        if (!value)
        {
            std::cerr << message << '\n';
            throw std::runtime_error(message);
        }
    }
    struct Report { bool tested{}, rendered{}, measured{}, cleaned{}; };
    Visual2DNode *Find(Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name) return &node;
        for (const auto &child : node.Children())
            if (auto *found = Find(*child, name)) return found;
        return nullptr;
    }
    class SearchClient final : public IGameClient
    {
      public:
        explicit SearchClient(Report &report) : report_(report) {}
        EngineConfig GetEngineConfig() const override
        {
            EngineConfig config;
            config.windowTitle = L"YOUMU.NativeSearch.Test";
            config.showWindow = false;
            config.audio.preferredBackend = AudioOutputBackend::NoSound;
            return config;
        }
        void Initialize(const EngineServices &services) override
        try
        {
            visuals_.Initialize(services.visual2DRendering, {1280, 720});
            SongCatalogEntry song, other;
            song.music.names = {"Angel Dream", "天使の夢", "천사의 꿈"};
            song.patterns.resize(1);
            other.music.names = {"Other song"};
            other.patterns.resize(1);
            state_.catalog_.songs = {song, other};
            view_ = std::make_unique<MusicSelectView>(visuals_, state_, SongSelectPurpose::Editor, texts_, options_);
            view_->Initialize(services);
            const HWND parent = FindWindowW(nullptr, L"YOUMU.NativeSearch.Test");
            DWORD process{};
            GetWindowThreadProcessId(parent, &process);
            Check(process == GetCurrentProcessId(), "Search test owns its window.");
            const HWND child = FindWindowExW(parent, nullptr, L"EDIT", nullptr);
            if (!child) throw std::runtime_error("Search uses a real native editor.");
            child_ = child;
            InputState input;
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'F', true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && GetFocus() == child, "Ctrl+F focuses inline search.");
            input = {};
            const auto query = [&](const wchar_t *value)
            {
                SendMessageW(child, EM_SETSEL, 0, -1);
                SendMessageW(child, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(value));
                Check(view_->Update(input, 0) == SongSelectCommand::None, "Typing must not issue scene commands.");
                Check(state_.searchText_ == value && state_.visibleSongIndices_ == std::vector<std::size_t>{0},
                      "Native Unicode input updates real song results.");
            };
            query(L"angel");
            query(L"dream angel");
            query(L"天使");
            query(L"천사");
            query(L"angel");
            SendMessageW(child, WM_CHAR, L' ', 0);
            Check(view_->Update(input, 0) == SongSelectCommand::None && state_.searchText_ == L"angel ",
                  "WM_CHAR space edits query without starting gameplay.");
            for (const auto key : {VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_SPACE})
            {
                input = {};
                InputStateTestAccess::Key(input, static_cast<std::uint16_t>(key), true);
                Check(view_->Update(input, 0) == SongSelectCommand::None && state_.focusedSongPosition_ == 0,
                      "Editing keys must not navigate songs or launch gameplay.");
            }
            for (const auto key : {VK_RETURN, VK_ESCAPE})
            {
                input = {};
                SendMessageW(child, WM_IME_STARTCOMPOSITION, 0, 0);
                SendMessageW(child, WM_KEYDOWN, static_cast<WPARAM>(key), 0);
                InputStateTestAccess::Key(input, static_cast<std::uint16_t>(key), true);
                Check(view_->Update(input, 0) == SongSelectCommand::None && GetFocus() == child,
                      "IME Enter/Escape must stay inside the search editor.");
                SendMessageW(child, WM_IME_ENDCOMPOSITION, 0, 0);
            }
            input = {};
            SendMessageW(child, WM_KEYDOWN, VK_RETURN, 0);
            InputStateTestAccess::Key(input, VK_RETURN, true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && GetFocus() != child,
                  "Search submit ends editing without launching gameplay.");
            RECT before{}, after{};
            Check(GetWindowRect(child, &before) != FALSE, "Read native search bounds.");
            std::string resizeError;
            Check(services.windowControl->SetClientSize({960, 540}, resizeError), "Resize native search window.");
            visuals_.OnResize(960, 540);
            view_->OnResize(960, 540);
            Check(GetWindowRect(child, &after) != FALSE && after.right - after.left < before.right - before.left,
                  "Native search width follows responsive Canvas layout.");
            MapWindowPoints(nullptr, parent, reinterpret_cast<POINT *>(&after), 2);
            Check(after.left >= 0 && after.top >= 0 && after.right <= 960 && after.bottom <= 540,
                  "Resized search stays inside the client area.");
            input = {};
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            static_cast<void>(view_->Update(input, .3));
            Check((GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) == 0, "Options cover and hide native search.");
            view_->SetVisible(false);
            Check(GetFocus() != child, "Hidden scene releases search focus.");
            view_->SetVisible(true);
            input = {};
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            static_cast<void>(view_->Update(input, .3));
            input = {};
            static_cast<void>(view_->Update(input, .3));
            report_.tested = true;
        }
        catch (const std::exception &error)
        {
            std::cerr << "Native search initialization: " << error.what() << '\n';
            throw;
        }
        bool Update(const UpdateContext &context) override
        {
            Check(context.totalSeconds < 10, "Native search test completes promptly.");
            visuals_.Update(context.deltaSeconds);
            if (report_.rendered)
            {
                auto *canvas = visuals_.FindCanvas(1);
                if (!canvas) throw std::runtime_error("Rendered metadata Canvas missing.");
                auto *title = Find(canvas->Root(), "SelectedSong");
                if (!title) throw std::runtime_error("Rendered metadata title missing.");
                auto *scroll = title->GetComponent<ScrollableTextComponent>();
                Check(scroll && scroll->Text() == L"Angel Dream\n天使の夢\n천사의 꿈" &&
                          scroll->ScrollRange().height > 0,
                      "Real DirectWrite multiline measurement drives the selected title scrollbar.");
                report_.measured = true;
            }
            return !report_.rendered;
        }
        void Render(const RenderContext &context) override
        {
            visuals_.Render(context);
            report_.rendered = true;
        }
        void OnResize(std::uint32_t width, std::uint32_t height) override
        {
            visuals_.OnResize(width, height);
            if (view_) view_->OnResize(width, height);
        }
        void Shutdown() noexcept override
        {
            if (view_) view_->Shutdown();
            view_.reset();
            report_.cleaned = !IsWindow(child_) && visuals_.CanvasCount() == 0;
        }
      private:
        Report &report_;
        HWND child_{};
        ScreenVisual2DManager visuals_;
        SongSelectionState state_;
        TextCatalog texts_;
        OptionSettings options_{"build/native-search-unused.ini"};
        std::unique_ptr<MusicSelectView> view_;
    };
}
void TestNativeSongSearch()
{
    Report report;
    Check(Run(std::make_unique<SearchClient>(report)) == 0 && report.tested && report.rendered &&
              report.measured && report.cleaned,
          "Native search integration and shutdown must pass.");
}
