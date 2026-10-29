#include "EditorView.h"
#include "EditorTime.h"
#include <shellapi.h>
#include "Texts/GameIdentity.h"
#include <algorithm>
#include <format>
#include <sstream>
using namespace editor_ui;

void EditorView::UpdateSliders(const UpdateContext &context)
{
    if (state_.Tab() != EditorTab::Pattern && state_.Tab() != EditorTab::Audio)
        return;

    const auto &input = context.input;
    PointerInput pointer{};
    const auto mapped =
        MapScreenPointer({static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                            {static_cast<float>(width), static_cast<float>(height)}, *canvas.Get());
    pointer.available = input.IsMouseInsideWindow() && mapped.has_value();
    pointer.position =
        mapped.value_or(Point{static_cast<float>(input.MousePositionX()) / canvas.Get()->PixelScale() -
                                     canvas.Get()->LogicalSize().width * .5F,
                                 canvas.Get()->LogicalSize().height * .5F -
                                     static_cast<float>(input.MousePositionY()) / canvas.Get()->PixelScale()});
    pointer.leftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(MouseButton::Left);
    sliderInput_.Process(*canvas.Get(), pointer);

    bool changed = false;
    bool timeChanged = false;
    bool rateChanged = false;
    for (const auto &action : canvas.Get()->TakeActions())
    {
        changed |= action.source == divisionSliderNode_->Id() && action.type == ActionType::ValueChanged;
        timeChanged |= action.source == timelineSliderNode_->Id() && action.type == ActionType::ValueChanged;
        rateChanged |= action.source == playbackRateSliderNode_->Id() && action.type == ActionType::ValueChanged;
    }
    // A direct-input value above 16 can share the slider's end position.
    if (pointer.leftButtonPressed && sliderInput_.CapturedNode() == divisionSliderNode_->Id())
        changed = true;
    if (pointer.leftButtonPressed && sliderInput_.CapturedNode() == timelineSliderNode_->Id())
        timeChanged = true;
    if (changed)
    {
        const float value = divisionSliderNode_->GetComponent<SliderBehaviorComponent>()->Value();
        const int division = std::clamp(1 + static_cast<int>(std::lround(value * 15)), 1, 16);
        if (state_.Score().division != division)
        {
            state_.SetDivision(division);
        }
        SyncDivisionSlider();
    }
    if (timeChanged)
    {
        const auto [begin, end] = state_.TimelineRangeMilliseconds();
        const float value = timelineSliderNode_->GetComponent<SliderBehaviorComponent>()->Value();
        state_.Seek(std::round(std::lerp(begin, end, static_cast<double>(value))));
    }
    if (rateChanged)
    {
        const float value = playbackRateSliderNode_->GetComponent<SliderBehaviorComponent>()->Value();
        const double rate = std::round(std::lerp(EditorPlayback::MinimumRate, EditorPlayback::MaximumRate,
                                               static_cast<double>(value)) * 20) / 20;
        state_.SetPlaybackRate(rate);
    }
}

EditorMenuInputResult EditorView::UpdateMenu(const InputState &input)
{
    EditorMenuInput menu;
    if (input.IsMouseInsideWindow())
        menu.pointer = MapScreenPointer(
            {static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
            {static_cast<float>(width), static_cast<float>(height)}, *canvas.Get());
    menu.leftPressed = input.WasMouseButtonPressed(MouseButton::Left);
    menu.rightPressed = input.WasMouseButtonPressed(MouseButton::Right);
    const bool alt = input.IsKeyDown(VK_LMENU) || input.IsKeyDown(VK_RMENU);
    const bool control = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    const bool shift = input.IsKeyDown(VK_LSHIFT) || input.IsKeyDown(VK_RSHIFT);
    menu.toggle = input.WasKeyPressed(VK_F10) || (alt && input.WasKeyPressed('F'));
    menu.toggleEdit = alt && input.WasKeyPressed('E');
    menu.accept = input.WasKeyPressed(VK_RETURN);
    menu.dismiss = input.WasKeyPressed(VK_ESCAPE);
    menu.selectStep = input.WasKeyPressed(VK_DOWN) ? 1 : input.WasKeyPressed(VK_UP) ? -1 : 0;
    menu.switchStep = input.WasKeyPressed(VK_RIGHT) ? 1 : input.WasKeyPressed(VK_LEFT) ? -1 : 0;
    if (control)
    {
        if (input.WasKeyPressed('S')) menu.shortcutAction = EditorMenuAction::Save;
        else if (input.WasKeyPressed('Z')) menu.shortcutAction = shift ? EditorMenuAction::Redo : EditorMenuAction::Undo;
        else if (input.WasKeyPressed('Y')) menu.shortcutAction = EditorMenuAction::Redo;
        else if (input.WasKeyPressed('C')) menu.shortcutAction = EditorMenuAction::Copy;
        else if (input.WasKeyPressed('V')) menu.shortcutAction = shift ? EditorMenuAction::PasteWithBlanks : EditorMenuAction::Paste;
    }
    return menuBar_.Update(menu, state_.Document().Dirty());
}

bool EditorView::Update(const UpdateContext &context)
{
    bool leave = false;
    const auto &input = context.input;
    if (textRevision_ != texts_.Revision())
    {
        textRevision_ = texts_.Revision();
        state_.RequestRebuild();
    }
    try
    {
        const auto menu = UpdateMenu(input);
        if (menu.action)
            ExecuteMenuAction(*menu.action);
        if (!menu.consumeKeyboard)
            leave = UpdateKeyboard(input);
        UpdateTimelineNavigation(input, !menu.consumeKeyboard, !menu.consumePointer);
        if (menu.consumePointer)
            sliderInput_.Reset(*canvas.Get());
        else
            UpdatePointer(context);
    }
    catch (const std::exception &e)
    {
        state_.SetStatus(e.what());
    }
    if (state_.NeedsRebuild())
        Build();
    return leave;
}

bool EditorView::UpdateKeyboard(const InputState &input)
{
    bool leave = false;
    if (input.WasKeyPressed(VK_ESCAPE))
    {
        const bool selectedNotes = !state_.SelectedNoteIds().empty() || state_.NoteSelectionRange().has_value();
        const bool interaction = state_.Mode().CancelInteraction();
        if (selectedNotes || interaction)
        {
            state_.SelectNotes({});
            state_.RequestRebuild();
        }
        else if (!state_.Document().Dirty() ||
                 MessageBoxW(GetActiveWindow(), Texts().discardChanges.data(),
                             std::format(L"{} {}", GameName, Texts().editorTitleSuffix).c_str(),
                             MB_YESNO | MB_ICONQUESTION) == IDYES)
            leave = true;
    }
    const bool controlDown = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (input.WasKeyPressed(VK_DELETE))
        state_.DeleteSelection();
    if (!controlDown && !input.IsKeyDown(VK_LMENU) && !input.IsKeyDown(VK_RMENU))
    {
        if (input.WasKeyPressed('Z'))
            state_.SeekFirstNote();
        if (input.WasKeyPressed('X'))
            state_.SeekLastNote();
    }
    if (state_.Tab() == EditorTab::Pattern && !controlDown && !input.IsKeyDown(VK_LMENU) && !input.IsKeyDown(VK_RMENU))
        state_.Mode().HandleToolKeys(state_, input);
    if (input.WasKeyPressed(VK_LEFT))
    {
        state_.Seek(state_.TimeMilliseconds() - 1);
    }
    if (input.WasKeyPressed(VK_RIGHT))
    {
        state_.Seek(state_.TimeMilliseconds() + 1);
    }

    return leave;
}

void EditorView::UpdatePointer(const UpdateContext &context)
{
    const auto &input = context.input;
    toggleSelection_ = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    rangeSelection_ = input.IsKeyDown(VK_LSHIFT) || input.IsKeyDown(VK_RSHIFT);
    const float scale = std::max(.001F, std::min(width / 1920.0F, height / 1080.0F));
    const Point point{(input.MousePositionX() - width * .5F) / scale + 960,
                         (input.MousePositionY() - height * .5F) / scale + 540};
    UpdateSliders(context);
    if (state_.Tab() == EditorTab::Pattern &&
        state_.Mode().UpdateScoreSelection(state_, point, !input.IsMouseButtonDown(MouseButton::Left)))
        return;
    if (input.WasMouseButtonPressed(MouseButton::Right) && state_.Tab() == EditorTab::Pattern &&
        !buildFailed_)
    {
        if (state_.Mode().OpenToolMenu(point))
        {
            state_.RequestRebuild();
        }
        else
            state_.Mode().EditScore(state_, point, true);
    }
    if (input.WasMouseButtonPressed(MouseButton::Left) &&
        sliderInput_.CapturedNode() != divisionSliderNode_->Id() &&
        sliderInput_.CapturedNode() != timelineSliderNode_->Id() &&
        sliderInput_.CapturedNode() != playbackRateSliderNode_->Id())
    {
        auto found = std::find_if(controls.rbegin(), controls.rend(),
                                  [point](const Control &c) { return c.rect.Contains(point); });
        if (found != controls.rend())
        {
            auto action = found->click;
            action();
        }
        else if (state_.Tab() == EditorTab::Pattern && !buildFailed_)
        {
            if (!state_.Mode().BeginScoreSelection(state_, point))
                state_.Mode().EditScore(state_, point, false);
        }
    }
}

void EditorView::UpdateTimelineNavigation(const InputState &input, bool keyboardAllowed, bool pointerAllowed)
{
    const bool modified = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL) ||
                          input.IsKeyDown(VK_LMENU) || input.IsKeyDown(VK_RMENU);
    if (keyboardAllowed && !modified && input.WasKeyPressed(VK_SPACE))
        state_.TogglePlayback();
    if (!pointerAllowed || input.MouseWheelDelta() == 0)
        return;
    const int direction = input.MouseWheelDelta() > 0 ? -1 : 1;
    if (state_.Tab() == EditorTab::Audio)
        state_.Seek(StepEditorTimeline(state_.Document().Timeline(), state_.TimeMilliseconds(),
                                      state_.Score().division, direction).count() / 1000.0);
    else if (state_.Tab() == EditorTab::Pattern)
        state_.Mode().ScrollScore(state_, direction);
    else
        state_.listOffset = static_cast<std::size_t>(
            std::max<std::int64_t>(0, static_cast<std::int64_t>(state_.listOffset) + direction));
    state_.RequestRebuild();
}

void EditorView::ExecuteMenuAction(EditorMenuAction action)
{
    switch (action)
    {
    case EditorMenuAction::Save: state_.Save(); break;
    case EditorMenuAction::Undo: state_.Undo(); break;
    case EditorMenuAction::Redo: state_.Redo(); break;
    case EditorMenuAction::Copy: state_.CopySelection(); break;
    case EditorMenuAction::Paste: state_.PasteSelection(false); break;
    case EditorMenuAction::PasteWithBlanks: state_.PasteSelection(true); break;
    case EditorMenuAction::OpenYmmFolder: OpenDocumentFolder(state_.MusicMetadataPath()); break;
    case EditorMenuAction::OpenYmpFolder: OpenDocumentFolder(state_.Document().SourcePath()); break;
    case EditorMenuAction::OpenYmeFolder: OpenDocumentFolder(state_.Document().Effects().sourcePath); break;
    }
}

void EditorView::OpenDocumentFolder(const std::filesystem::path &path)
{
    if (path.empty())
        throw std::runtime_error("The current document has no file location.");
    const auto directory = std::filesystem::absolute(path).parent_path();
    if (!std::filesystem::is_directory(directory))
        throw std::runtime_error("The current document folder does not exist.");
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(GetActiveWindow(), L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
        throw std::runtime_error("Could not open the document folder (ShellExecute error " + std::to_string(result) + ").");
}
