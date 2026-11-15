#include "MusicSelectView.h"
#include "Mode/PlayGameModeFactory.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

using namespace song_select;

PointerInput MusicSelectView::CanvasPointerInput(const InputState &input)
{
    canvasPointer_ = input.IsMouseInsideWindow()
                         ? MapScreenPointer(
                               {static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                               {static_cast<float>(width_), static_cast<float>(height_)}, *canvas_)
                         : std::nullopt;
    PointerInput pointer{};
    pointer.available = canvasPointer_.has_value();
    pointer.position = canvasPointer_.value_or(Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(MouseButton::Left);
    pointer.wheelDelta = input.MouseWheelDelta();
    pointer.timestampTicks = LatestPointerTimestamp(input);
    return pointer;
}

// While the alert is open it answers every key and the pointer reaches only its Canvas layer;
// queued actions of the controls under it are dropped.
bool MusicSelectView::ProcessAlert(const InputState &input)
{
    if (!alert_.IsOpen())
        return false;
    inputRouter_.Process(*canvas_, CanvasPointerInput(input));
    static_cast<void>(canvas_->TakeActions());
    static_cast<void>(alert_.Update(input));
    if (!alert_.IsOpen())
        inputRouter_.Reset(*canvas_);
    return true;
}

void MusicSelectView::ProcessPointer(const InputState &input)
{
    const auto pointer = CanvasPointerInput(input);
    if (IsAdditionMenuOpen() && (!canvasPointer_ || !additionMenu_->BoundsInCanvas().Contains(*canvasPointer_)))
        inputRouter_.Reset(*canvas_);
    else
        inputRouter_.Process(*canvas_, pointer);

    // The wheel scrolls tabs first, then the song list, unless an overlay owns the pointer.
    const bool overlayOpen = IsSelectionModalVisible() || options_.IsVisible() || IsAdditionMenuOpen();
    bool tabsScrolled = false;
    if (!overlayOpen && input.MouseWheelDelta() != 0)
    {
        tabsScrolled = canvasPointer_ && favoriteTabs_.Scroll(*canvasPointer_, input.MouseWheelDelta());
    }
    if (tabsScrolled || overlayOpen || !canvasPointer_.has_value())
        return;
    const auto *viewport = songList_.Viewport();
    if (viewport != nullptr && viewport->BoundsInCanvas().Contains(*canvasPointer_) &&
        std::abs(input.MouseWheelDelta()) > 0.0F)
        songList_.ScrollBy(-input.MouseWheelDelta() * layout::ScrollStep);
}

bool MusicSelectView::ProcessActions()
{
    for (const Action &action : canvas_->TakeActions())
    {
        if (options_.ProcessAction(action))
        {
            ApplyTexts();
            songList_.Rebuild();
            return true;
        }
        if (action.type == ActionType::Clicked && action.source == optionButtonId_)
        {
            SetSearchFocus(false);
            CloseAdditionMenu();
            options_.Toggle();
            inputRouter_.Reset(*canvas_);
            return true;
        }
        if (action.type == ActionType::Clicked && playOptionsButtonId_ != 0 && action.source == playOptionsButtonId_)
        {
            SetSearchFocus(false);
            if (!options_.IsVisible())
            {
                CloseAdditionMenu();
                playOptionsPanel_.Open(selection_.playOptions_);
            }
            inputRouter_.Reset(*canvas_);
            return true;
        }
        if (options_.IsVisible())
        {
            continue;
        }
        if (auto folder = favoriteTabs_.ProcessAction(action))
        {
            selection_.favoriteFolder_ = *folder;
            RebuildVisibleSongs();
            return true;
        }
        if (favoriteTabs_.IsManageAction(action))
        {
            OpenFavoritesPanel();
            return true;
        }
        if (filterButton_ && action.type == ActionType::Clicked && action.source == filterButton_->Id())
        {
            SetSearchFocus(false);
            CloseAdditionMenu();
            filterPanel_.Open(selection_.filter_, selection_.catalog_);
            return true;
        }
        if (action.type == ActionType::SelectionChanged && action.source == modeSelectorId_)
        {
            const auto modes = AvailableInputLayouts();
            if (action.selectedIndex < modes.size()) selection_.selectedMode_ = modes[action.selectedIndex].modeId;
            return true;
        }
        if (action.type == ActionType::SelectionChanged && action.source == sortSelectorId_)
        {
            selection_.sortMode_ = static_cast<SortMode>(
                std::min<std::size_t>(action.selectedIndex, static_cast<std::size_t>(SortMode::BaseBpm)));
            RebuildVisibleSongs();
            return false;
        }
        if (action.type != ActionType::Clicked)
        {
            continue;
        }
        const auto recordAction = records_.ProcessAction(action);
        if (recordAction == SongRecordAction::OpenFilter)
        {
            OpenRecordFilter(); return true;
        }
        if (recordAction == SongRecordAction::PageChanged) return true;
        if (action.source == backButtonId_)
        {
            command_ = SongSelectCommand::Back;
            return true;
        }
        if (action.source == goButtonId_)
        {
            command_ = SongSelectCommand::Launch;
            return true;
        }
        if (action.source == searchFieldId_)
        {
            SetSearchFocus(true);
            RefreshSearchPresentation();
            return false;
        }
        const auto listAction = songList_.ProcessAction(action);
        switch (listAction.kind)
        {
        case SongListView::ActionKind::CreateDifficultyClicked:
            SetSearchFocus(false);
            SelectVisibleSong(listAction.index);
            RefreshSearchPresentation();
            OpenAdditionMenu(listAction.anchor);
            return true;
        case SongListView::ActionKind::SongClicked:
            SetSearchFocus(false);
            SelectVisibleSong(listAction.index);
            RefreshSearchPresentation();
            return false;
        case SongListView::ActionKind::PatternClicked:
        {
            // Clicking another difficulty selects it; clicking the selected one starts it.
            SetSearchFocus(false);
            const bool start = selection_.selectedPatternIndex_ == listAction.index;
            SelectPattern(listAction.index);
            RefreshSearchPresentation();
            if (!start)
                return false;
            command_ = SongSelectCommand::Launch;
            return true;
        }
        case SongListView::ActionKind::None:
            break;
        }
    }
    return false;
}

bool MusicSelectView::ProcessKeyboard(const InputState &input)
{
    if (options_.IsVisible())
    {
        return false;
    }
    const bool controlDown = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('F')))
    {
        SetSearchFocus(true, true);
        return false;
    }
    // Consume all editing keys, including IME commit/cancel, before scene commands.
    if (ProcessSearchKeyboard(input)) return false;
    if (input.WasKeyPressed(VK_ESCAPE))
    {
        command_ = SongSelectCommand::Back;
        return true;
    }
    if (selection_.visibleSongIndices_.empty())
    {
        return false;
    }

    if (input.WasKeyPressed(VK_LEFT))
    {
        MoveSongFocus(-1);
    }
    else if (input.WasKeyPressed(VK_RIGHT))
    {
        MoveSongFocus(1);
    }
    else if (input.WasKeyPressed(VK_UP))
    {
        MoveDifficultyFocus(-1);
    }
    else if (input.WasKeyPressed(VK_DOWN))
    {
        MoveDifficultyFocus(1);
    }

    if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_SPACE))
    {
        command_ = SongSelectCommand::Launch;
        return true;
    }
    return false;
}

bool MusicSelectView::ProcessSearchKeyboard(const InputState &input)
{
    if (!searchFocused_) return false;
    if (searchInput_)
    {
        const bool submit = searchInput_->TakeSubmit();
        const bool cancel = searchInput_->TakeCancel();
        if (submit || cancel) SetSearchFocus(false);
    }
    else if (input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_ESCAPE))
        SetSearchFocus(false);
    return true;
}

void MusicSelectView::MoveSongFocus(const int delta)
{
    selection_.MoveSongFocus(delta);
    RefreshSelection();
}

void MusicSelectView::MoveDifficultyFocus(const int delta)
{
    const auto previousSong = selection_.focusedSongPosition_;
    const auto previousPattern = selection_.selectedPatternIndex_;
    selection_.MoveDifficultyFocus(delta);
    if (previousSong != selection_.focusedSongPosition_)
        RefreshSelection();
    else if (previousPattern != selection_.selectedPatternIndex_)
        SelectPattern(selection_.selectedPatternIndex_);
}

void MusicSelectView::SelectVisibleSong(const std::size_t visiblePosition)
{
    selection_.SelectVisibleSong(visiblePosition);
    RefreshSelection();
}

void MusicSelectView::SelectPattern(const std::size_t index)
{
    selection_.SelectPattern(index);
    songList_.RefreshPatternSelection();
    RefreshSelectionPresentation();
}
