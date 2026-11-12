#include "MusicSelectView.h"
#include "Presentation/ScrollableTextComponent.h"
#include "Mode/PlayGameModeFactory.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

using namespace song_select;

void MusicSelectView::ProcessPointer(const InputState &input)
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
    if (additionMenu_ && additionMenu_->IsVisible() &&
        (!canvasPointer_ || !additionMenu_->BoundsInCanvas().Contains(*canvasPointer_)))
        inputRouter_.Reset(*canvas_);
    else
        inputRouter_.Process(*canvas_, pointer);

    bool textScrolled = false;
    if (!IsSelectionModalVisible() && !options_.IsVisible() && !(additionMenu_ && additionMenu_->IsVisible()) && input.MouseWheelDelta() != 0)
    {
        textScrolled = canvasPointer_ && favoriteTabs_.Scroll(*canvasPointer_, input.MouseWheelDelta());
        if (!textScrolled) if (auto *hovered = canvas_->FindNode(inputRouter_.HoveredNode()))
            if (auto *text = hovered->GetComponent<ScrollableTextComponent>())
                textScrolled = text->Scroll(input.MouseWheelDelta(),
                                            input.IsKeyDown(VK_LSHIFT) || input.IsKeyDown(VK_RSHIFT));
    }
    if (!textScrolled && !IsSelectionModalVisible() && !options_.IsVisible() && !(additionMenu_ && additionMenu_->IsVisible()) &&
        canvasPointer_.has_value() &&
        songList_.Viewport() != nullptr && songList_.Viewport()->BoundsInCanvas().Contains(*canvasPointer_) &&
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
            if (IsEditorSongSelect()) options_.Toggle();
            else if (!options_.IsVisible())
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
            SetSearchFocus(false);
            SelectPattern(listAction.index);
            RefreshSearchPresentation();
            command_ = SongSelectCommand::Launch;
            return true;
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
