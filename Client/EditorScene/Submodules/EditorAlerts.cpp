#include "EditorPatternCreation.h"
#include "Presentation/SceneWheel.h"
#include "EditorView.h"
using namespace PhantomCore;

using namespace editor_ui;

namespace
{
    // Buttons of the unsaved-changes alert, left to right.
    enum LeaveButton : std::size_t
    {
        Discard,
        Cancel,
        Save
    };
} // namespace

void EditorView::ShowAlert(AlertContent content, AlertPurpose purpose)
{
    // A drag or typing in progress ends; the editor behind the alert takes no input.
    if (auto rejected = fields_.BlurAll())
        state_.SetStatus(*rejected);
    sliderInput_.Reset(*canvas.Get());
    alertPurpose_ = purpose;
    alert_.Show(std::move(content));
    state_.RequestRebuild();
}

void EditorView::ShowFailure(AlertKind kind, std::string_view titleKey, std::string_view messageKey, std::string detail)
{
    ShowAlert({kind, std::wstring(texts_.Get(titleKey)), std::wstring(texts_.Get(messageKey)), Wide(detail),
               {{std::wstring(texts_.Get("Alert.close")), true, false, true}}},
              AlertPurpose::Notice);
}

void EditorView::ConfirmLeave()
{
    // Discard stands apart at the left; Esc cancels and Enter saves.
    ShowAlert({AlertKind::Warning, std::wstring(texts_.Get("Editor.unsavedTitle")), std::wstring(Texts().discardChanges),
               {},
               {{std::wstring(texts_.Get("Editor.discard")), false, true},
                {std::wstring(texts_.Get("Editor.cancel")), false, false, true},
                {std::wstring(Texts().save), true}}},
              AlertPurpose::Leave);
}

bool EditorView::UpdateAlert(const InputState &input)
{
    auto &target = *canvas.Get();
    PointerInput pointer{};
    const auto mapped =
        MapScreenPointer({static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                         {static_cast<float>(width), static_cast<float>(height)}, target);
    pointer.available = input.IsMouseInsideWindow() && mapped.has_value();
    pointer.position = mapped.value_or(Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(MouseButton::Left);
    pointer.wheelDelta = SceneWheelDelta(input);
    alertInput_.Process(target, pointer);
    static_cast<void>(target.TakeActions());
    const auto purpose = alertPurpose_;
    const auto chosen = alert_.Update(input);
    if (alert_.IsOpen())
        return false;
    alertInput_.Reset(target);
    state_.RequestRebuild();
    if (!chosen || purpose != AlertPurpose::Leave)
        return false;
    if (*chosen == Discard)
        return true;
    // A failed save opens its own alert and the editor stays.
    return *chosen == Save && SaveDocument();
}

bool EditorView::SaveDocument()
{
    const bool creating = state_.Creating();
    try
    {
        state_.Save();
        return true;
    }
    catch (const Error &error)
    {
        // A name or folder the user can change is a field problem, not a stopped operation.
        const bool field = error.reason != Failure::InvalidMetadata;
        if (field)
        {
            const auto key = error.reason == Failure::ExistingFolder ? "MusicSelect.createDifficultyExists"
                                                                     : "MusicSelect.createDifficultyNameError";
            state_.SetStatus(Utf8(std::wstring(texts_.Get(key))) + " (" + error.what() + ")");
            return false;
        }
        ShowFailure(AlertKind::Error, "Editor.createFailedTitle", "MusicSelect.createDifficultyError", error.what());
    }
    catch (const std::exception &error)
    {
        const auto path = state_.Document().SourcePath();
        std::string detail = path.empty() ? std::string{} : Utf8(path.wstring()) + "\n";
        ShowFailure(AlertKind::Error, creating ? "Editor.createFailedTitle" : "Editor.saveFailedTitle",
                    creating ? "MusicSelect.createDifficultyError" : "Editor.saveFailedMessage", detail + error.what());
    }
    return false;
}

void EditorView::Paste(bool includeBlanks)
{
    try
    {
        state_.PasteSelection(includeBlanks);
    }
    catch (const std::exception &error)
    {
        // The copied block is all-or-nothing, so nothing changed.
        ShowFailure(AlertKind::Warning, "Editor.pasteFailedTitle", "Editor.pasteFailedMessage", error.what());
    }
}
