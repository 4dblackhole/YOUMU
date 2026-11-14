#pragma once
#include "PhantomCore.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include <algorithm>
#include <array>
#include <stdexcept>

// Title screen (UI-02), authored in logical pixels on a 1280x720 screen with a top-left origin.
namespace logo_ui
{
    constexpr PhantomCore::Size ReferenceSize{1280.0F, 720.0F};
    constexpr float MaximumWidth = 2560.0F; // 32:9. Wider screens get side bands.
    constexpr PhantomCore::Size MenuButton{463.0F / 1.5F, 52.0F};
    constexpr float MenuTop = 344.0F;
    constexpr float MenuPitch = 64.0F;
    constexpr float MenuCenterOffsetX = 0.0F;
    constexpr float SelectedShift = 12.0F; // The chosen item slides to the right.
    constexpr PhantomCore::Size CursorSize{28.0F, 28.0F};
    constexpr float CursorLeft = 20.0F;
    constexpr PhantomCore::Size FeatherSize{12.0F, 34.0F};
    constexpr std::size_t FeatherCount = 7;
    constexpr std::size_t GameStartIndex = 0;
    constexpr std::size_t EditorIndex = 1;
    constexpr std::size_t OptionIndex = 2;
    constexpr std::size_t ExitIndex = 3;

    // The 16:9 arrangement is never squeezed: narrower screens scale it between bands.
    struct TitleFrame
    {
        float width{};
        float scale{};
    };
    [[nodiscard]] constexpr TitleFrame CalculateFrame(const float logicalWidth) noexcept
    {
        return {std::clamp(logicalWidth, ReferenceSize.width, MaximumWidth),
                std::min(1.0F, std::max(logicalWidth, 1.0F) / ReferenceSize.width)};
    }

    // Converts a top-left rectangle into the parent's lower-left, Y-up coordinates.
    [[nodiscard]] constexpr PhantomCore::Rect TopLeft(const PhantomCore::Rect bounds, const float parentHeight) noexcept
    {
        return {bounds.x, parentHeight - bounds.y - bounds.height, bounds.width, bounds.height};
    }

    template <typename ComponentType>
    [[nodiscard]] inline ComponentType &RequireComponent(PhantomCore::Visual2DNode &node)
    {
        ComponentType *component = node.GetComponent<ComponentType>();
        if (component == nullptr)
        {
            throw std::logic_error("The title menu node is missing a component.");
        }
        return *component;
    }

    // The skin supplies the slanted bar; without it the earlier flat colours remain.
    inline void ApplyButtonSelectionStyle(PhantomCore::Visual2DNode &button, PhantomCore::ScreenVisual2DManager &visuals,
                                          const bool selected)
    {
        PhantomCore::VisualStyle style{};
        if (selected)
        {
            style.normal = style.hovered = ui_colors::theme::Brand;
            style.pressed = ui_colors::theme::BrandDeep;
            style.disabled = ui_colors::logo::SelectedDisabled;
        }
        else
        {
            style.normal = style.hovered = PhantomCore::Color{0.941F, 0.973F, 1.0F, 0.86F};
            style.pressed = ui_colors::logo::Pressed;
            style.disabled = ui_colors::logo::Disabled;
        }
        SetSkinBaseStyle(button, style);
        // The bar image carries its own opacity; the fallback colour must not thin it again.
        BindSkinPart(button, visuals, "Logo", selected ? "Menu.ButtonSelected" : "Menu.Button");
        RequireComponent<PhantomCore::TextVisualComponent>(button).SetTextColor(selected ? ui_colors::White
                                                                                           : ui_colors::theme::Ink);
    }
} // namespace logo_ui
