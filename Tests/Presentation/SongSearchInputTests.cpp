#include "MusicSelectTestInput.h"
#include "SongSearchInputTests.h"
#include "GameScene/MusicSelectScene/Submodules/MusicSelectView.h"
#include "Presentation/ScrollableTextComponent.h"
#include <Windows.h>
#include <algorithm>
#include <vector>
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
            song.patterns[0].noteCount = 100;
            song.patterns[0].pattern.baseBpm = 150;
            song.patterns[0].pattern.mode = "Taiko";
            other.music.names = {"Other song"};
            other.patterns.resize(1);
            other.patterns[0].noteCount = 200;
            other.patterns[0].pattern.baseBpm = 90;
            other.patterns[0].pattern.mode = "Taiko";
            state_.catalog_.songs = {song, other};
            view_ = std::make_unique<MusicSelectView>(visuals_, state_, SongSelectPurpose::Gameplay, texts_, options_);
            view_->Initialize(services);
            const HWND parent = FindWindowW(nullptr, L"YOUMU.NativeSearch.Test");
            DWORD process{};
            GetWindowThreadProcessId(parent, &process);
            Check(process == GetCurrentProcessId(), "Search test owns its window.");
            HWND child = nullptr;
            while ((child = FindWindowExW(parent, child, L"EDIT", nullptr)) != nullptr)
                if ((GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) != 0) break;
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
            TestFilters(parent);
            report_.tested = true;
        }
        catch (const std::exception &error)
        {
            std::cerr << "Native search initialization: " << error.what() << '\n';
            throw;
        }
        void TestFilters(HWND parent)
        {
            auto *canvas = visuals_.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Filter Canvas missing.");
            InputState input;
            const auto click = [&](std::string_view name)
            {
                auto *node = Find(canvas->Root(), name);
                if (!node) throw std::runtime_error("Filter widget missing.");
                const auto bounds = node->BoundsInCanvas();
                Visual2DInputRouter router;
                PointerInput pointer{};
                pointer.available = true;
                pointer.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
                pointer.leftButtonDown = pointer.leftButtonPressed = true;
                router.Process(*canvas, pointer);
                pointer.leftButtonDown = pointer.leftButtonPressed = false;
                pointer.leftButtonReleased = true;
                router.Process(*canvas, pointer);
                Check(view_->Update(input, 0) == SongSelectCommand::None, "Filter buttons never launch or leave the scene.");
            };
            const auto edits = [&]
            {
                std::vector<HWND> fields;
                HWND edit{};
                while ((edit = FindWindowExW(parent, edit, L"EDIT", nullptr)) != nullptr)
                    if ((GetWindowLongPtrW(edit, GWL_STYLE) & WS_VISIBLE) != 0) fields.push_back(edit);
                std::ranges::sort(fields, [](HWND left, HWND right)
                {
                    RECT a{}, b{};
                    GetWindowRect(left, &a);
                    GetWindowRect(right, &b);
                    return a.top == b.top ? a.left < b.left : a.top < b.top;
                });
                return fields;
            };
            const auto setText = [](HWND field, const wchar_t *text)
            {
                SendMessageW(field, EM_SETSEL, 0, -1);
                SendMessageW(field, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(text));
            };
            setText(child_, L"");
            static_cast<void>(view_->Update(input, 0));
            click("FilterButton");
            auto fields = edits();
            Check(fields.size() == 6 && (GetWindowLongPtrW(child_, GWL_STYLE) & WS_VISIBLE) == 0,
                  "Filter shows native numeric fields, hides search and leaves uncalculated difficulty disabled.");
            auto *overlay = Find(canvas->Root(), "SongFilter.Overlay");
            if (!overlay) throw std::runtime_error("Filter overlay missing.");
            setText(fields[0], L"100");
            setText(fields[1], L"150");
            setText(fields[2], L"140");
            static_cast<void>(view_->Update(input, 0));
            Check(state_.visibleSongIndices_.size() == 2 && !state_.filter_.IsActive(), "Editing leaves the committed filter unchanged.");
            SetFocus(fields[0]);
            SendMessageW(fields[0], WM_IME_STARTCOMPOSITION, 0, 0);
            SendMessageW(fields[0], WM_KEYDOWN, VK_RETURN, 0);
            InputStateTestAccess::Key(input, VK_RETURN, true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && overlay->IsVisible(),
                  "IME confirmation does not apply filters or launch gameplay.");
            SendMessageW(fields[0], WM_IME_ENDCOMPOSITION, 0, 0);
            input = {};
            SendMessageW(fields[0], WM_KEYDOWN, VK_RETURN, 0);
            Check(view_->Update(input, 0) == SongSelectCommand::None && !overlay->IsVisible() &&
                      state_.visibleSongIndices_ == std::vector<std::size_t>{0},
                  "Native Enter commits inclusive note/BPM AND bounds without leaking to gameplay.");
            Check(edits().size() == 1 && state_.catalog_.songs.size() == 2, "Apply restores search and preserves the original catalog.");
            click("FilterButton");
            click("SongFilter.Reset");
            click("SongFilter.Close");
            Check(state_.filter_.IsActive() && state_.visibleSongIndices_.size() == 1, "Closing a reset draft cancels it.");
            click("FilterButton");
            fields = edits();
            Check(fields.size() == 6, "Reopen native fields.");
            setText(fields[0], L"200");
            setText(fields[1], L"100");
            click("SongFilter.Apply");
            Check(overlay->IsVisible() && state_.filter_.minimumNotes == 100,
                  "Invalid reversed bounds keep the panel open and preserve committed conditions.");
            click("SongFilter.Reset");
            click("SongFilter.Apply");
            Check(!state_.filter_.IsActive() && state_.visibleSongIndices_.size() == 2, "Reset plus Apply restores all songs.");
            click("FilterButton");
            click("SongFilter.Mode");
            Check(edits().empty(), "Native fields hide while the Canvas mode popup covers them.");
            input = {};
            InputStateTestAccess::Key(input, VK_ESCAPE, true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && !overlay->IsVisible(), "Filter Escape is modal and never leaves the scene.");
            input = {};
            click("FilterButton");
            view_->SetVisible(false);
            Check(edits().empty() && !overlay->IsVisible(), "Hiding the scene closes the filter and hides every native field.");
            view_->SetVisible(true);
            Check(edits().size() == 1, "Returning to the scene restores only search.");
            click("Favorites.Manage");
            Check(edits().empty(), "Favorite management hides native search and numeric fields.");
            input = {};
            InputStateTestAccess::Key(input, VK_LCONTROL, true);
            InputStateTestAccess::Key(input, 'O', true);
            Check(view_->Update(input, 0) == SongSelectCommand::None, "Favorites consumes background options shortcuts.");
            input = {};
            InputStateTestAccess::Key(input, VK_ESCAPE, true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && edits().size() == 1,
                  "Favorite Escape closes only its modal and restores native search.");
            input = {};
            setText(child_, L"angel");
            static_cast<void>(view_->Update(input, 0));
            const auto selected = state_.focusedSongPosition_, pattern = state_.selectedPatternIndex_;
            const auto visible = state_.visibleSongIndices_;
            click("RecordQuery");
            auto *recordFilter = Find(canvas->Root(), "RecordFilter.Overlay");
            if (!recordFilter) throw std::runtime_error("Record filter overlay missing.");
            fields = edits();
            Check(recordFilter && recordFilter->IsVisible() && fields.size() == 9,
                  "Record filters own a separate panel with real native numeric editors.");
            setText(fields[0], L"70"); setText(fields[1], L"60");
            click("RecordFilter.Apply");
            Check(recordFilter->IsVisible(), "Reversed record bounds are rejected without closing the draft.");
            setText(fields[0], L"60"); setText(fields[1], L"70");
            SetFocus(fields[0]);
            SendMessageW(fields[0], WM_IME_STARTCOMPOSITION, 0, 0);
            SendMessageW(fields[0], WM_KEYDOWN, VK_RETURN, 0);
            InputStateTestAccess::Key(input, VK_RETURN, true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && recordFilter->IsVisible(),
                  "IME Enter cannot apply a record filter or launch gameplay.");
            SendMessageW(fields[0], WM_IME_ENDCOMPOSITION, 0, 0);
            input = {};
            for (const auto key : {VK_RIGHT, VK_DOWN, VK_SPACE, static_cast<int>('O')})
            {
                input = {};
                InputStateTestAccess::Key(input, VK_LCONTROL, true);
                InputStateTestAccess::Key(input, static_cast<std::uint16_t>(key), true);
                Check(view_->Update(input, 0) == SongSelectCommand::None && recordFilter->IsVisible(),
                      "Record filtering consumes background shortcuts and navigation.");
            }
            input = {};
            SendMessageW(fields[0], WM_KEYDOWN, VK_RETURN, 0);
            Check(view_->Update(input, 0) == SongSelectCommand::None && !recordFilter->IsVisible() && edits().size() == 1,
                  "Native Enter applies record conditions and restores only search.");
            Check(state_.searchText_ == L"angel" && state_.visibleSongIndices_ == visible &&
                      state_.focusedSongPosition_ == selected && state_.selectedPatternIndex_ == pattern,
                  "Record filtering preserves the search, visible songs and selected chart.");
            click("RecordQuery");
            fields = edits();
            wchar_t minimum[32]{}; GetWindowTextW(fields[0], minimum, 32);
            Check(std::wstring_view(minimum) == L"60", "Reopen restores applied record conditions.");
            setText(fields[0], L"65");
            click("RecordFilter.Close");
            click("RecordQuery");
            fields = edits(); GetWindowTextW(fields[0], minimum, 32);
            Check(std::wstring_view(minimum) == L"60", "Close discards the pending record filter edit.");
            click("RecordFilter.Choice.0");
            Check(edits().empty(), "Record combo popups hide native fields so they cannot cover choices.");
            input = {};
            InputStateTestAccess::Key(input, VK_ESCAPE, true);
            Check(view_->Update(input, 0) == SongSelectCommand::None && !recordFilter->IsVisible(),
                  "Record Escape returns to song selection without leaving it.");
            input = {};
            click("RecordQuery");
            view_->SetVisible(false);
            Check(edits().empty() && !recordFilter->IsVisible(), "Scene hiding closes the record modal and all editors.");
            view_->SetVisible(true);
            Check(edits().size() == 1, "Scene return restores search without reopening a record draft.");
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
