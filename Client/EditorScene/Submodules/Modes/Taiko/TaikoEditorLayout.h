#pragma once
#include "PhantomCore.h"

namespace taiko_editor_layout
{
    inline constexpr int MeasuresPerRow = 4, VisibleRows = 5;
    inline constexpr float StaffHeight = 48;

    // Score rows inside the area the common view lends for the score view.
    struct ScoreGrid
    {
        float left{}, measureWidth{}, rowTop{}, rowHeight{};
        [[nodiscard]] float Right() const noexcept { return left + measureWidth * MeasuresPerRow; }
        [[nodiscard]] PhantomCore::Rect Bounds() const noexcept
        {
            return {left, rowTop, measureWidth * MeasuresPerRow, rowHeight * VisibleRows};
        }
    };
    [[nodiscard]] inline ScoreGrid ScoreGridFor(PhantomCore::Rect area) noexcept
    {
        return {area.x + 24, (area.width - 48) / MeasuresPerRow, area.y + 36, (area.height - 48) / VisibleRows};
    }
} // namespace taiko_editor_layout
