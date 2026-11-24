#include "EditorTextFields.h"
#include "EditorSupport.h"
#include "Presentation/NativeInputFonts.h"
#include "Presentation/UiColors.h"

using namespace PhantomCore;

void EditorTextFields::Begin() noexcept
{
    for (auto &[key, field] : fields_)
        field.declared = false;
}

void EditorTextFields::Declare(const std::string &key, TextInputBounds screen, std::uint32_t fontPixels,
                               const std::wstring &text, Commit commit, bool number, Commit typing)
{
    if (!window_)
        return;
    auto [entry, created] = fields_.try_emplace(key);
    auto &field = entry->second;
    if (created)
    {
        field.input = window_->CreateTextInput();
        if (!field.input)
            throw std::runtime_error("Cannot create an editor text field.");
        field.input->SetColors(0x16294A, 0xFFFFFF);
    }
    field.declared = true;
    field.commit = std::move(commit);
    field.typing = std::move(typing);
    field.input->SetBounds(screen);
    field.input->SetFont(number ? native_input_fonts::Number() : native_input_fonts::Text, fontPixels);
    // The document's value replaces the editor's text unless the user is typing in it.
    if (field.shown != text && !field.input->HasFocus())
    {
        field.input->SetText(text);
        field.typed = text;
        field.edited = false;
    }
    field.shown = text;
    field.input->SetVisible(true);
}

void EditorTextFields::End() noexcept
{
    for (auto &[key, field] : fields_)
        if (!field.declared && field.input)
        {
            field.input->Blur();
            field.input->SetVisible(false);
            field.focused = false;
        }
}

std::optional<std::string> EditorTextFields::CommitField(Field &field)
{
    if (!field.edited || field.typed == field.shown)
        return std::nullopt;
    field.edited = false;
    try
    {
        field.commit(editor_ui::Utf8(field.typed));
        return std::nullopt;
    }
    catch (const std::exception &error)
    {
        // Keep the rejected text in the editor so it can be corrected.
        field.edited = true;
        return error.what();
    }
}

std::optional<std::string> EditorTextFields::Update()
{
    std::optional<std::string> error;
    for (auto &[key, field] : fields_)
    {
        if (!field.input || !field.declared)
            continue;
        std::wstring text;
        if (field.input->TakeText(text))
        {
            field.typed = std::move(text);
            field.edited = true;
            if (field.typing)
                field.typing(editor_ui::Utf8(field.typed));
        }
        if (field.input->TakeCancel())
        {
            field.input->SetText(field.shown);
            field.typed = field.shown;
            field.edited = false;
            field.input->Blur();
        }
        const bool focused = field.input->HasFocus();
        const bool left = field.focused && !focused;
        field.focused = focused;
        if (field.input->TakeSubmit() || left)
            if (auto failed = CommitField(field))
                error = std::move(failed);
    }
    return error;
}

bool EditorTextFields::Editing() const noexcept
{
    for (const auto &[key, field] : fields_)
        if (field.declared && field.input && field.input->HasFocus())
            return true;
    return false;
}

std::optional<std::string> EditorTextFields::BlurAll()
{
    std::optional<std::string> error;
    for (auto &[key, field] : fields_)
    {
        if (!field.input)
            continue;
        if (field.focused)
            if (auto failed = CommitField(field))
                error = std::move(failed);
        field.input->Blur();
        field.focused = false;
    }
    return error;
}

void EditorTextFields::Shutdown() noexcept
{
    fields_.clear();
    window_ = nullptr;
}
