#pragma once
#include <algorithm>
#include <cstdint>
#include <span>

namespace taiko_editor_layout
{
    // The score view lays four measures per row; the row count follows the area's height.
    inline constexpr int MeasuresPerRow = 4;

    // The first measure of the score row holding a measure. Rows start at measure 0 and at each YMP system
    // break (---, sorted next-measure numbers), then every MeasuresPerRow measures.
    inline std::int64_t ScoreRowStart(std::span<const std::int64_t> breaks, std::int64_t measure)
    {
        const auto after = std::ranges::upper_bound(breaks, measure);
        const std::int64_t start = after == breaks.begin() ? 0 : std::max<std::int64_t>(0, *std::prev(after));
        return start + (measure - start) / MeasuresPerRow * MeasuresPerRow;
    }

    // The first measure of the row after the one starting at rowStart.
    inline std::int64_t NextScoreRowStart(std::span<const std::int64_t> breaks, std::int64_t rowStart)
    {
        const auto next = std::ranges::upper_bound(breaks, rowStart);
        const std::int64_t full = rowStart + MeasuresPerRow;
        return next == breaks.end() ? full : std::min(full, *next);
    }
} // namespace taiko_editor_layout
