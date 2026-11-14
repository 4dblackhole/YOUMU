#pragma once
#include "PhantomCore.h"
#include <array>
#include <cstddef>
#include <optional>
#include <vector>
namespace taiko_result_ui
{
    // Judgement share ring around the rank. Slices start at 12 o'clock and run
    // clockwise in the order MAX, PERFECT, GREAT, GOOD, BAD, OK, MISS, POOR.
    inline constexpr std::size_t RingJudgementCount = 8;
    struct RingSlice
    {
        std::size_t judgement{};
        std::size_t count{};
        // Fractions of one clockwise turn from 12 o'clock; start < end.
        double start{}, end{};
    };
    struct JudgementRing
    {
        PhantomCore::Point center{};
        float innerRadius{}, outerRadius{};
        std::vector<RingSlice> slices;
        // Pixel-row spans of the band, coloured by slice. Empty without counts.
        std::vector<PhantomCore::RectanglePrimitive> rectangles;
    };
    // rowHeight is the Canvas length of one physical pixel, so the rows stay
    // one pixel tall at every resolution. The geometry is built once per layout.
    JudgementRing BuildJudgementRing(const std::array<std::size_t, RingJudgementCount> &counts,
                                     const std::array<PhantomCore::Color, RingJudgementCount> &colors,
                                     PhantomCore::Point center, float innerRadius, float outerRadius,
                                     float rowHeight);
    // The slice under a Canvas point, or nothing outside the band.
    std::optional<RingSlice> RingSliceAt(const JudgementRing &ring, PhantomCore::Point point);
} // namespace taiko_result_ui
