#pragma once
#include "PhantomCore.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiDropdown.h"
#include "Presentation/UiFonts.h"
#include "Presentation/UiTextWrap.h"
#include "Texts/Options/OptionTexts.h"
#include <Windows.h>
#include <array>
#include <stdexcept>

// Drawer geometry and styling (UI-02 options panel), in 1280x720 logical units.
// Layout rectangles are written from the top-left of their parent and converted
// with TopLeft, because Visual2D places children from the parent's bottom-left.
namespace options_ui
{
    constexpr float PanelWidth = 432, PanelHeight = 720;
    constexpr float RailWidth = 64, RailExpandedWidth = 184;
    constexpr float RailButtonTop = 8, RailButtonHeight = 60, RailIconSize = 24;
    constexpr float BodyLeft = RailWidth, BodyWidth = PanelWidth - RailWidth;
    constexpr float HeaderHeight = 68, FooterMinimumHeight = 72;
    constexpr float ContentLeft = 24, FieldWidth = 308, ScrollbarWidth = 12;
    constexpr float SectionPaddingTop = 22, SectionHeaderHeight = 26, SectionPaddingBottom = 28;
    constexpr float FieldGap = 16, LabelHeight = 18, ControlOffset = 24, ControlHeight = 40;
    constexpr float KeyCardHeight = 64;
    // Inline slider rows (the three volumes): caption, track and value on one 36-tall row each.
    constexpr float InlineRowHeight = 36;
    // Screen position: the monitor box, a gap, then the X, Y and Center row.
    constexpr float PositionBoxHeight = 120, PositionRowGap = 8;
    constexpr float PositionControlHeight = PositionBoxHeight + PositionRowGap + ControlHeight;

    constexpr auto DrawerColor = ui_colors::Rgb(0xF3F8FE);
    constexpr auto RailColor = ui_colors::theme::BrandDeep;
    constexpr auto FieldColor = ui_colors::Rgb(0xFFFFFF, .74F), FieldHoverColor = ui_colors::Rgb(0xFFFFFF);
    constexpr auto FieldPressedColor = ui_colors::Rgb(0xE6EEFC), FieldDisabledColor = ui_colors::Rgb(0xFFFFFF, .4F);
    constexpr auto FieldLineColor = ui_colors::Rgb(0xB5CFF5);
    constexpr auto RailIdleColor = ui_colors::Rgb(0xFFFFFF, .62F);
    constexpr auto WarningColor = ui_colors::Rgb(0x6E4700);
    constexpr auto ScrimColor = ui_colors::Rgb(0x142F5E, .42F);
    constexpr auto ModalColor = ui_colors::Rgb(0xF0F8FF, .98F);
    constexpr auto HeadingColor = ui_colors::theme::Ink, LabelColor = ui_colors::theme::Ink;

    [[nodiscard]] constexpr PhantomCore::Rect TopLeft(const PhantomCore::Rect bounds, const float parentHeight) noexcept
    {
        return {bounds.x, parentHeight - bounds.y - bounds.height, bounds.width, bounds.height};
    }

    template <typename Component> [[nodiscard]] Component &RequireComponent(PhantomCore::Visual2DNode &node)
    {
        Component *component = node.GetComponent<Component>();
        if (component == nullptr)
        {
            throw std::logic_error("The options panel is missing a required component.");
        }
        return *component;
    }

    inline void SetColors(PhantomCore::Visual2DNode &node, const PhantomCore::Color normal,
                          const PhantomCore::Color hovered, const PhantomCore::Color pressed,
                          const PhantomCore::Color disabled)
    {
        PhantomCore::VisualStyle style{};
        style.normal = normal;
        style.hovered = hovered;
        style.pressed = pressed;
        style.disabled = disabled;
        RequireComponent<PhantomCore::SpriteVisualComponent>(node).SetStyle(style);
    }
    inline void SetPanelColor(PhantomCore::Visual2DNode &node, const PhantomCore::Color color)
    {
        SetColors(node, color, color, color, color);
    }
    // Recolours a white skin icon and keeps its image; an icon the skin lacks stays transparent
    // instead of becoming a square.
    inline void TintIcon(PhantomCore::Visual2DNode &icon, const PhantomCore::Color colour)
    {
        auto &sprite = RequireComponent<PhantomCore::SpriteVisualComponent>(icon);
        auto style = sprite.Style();
        if (style.normal.alpha <= 0)
            return;
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        sprite.SetStyle(style);
    }
    // White field with the redesign's hover and press states.
    inline void SetFieldColors(PhantomCore::Visual2DNode &node)
    {
        SetColors(node, FieldColor, FieldHoverColor, FieldPressedColor, FieldDisabledColor);
    }

    // Caption text: Nunito, or Noto Sans KR when the string holds Hangul/Kana/Han.
    inline void StyleCaption(PhantomCore::TextVisualComponent &text, const float size, const std::uint16_t weight,
                             const PhantomCore::Color color,
                             const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading)
    {
        text.SetFont(ui_fonts::Text(text.Text()));
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(color);
        text.SetHorizontalAlignment(alignment);
    }
    // Number text: Barlow Semi Condensed.
    inline void StyleNumber(PhantomCore::TextVisualComponent &text, const float size, const std::uint16_t weight,
                            const PhantomCore::Color color,
                            const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading,
                            const bool italic = true)
    {
        auto face = ui_fonts::Number(weight, italic);
        text.SetFont(std::move(face.font));
        text.SetFontSize(size);
        text.SetFontWeight(face.weight);
        text.SetTextColor(color);
        text.SetHorizontalAlignment(alignment);
    }
    // Replaces caption text and re-selects the face, because the new string may need CJK glyphs.
    inline void SetCaption(PhantomCore::Visual2DNode &node, std::wstring value)
    {
        auto &text = RequireComponent<PhantomCore::TextVisualComponent>(node);
        text.SetFont(ui_fonts::Text(value));
        text.SetText(std::move(value));
    }
    // Notices are broken to a width by the shared helper (Presentation/UiTextWrap.h).
    using ui_text::WrapText;

    [[nodiscard]] inline PhantomCore::Visual2DNode &CreateCaption(PhantomCore::Visual2DNode &parent,
                                                                  const PhantomCore::Rect topLeftBounds,
                                                                  const float size, const std::uint16_t weight,
                                                                  const PhantomCore::Color color, std::string name)
    {
        auto &label = PhantomCore::CreateLabel(parent, TopLeft(topLeftBounds, parent.NodeSize().height), L"",
                                               std::move(name));
        StyleCaption(RequireComponent<PhantomCore::TextVisualComponent>(label), size, weight, color);
        return label;
    }
    // A field caption: 13px semibold ink above its control.
    [[nodiscard]] inline PhantomCore::Visual2DNode &CreateFieldLabel(PhantomCore::Visual2DNode &parent,
                                                                     const float top, std::string name)
    {
        return CreateCaption(parent, {ContentLeft, top, FieldWidth, LabelHeight}, 13, 600, LabelColor,
                             std::move(name));
    }
    // A clickable field (path or number box): text from the left edge, clipped to the box.
    inline void StyleFieldButton(PhantomCore::Visual2DNode &node, const bool number)
    {
        SetFieldColors(node);
        auto &text = RequireComponent<PhantomCore::TextVisualComponent>(node);
        if (number)
            StyleNumber(text, 15, 700, HeadingColor, PhantomCore::TextAlignment::Leading, false);
        else
            StyleCaption(text, 14, 600, HeadingColor);
        const auto size = node.NodeSize();
        text.SetContentBounds({12, 0, size.width - 24, size.height});
        node.SetClipRect({0, 0, size.width, size.height});
    }
    // The 2px line under an input box; a sibling drawn just above the box.
    inline void AddFieldLine(PhantomCore::Visual2DNode &parent, const PhantomCore::Visual2DNode &field,
                             const std::string &name)
    {
        const auto bounds = field.Bounds();
        auto &line = PhantomCore::CreateSprite(parent, {bounds.x, bounds.y, bounds.width, 2}, {}, name);
        SetPanelColor(line, FieldLineColor);
        line.SetZIndex(field.ZIndex() + 1);
    }

    // Drop-down boxes are white fields; their value and list style come from UiDropdown.
    inline void ConfigureCombo(PhantomCore::Visual2DNode &node)
    {
        SetFieldColors(node);
    }

    inline std::wstring WideText(const std::string &value)
    {
        if (value.empty())
            return {};
        const int length =
            MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length);
        return result;
    }

    inline std::wstring OutputName(PhantomCore::AudioOutputBackend output, const OptionTextSet &text)
    {
        switch (output)
        {
        case PhantomCore::AudioOutputBackend::Automatic:
            return std::wstring(text.automatic);
        case PhantomCore::AudioOutputBackend::Wasapi:
            return std::wstring(text.outputWasapi);
        case PhantomCore::AudioOutputBackend::Asio:
            return std::wstring(text.outputAsio);
        default:
            return std::wstring(text.noSound);
        }
    }

    // One key of a whole-number draft (offset, screen position): digits, a leading minus, Backspace and
    // Delete. The first key after the field was clicked replaces the shown value.
    inline void EditNumberDraft(std::wstring &draft, bool &replace, const std::uint16_t key)
    {
        if (key == VK_BACK)
        {
            if (replace)
                draft.clear();
            else if (!draft.empty())
                draft.pop_back();
            replace = false;
            return;
        }
        if (key == VK_DELETE)
        {
            draft.clear();
            replace = false;
            return;
        }
        wchar_t character = 0;
        if (key >= '0' && key <= '9')
            character = static_cast<wchar_t>(key);
        else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
            character = static_cast<wchar_t>('0' + key - VK_NUMPAD0);
        else if (key == VK_OEM_MINUS || key == VK_SUBTRACT)
            character = L'-';
        if (!character)
            return;
        if (replace)
        {
            draft.clear();
            replace = false;
        }
        if (draft.size() < 11 && (character != L'-' || draft.empty()))
            draft += character;
    }

    constexpr std::array Outputs{PhantomCore::AudioOutputBackend::Automatic, PhantomCore::AudioOutputBackend::Wasapi,
                                 PhantomCore::AudioOutputBackend::Asio};
} // namespace options_ui
