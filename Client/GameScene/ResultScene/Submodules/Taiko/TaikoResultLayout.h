#pragma once
#include "PhantomCore.h"
#include "Presentation/UiColors.h"
#include <algorithm>
#include <array>
#include <format>
#include <string>
namespace taiko_result_ui::layout
{
    // The redesign is authored on a 1280x720 logical screen, top-left origin and
    // Y down. Wider screens only add width; the left column stays 340 wide and
    // the centre and right columns share the rest.
    inline constexpr float ReferenceWidth = 1280, Height = 720, MaximumWidth = 2560;
    inline constexpr float Margin = 32, ColumnGap = 24, LeftWidth = 340;
    inline constexpr float BannerHeight = 72, BottomBarHeight = 64, BodyTop = 96;

    constexpr PhantomCore::Color ResultColor(unsigned rgb, float alpha = 1)
    {
        return ui_colors::Rgb(rgb, alpha);
    }
    inline constexpr auto Ink = ui_colors::theme::Ink, InkSoft = ui_colors::theme::InkSoft;
    inline constexpr auto Caption = ui_colors::theme::Caption, BrandText = ui_colors::theme::BrandText;
    // Judgement colours keep their established meaning: MAX, PERFECT, GREAT, GOOD, BAD, OK, MISS, POOR.
    inline constexpr std::array<PhantomCore::Color, 8> Markers{ResultColor(0xFFFFFF), ResultColor(0x2DDFFF), ResultColor(0x28FF28),
                                                     ResultColor(0xFFBF20), ResultColor(0x4018FF), ResultColor(0x4F9FDF),
                                                     ResultColor(0x829AB3), ResultColor(0xFF3A26)};

    // Column geometry for one content width.
    struct Columns
    {
        float width{};
        float leftX{}, centerX{}, rightX{}, sideWidth{};
    };
    constexpr Columns ColumnsFor(float contentWidth)
    {
        const float side = (contentWidth - Margin * 2 - LeftWidth - ColumnGap * 2) * 0.5F;
        return {contentWidth, Margin, Margin + LeftWidth + ColumnGap, contentWidth - Margin - side, side};
    }
    // Top-left / Y-down design rectangle inside a root whose origin is its bottom-left.
    constexpr PhantomCore::Rect Bounds(PhantomCore::Rect source)
    {
        return {source.x, Height - source.y - source.height, source.width, source.height};
    }

    // Left column, relative to the content root.
    inline constexpr PhantomCore::Rect HpChart{32, 130, 340, 100};
    inline constexpr PhantomCore::Rect HpPlot{72, 144, 286, 72};
    // Centre ring: 344 across; slices fill radii 86..96 of the 100-unit ring drawing.
    inline constexpr float RingSize = 344, RingCenterY = 337;
    inline constexpr float RingInnerRadius = RingSize * 0.5F * 0.86F, RingOuterRadius = RingSize * 0.5F * 0.96F;
    // Right column plots, relative to the column's left edge and sized from its width.
    inline constexpr float HistogramTop = 214, ScatterTop = 441, ChartHeight = 171;
    constexpr PhantomCore::Rect HistogramChart(const Columns &c) { return {c.rightX, HistogramTop, c.sideWidth, ChartHeight}; }
    constexpr PhantomCore::Rect ScatterChart(const Columns &c) { return {c.rightX, ScatterTop, c.sideWidth, ChartHeight}; }
    constexpr PhantomCore::Rect HistogramPlot(const Columns &c)
    {
        return {c.rightX + 12, HistogramTop + 12, c.sideWidth - 24, ChartHeight - 22};
    }
    constexpr PhantomCore::Rect ScatterPlot(const Columns &c)
    {
        return {c.rightX + 12, ScatterTop + 10, c.sideWidth - 24, ChartHeight - 20};
    }

    // Layout-time width estimates for placing a neighbour right after a text run
    // (the level badge after a difficulty name, the percent sign after a number).
    // The renderer measures text only when drawing, so these use average advances.
    constexpr float EstimateTextWidth(std::wstring_view text, float size)
    {
        float width = 0;
        for (const wchar_t c : text)
            width += c >= 0x1100 ? size : c == L' ' ? size * 0.28F : size * 0.58F;
        return width;
    }
    constexpr float EstimateNumberWidth(std::wstring_view text, float size)
    {
        float width = 0;
        for (const wchar_t c : text)
            width += (c >= L'0' && c <= L'9') ? size * 0.44F
                     : (c == L'.' || c == L',' || c == L' ') ? size * 0.22F
                     : c == L'%' ? size * 0.76F : size * 0.5F;
        return width;
    }

    template <typename... Args> std::wstring Format(std::wstring_view value, Args &&...args)
    {
        return std::vformat(value, std::make_wformat_args(args...));
    }
} // namespace taiko_result_ui::layout
