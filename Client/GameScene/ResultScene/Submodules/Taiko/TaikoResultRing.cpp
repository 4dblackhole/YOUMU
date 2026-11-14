#include "TaikoResultRing.h"
#include <algorithm>
#include <cmath>
#include <numbers>

using namespace PhantomCore;

namespace
{
    constexpr double Turn = 2.0 * std::numbers::pi;

    // Clockwise turn fraction from 12 o'clock on a Y-up Canvas.
    double TurnAt(double x, double y)
    {
        const double turn = std::atan2(x, y) / Turn;
        return turn < 0 ? turn + 1.0 : turn;
    }
    const taiko_result_ui::RingSlice *SliceAt(const std::vector<taiko_result_ui::RingSlice> &slices, double turn)
    {
        for (const auto &slice : slices)
            if (turn >= slice.start && turn < slice.end)
                return &slice;
        // A full ring ends at exactly 1.0; rounding may land just past the last boundary.
        return !slices.empty() && turn >= slices.back().start ? &slices.back() : nullptr;
    }
} // namespace

namespace taiko_result_ui
{
    JudgementRing BuildJudgementRing(const std::array<std::size_t, RingJudgementCount> &counts,
                                     const std::array<Color, RingJudgementCount> &colors, Point center,
                                     float innerRadius, float outerRadius, float rowHeight)
    {
        JudgementRing ring;
        ring.center = center;
        ring.innerRadius = innerRadius;
        ring.outerRadius = outerRadius;
        std::size_t total = 0;
        for (const auto count : counts)
            total += count;
        if (total == 0 || !(outerRadius > innerRadius) || !(innerRadius >= 0) || !(rowHeight > 0))
            return ring;

        std::size_t accumulated = 0;
        for (std::size_t i = 0; i < counts.size(); ++i)
        {
            if (counts[i] == 0)
                continue;
            const double start = static_cast<double>(accumulated) / static_cast<double>(total);
            accumulated += counts[i];
            ring.slices.push_back({i, counts[i], start, static_cast<double>(accumulated) / static_cast<double>(total)});
        }

        // Each pixel row crosses the band in a left and a right span. A slice
        // boundary is a ray from the centre, so it cuts a row at most once per side.
        const double outer = outerRadius, inner = innerRadius;
        const auto rows = static_cast<std::size_t>(std::ceil(2.0 * outer / rowHeight));
        std::vector<double> cuts;
        for (std::size_t row = 0; row < rows; ++row)
        {
            const double bottom = -outer + static_cast<double>(row) * rowHeight;
            const double height = std::min<double>(rowHeight, outer - bottom);
            const double y = bottom + height * 0.5;
            const double outerHalf = std::sqrt(std::max(outer * outer - y * y, 0.0));
            const double innerHalf = std::abs(y) < inner ? std::sqrt(inner * inner - y * y) : 0.0;
            if (outerHalf <= innerHalf)
                continue;
            for (const double side : {-1.0, 1.0})
            {
                const double from = side < 0 ? -outerHalf : innerHalf;
                const double to = side < 0 ? -innerHalf : outerHalf;
                cuts.assign({from, to});
                if (ring.slices.size() > 1)
                    for (const auto &slice : ring.slices)
                    {
                        const double angle = slice.start * Turn;
                        const double directionY = std::cos(angle);
                        // The ray reaches this row only on its own side of the centre line.
                        if (directionY * y <= 0)
                            continue;
                        const double x = y * std::sin(angle) / directionY;
                        if (x > from && x < to)
                            cuts.push_back(x);
                    }
                std::ranges::sort(cuts);
                for (std::size_t i = 0; i + 1 < cuts.size(); ++i)
                {
                    const double left = cuts[i], right = cuts[i + 1];
                    if (right - left <= 0)
                        continue;
                    const auto *slice = SliceAt(ring.slices, TurnAt((left + right) * 0.5, y));
                    if (!slice)
                        continue;
                    ring.rectangles.push_back({{center.x + static_cast<float>(left), center.y + static_cast<float>(bottom),
                                                static_cast<float>(right - left), static_cast<float>(height)},
                                               colors[slice->judgement], 0.0F});
                }
            }
        }
        return ring;
    }

    std::optional<RingSlice> RingSliceAt(const JudgementRing &ring, Point point)
    {
        const double x = point.x - ring.center.x, y = point.y - ring.center.y;
        const double distance = std::hypot(x, y);
        if (distance < ring.innerRadius || distance > ring.outerRadius)
            return std::nullopt;
        const auto *slice = SliceAt(ring.slices, TurnAt(x, y));
        return slice ? std::optional{*slice} : std::nullopt;
    }
} // namespace taiko_result_ui
