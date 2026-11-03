#include "MusicSelectView.h"
#include "Presentation/ScrollableTextComponent.h"
#include "Presentation/PlayOptionsDialog.h"
#include "Mode/PlayGameModeFactory.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

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
    if (!filterPanel_.IsVisible() && !options_.IsVisible() && !(additionMenu_ && additionMenu_->IsVisible()) && input.MouseWheelDelta() != 0)
    {
        if (auto *hovered = canvas_->FindNode(inputRouter_.HoveredNode()))
            if (auto *text = hovered->GetComponent<ScrollableTextComponent>())
                textScrolled = text->Scroll(input.MouseWheelDelta(),
                                            input.IsKeyDown(VK_LSHIFT) || input.IsKeyDown(VK_RSHIFT));
    }
    if (!textScrolled && !filterPanel_.IsVisible() && !options_.IsVisible() && !(additionMenu_ && additionMenu_->IsVisible()) &&
        canvasPointer_.has_value() &&
        songViewport_ != nullptr && songViewport_->BoundsInCanvas().Contains(*canvasPointer_) &&
        std::abs(input.MouseWheelDelta()) > 0.0F)
    {
        scrollOffset_ -= input.MouseWheelDelta() * layout::ScrollStep;
        ApplyScrollOffset();
    }
}

bool MusicSelectView::ProcessActions()
{
    for (const Action &action : canvas_->TakeActions())
    {
        if (options_.ProcessAction(action))
        {
            ApplyTexts();
            RebuildSongCards();
            return true;
        }
        if (action.type == ActionType::Clicked && action.source == optionButtonId_)
        {
            SetSearchFocus(false);
            if (IsEditorSongSelect()) options_.Toggle();
            else static_cast<void>(EditPlayOptions(selection_.playOptions_, texts_));
            inputRouter_.Reset(*canvas_);
            return true;
        }
        if (options_.IsVisible())
        {
            continue;
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
        if (recordQueryButton_ && action.source == recordQueryButton_->Id())
        {
            EditRecordQuery(); return true;
        }
        if (recordPreviousButton_ && action.source == recordPreviousButton_->Id())
        {
            recordOffset_ = recordOffset_ >= 6 ? recordOffset_ - 6 : 0;
            ++recordRevision_; return true;
        }
        if (recordNextButton_ && action.source == recordNextButton_->Id() && recordNext_)
        {
            recordOffset_ += 6; ++recordRevision_; return true;
        }
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
        for (std::size_t position = 0; position < songCards_.size(); ++position)
        {
            if (IsEditorSongSelect() && songCards_[position].createDifficultyId != 0 &&
                action.source == songCards_[position].createDifficultyId)
            {
                const auto anchor = songCards_[position].button->BoundsInCanvas();
                SetSearchFocus(false);
                SelectVisibleSong(songCards_[position].visiblePosition);
                RefreshSearchPresentation();
                OpenAdditionMenu(anchor);
                return true;
            }
            if (songCards_[position].button != nullptr && action.source == songCards_[position].button->Id())
            {
                SetSearchFocus(false);
                SelectVisibleSong(songCards_[position].visiblePosition);
                RefreshSearchPresentation();
                return false;
            }
        }
        for (const auto [id, patternIndex] : difficultyButtonIds_)
        {
            if (action.source == id)
            {
                SetSearchFocus(false);
                SelectPattern(patternIndex);
                RefreshSearchPresentation();
                command_ = SongSelectCommand::Launch;
                return true;
            }
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
    for (const auto [id, patternIndex] : difficultyButtonIds_)
    {
        if (auto *button = canvas_->FindNode(id))
        {
            const bool selected = patternIndex == selection_.selectedPatternIndex_;
            ApplyButtonStyle(*button, selected ? AccentBlue : PureWhite, selected ? AccentHover : PaleBlue,
                             selected ? PureWhite : DeepBlue);
            BindSkinImage(*button, screenVisuals_, "MusicSelect",
                                                     selected ? "DifficultySelected" : "Difficulty");
        }
    }
    RefreshSelectionPresentation();
}
