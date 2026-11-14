#pragma once
#include "PhantomCore.h"

// Shared UI colors. Scene layouts map these tokens to their local roles;
// note/judgement colors remain owned by their gameplay presentation policy.
namespace ui_colors
{
    constexpr PhantomCore::Color Rgb(unsigned rgb, float alpha = 1)
    {
        return {static_cast<float>((rgb >> 16) & 255) / 255, static_cast<float>((rgb >> 8) & 255) / 255,
                static_cast<float>(rgb & 255) / 255, alpha};
    }
    inline constexpr PhantomCore::Color White{1, 1, 1, 1}, Ink{.08F, .28F, .52F, 1}, Error{.75F, .17F, .21F, 1};
    inline constexpr auto ClearBackground = Rgb(0xF0F8FF);
    // Redesign palette (UI-02). Cobalt with a hatch texture is the base surface,
    // navy the darkest one, and dawn gold marks selection only.
    namespace theme
    {
        inline constexpr auto Brand = Rgb(0x1F4C99), BrandDeep = Rgb(0x142F5E), BrandText = Rgb(0x1D4590);
        inline constexpr auto Sun = Rgb(0xF2B23A);
        inline constexpr auto Ink = Rgb(0x16294A), InkSoft = Rgb(0x2A4269), Caption = Rgb(0x4B6184);
        inline constexpr auto Rule = Rgb(0x3F6FA8, .2F), RuleStrong = Rgb(0x3F6FA8, .38F);
        inline constexpr auto Plot = Rgb(0xFFFFFF, .62F), Grid = Rgb(0xE3EEF8), Axis = Rgb(0x0F2A47);
        inline constexpr auto Chip = Rgb(0xFFFFFF, .8F), Badge = Rgb(0xE6EEFC);
        inline constexpr auto Ok = Rgb(0x17794F), Danger = Rgb(0xB42340), DangerSurface = Rgb(0xFDECEF);
        inline constexpr auto Curve = Rgb(0x3570C8), CurveFill = Rgb(0x2F8EE5, .16F), ClearLine = Rgb(0x33598A);
        inline constexpr auto Letterbox = Rgb(0x04070D);
    } // namespace theme
    namespace sky
    {
        inline constexpr PhantomCore::Color Background{.918F, .965F, 1, 1}, Surface{.976F, .992F, 1, .98F};
        inline constexpr PhantomCore::Color Accent{.310F, .624F, .878F, 1}, AccentHover{.365F, .690F, .930F, 1};
        inline constexpr PhantomCore::Color Border{.569F, .769F, .890F, 1}, PaleBorder{.725F, .851F, .929F, 1};
        inline constexpr PhantomCore::Color Pale{.937F, .973F, .996F, 1}, Viewport{.933F, .973F, .992F, .62F};
        inline constexpr PhantomCore::Color Focus{.906F, .965F, 1, 1}, Row{.929F, .973F, .996F, 1};
        inline constexpr PhantomCore::Color Ink{.157F, .325F, .455F, 1}, Muted{.290F, .471F, .592F, 1};
        inline constexpr PhantomCore::Color SoftText{.455F, .659F, .784F, 1}, Disabled{.70F, .78F, .84F, .55F};
    } // namespace sky
    namespace logo
    {
        inline constexpr PhantomCore::Color Selected{.22F, .56F, .88F, .94F}, SelectedHover{.30F, .66F, .96F, 1};
        inline constexpr PhantomCore::Color SelectedPressed{.12F, .39F, .72F, 1}, SelectedDisabled{.20F, .35F, .52F, .55F};
        inline constexpr PhantomCore::Color Button{.78F, .89F, .98F, .88F}, Pressed{.20F, .48F, .78F, 1};
        inline constexpr PhantomCore::Color Disabled{.55F, .64F, .72F, .50F};
    } // namespace logo
    namespace options
    {
        // Reuse the existing sky border shade as a stronger panel surface.
        inline constexpr auto Background = sky::PaleBorder;
        inline constexpr auto Field = sky::Pale, Hover = sky::Focus, Pressed = sky::Accent;
        inline constexpr auto Disabled = sky::Disabled, Label = sky::Ink;
    } // namespace options
    namespace editor
    {
        inline constexpr PhantomCore::Color Background{.914F, .961F, 1, 1}, Paper{.973F, .988F, 1, 1};
        inline constexpr PhantomCore::Color Ink{.17F, .34F, .47F, 1}, Accent{.31F, .62F, .88F, 1}, Pale{.82F, .91F, .96F, 1};
    } // namespace editor
    namespace result
    {
        inline constexpr auto Banner = Rgb(0x4B7897);
        inline constexpr auto Ink = Rgb(0x285374), Muted = Rgb(0x5783A0), Accent = Rgb(0x4F9FDF);
        inline constexpr auto Surface = Rgb(0xF9FDFF), Plot = Rgb(0xEAF6FF), Keyline = Rgb(0xBEDFF3);
    } // namespace result
    namespace pause
    {
        inline constexpr auto Ink = result::Ink, Surface = result::Surface, Focus = result::Accent;
        inline constexpr auto Keyline = result::Keyline, Selector = result::Plot;
        inline constexpr auto Hover = sky::AccentHover, Pressed = sky::Ink, Disabled = sky::Disabled;
    } // namespace pause
} // namespace ui_colors
