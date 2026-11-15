#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <charconv>

using namespace PhantomCore;

using namespace options_ui;
void OptionsPanel::CreateOffsetControls()
{
    // A short number box followed by its unit.
    constexpr float FieldBoxWidth = 112;
    const float top = NextField(ControlHeight);
    offsetLabel_ = &CreateFieldLabel(*content_, top, "Options.Offset.Label");
    offsetField_ = &CreateButton(*content_,
                                 TopLeft({ContentLeft, top + ControlOffset, FieldBoxWidth, ControlHeight}, contentHeight_),
                                 L"", "Options.Offset.Field");
    StyleFieldButton(*offsetField_, true);
    AddFieldLine(*content_, *offsetField_, "Options.Offset.Line");
    offsetUnit_ = &CreateCaption(*content_, {ContentLeft + FieldBoxWidth + 8, top + ControlOffset, 40, ControlHeight}, 13,
                                 400, ui_colors::theme::Caption, "Options.Offset.Unit");
}
void OptionsPanel::RefreshOffsetControls()
{
    SetCaption(*offsetLabel_, std::wstring(Options(texts_).globalOffset));
    SetCaption(*offsetUnit_, L"ms");
    RequireComponent<TextVisualComponent>(*offsetField_)
        .SetText(editingOffset_ ? offsetDraft_ + L"_" : std::to_wstring(options_.Values().globalOffsetMs));
}
void OptionsPanel::CancelKeyboardEdit()
{
    captureRow_.reset();
    editingOffset_ = false;
    replaceOffsetDraft_ = false;
    if (offsetField_)
        RefreshOffsetControls();
    if (!keyRows_.empty())
        RefreshKeyControls();
}
void OptionsPanel::ApplyOffset()
{
    std::string draft;
    draft.reserve(offsetDraft_.size());
    for (const auto character : offsetDraft_)
        draft.push_back(static_cast<char>(character)); // Numeric ASCII draft only.
    std::int32_t value{};
    const auto parsed = std::from_chars(draft.data(), draft.data() + draft.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != draft.data() + draft.size())
    {
        status_ = Status::InvalidInput;
        statusDetail_ = "Offset must be an integer number of milliseconds within int32 range.";
    }
    else
    {
        auto candidate = options_.Values();
        candidate.globalOffsetMs = value;
        if (SaveOptions(candidate))
            editingOffset_ = false;
    }
    RefreshStatus();
}
void OptionsPanel::EditOffset(std::uint16_t key)
{
    if (key == VK_RETURN)
        ApplyOffset();
    else if (key == VK_BACK)
    {
        if (replaceOffsetDraft_)
            offsetDraft_.clear();
        else if (!offsetDraft_.empty())
            offsetDraft_.pop_back();
        replaceOffsetDraft_ = false;
    }
    else if (key == VK_DELETE)
    {
        offsetDraft_.clear();
        replaceOffsetDraft_ = false;
    }
    else
    {
        wchar_t character = 0;
        if (key >= '0' && key <= '9')
            character = static_cast<wchar_t>(key);
        else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
            character = static_cast<wchar_t>('0' + key - VK_NUMPAD0);
        else if (key == VK_OEM_MINUS || key == VK_SUBTRACT)
            character = L'-';
        if (character)
        {
            if (replaceOffsetDraft_)
            {
                offsetDraft_.clear();
                replaceOffsetDraft_ = false;
            }
            if (offsetDraft_.size() < 11 && (character != L'-' || offsetDraft_.empty()))
                offsetDraft_ += character;
        }
    }
    RefreshOffsetControls();
}
