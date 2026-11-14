#include "TaikoResultGraphs.h"
#include "Judgement/AccuracyRange.h"
#include "TaikoResultLayout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
using namespace PhantomCore;

namespace taiko_result_ui
{

    namespace
    {
        constexpr std::size_t GradeCount = AccuracyRange::BandCount;
        struct HistogramBin
        {
            std::array<std::size_t, GradeCount + 1> grades{};
            std::size_t total{};
        };
        using HistogramBins = std::map<std::int64_t, HistogramBin>;

        std::size_t GraphColorIndex(JudgementGrade grade) noexcept
        {
            const auto index = static_cast<std::size_t>(grade);
            return index < GradeCount ? index : GradeCount;
        }

        Color GraphColor(std::size_t index) noexcept
        {
            return index < GradeCount ? layout::Markers[index] : layout::Markers[6];
        }

        std::int64_t HistogramMillisecond(RhythmDuration error) noexcept
        {
            // Round symmetrically around zero without overflowing the us value
            // or losing its sub-millisecond precision in a floating conversion.
            const auto remainder = error.count() % 1000;
            return error.count() / 1000 + (remainder >= 500 ? 1 : remainder <= -500 ? -1 : 0);
        }

        void AppendHistogram(GraphData &data, const HistogramBins &bins, Rect histogram, float minimumBarWidth)
        {
            if (data.maximumBinCount == 0)
                return;
            const double binWidth = histogram.width / (2 * data.halfRangeMilliseconds);
            // Widen only the footprint at subpixel scales, never the 1ms
            // aggregation interval or its total. Keep edge bins inside the plot.
            const double barWidth = std::min<double>(histogram.width, std::max<double>(binWidth * .9, minimumBarWidth));
            const double inversePeak = 1.0 / static_cast<double>(data.maximumBinCount);
            for (const auto &[millisecond, bin] : bins)
            {
                const double center =
                    (static_cast<double>(millisecond) / data.halfRangeMilliseconds + 1) * .5 * histogram.width;
                const auto left =
                    static_cast<float>(std::clamp(center - barWidth / 2, 0.0, histogram.width - barWidth));
                std::size_t cumulativeCount{};
                // Preserve every recorded grade, including mixed judgement
                // windows in one bin. Peak scaling uses the whole bin total.
                for (std::size_t grade = 0; grade < bin.grades.size(); ++grade)
                {
                    const auto count = bin.grades[grade];
                    if (count == 0)
                        continue;
                    const double bottom = static_cast<double>(cumulativeCount) * inversePeak;
                    cumulativeCount += count;
                    const double top = static_cast<double>(cumulativeCount) * inversePeak;
                    data.rectangles.push_back(
                        {{histogram.x + left, histogram.y + static_cast<float>(histogram.height * bottom),
                          static_cast<float>(barWidth), static_cast<float>(histogram.height * (top - bottom))},
                         GraphColor(grade),
                         0});
                }
            }
        }
    } // namespace
    std::vector<RectanglePrimitive> BuildHpCurve(const PlayResultSummary &result, Rect plot)
    {
        std::vector<RectanglePrimitive> rectangles;
        if (!result.hpHistory || result.hpGauges.empty() || !result.hpGauges.front().applicable ||
            plot.width <= 0 || plot.height <= 0)
            return rectangles;
        const auto firstControl = std::ranges::find_if(result.controls, [](const auto &control)
            { return control.pauseMenuShown || control.type == PlayControlType::Rewind; });
        const double stopped = static_cast<double>((firstControl == result.controls.end()
            ? result.endTime : firstControl->from).count());
        double first = std::min(static_cast<double>(result.initialTime.count()), stopped);
        double last = std::max(static_cast<double>(result.endTime.count()), stopped);
        for (const auto &point : *result.hpHistory)
        {
            first = std::min(first, static_cast<double>(point.time.count()));
            last = std::max(last, static_cast<double>(point.time.count()));
        }
        last = std::max(first + 1'000.0, last);
        const auto position = [&](double time, double percent)
        {
            return Point{plot.x + static_cast<float>(std::clamp((time - first) / (last - first), 0.0, 1.0)) * plot.width,
                         plot.y + static_cast<float>(std::clamp(percent / 100.0, 0.0, 1.0)) * plot.height};
        };
        const auto segment = [&](Point from, Point to)
        {
            const auto parts = static_cast<unsigned>(std::max(1.0F, std::ceil(to.x - from.x)));
            for (unsigned part = 0; part < parts; ++part)
            {
                const float a = static_cast<float>(part) / static_cast<float>(parts);
                const float b = static_cast<float>(part + 1) / static_cast<float>(parts);
                const float x = from.x + (to.x - from.x) * a;
                const float y0 = from.y + (to.y - from.y) * a;
                const float y1 = from.y + (to.y - from.y) * b;
                const float thickness = std::min(2.0F, plot.height);
                const float left = std::clamp(x, plot.x, plot.x + std::max(plot.width - 1, 0.0F));
                const float bottom = std::clamp(std::min(y0, y1), plot.y, plot.y + plot.height - thickness);
                const float width = std::min(std::max((to.x - from.x) / static_cast<float>(parts), 1.0F),
                                             plot.x + plot.width - left);
                // A light wash under the line reads the area without hiding the grid.
                if (bottom > plot.y)
                    rectangles.push_back({{left, plot.y, width, bottom - plot.y}, ui_colors::theme::CurveFill, 0});
                rectangles.push_back({{left, bottom, width,
                                      std::min(std::max(std::abs(y1 - y0), thickness), plot.y + plot.height - bottom)},
                                      ui_colors::theme::Curve, 0});
            }
        };
        Point previous = position(first, 0);
        double percent = 0;
        for (const auto &point : *result.hpHistory)
        {
            if (!std::isfinite(point.percent) || static_cast<double>(point.time.count()) > stopped)
                continue;
            const auto next = position(static_cast<double>(point.time.count()), point.percent);
            if (next.x < previous.x) continue;
            segment(previous, next);
            previous = next;
            percent = point.percent;
        }
        segment(previous, position(stopped, percent));
        return rectangles;
    }

    GraphData BuildGraphs(const TaikoPlayResult &result, Rect scatter, Rect histogram,
                          float minimumBarWidth)
    {
        GraphData data;
        auto first = result.summary.initialTime;
        auto last = result.summary.endTime;
        for (const auto &sample : result.timingErrors)
        {
            first = std::min(first, sample.inputTime);
            last = std::max(last, sample.inputTime);
            data.halfRangeMilliseconds =
                std::max(data.halfRangeMilliseconds, std::abs(static_cast<double>(sample.error.count()) / 1000));
        }
        data.halfRangeMilliseconds = std::ceil(data.halfRangeMilliseconds / 10) * 10;
        data.startSeconds = static_cast<double>(first.count()) / 1'000'000;
        data.endSeconds = std::max(data.startSeconds + .001, static_cast<double>(last.count()) / 1'000'000);
        const Color grid = ui_colors::theme::Axis;
        data.rectangles.reserve(result.timingErrors.size() + 50);
        // The view owns decorative backgrounds/grid. These zero references
        // are part of interpreting the data range, followed by points/bins.
        data.rectangles.push_back({{scatter.x, scatter.y + scatter.height / 2 - .5F, scatter.width, 1}, grid, 0});
        data.rectangles.push_back({{histogram.x + histogram.width / 2 - .5F, histogram.y, 1, histogram.height}, grid, 0});
        // Store occupied 1ms bins only, so an outlier never allocates a dense
        // array proportional to the entire displayed error range.
        HistogramBins bins;
        for (const auto &sample : result.timingErrors)
        {
            const double seconds = static_cast<double>(sample.inputTime.count()) / 1'000'000;
            const double error = static_cast<double>(sample.error.count()) / 1000;
            const float x = static_cast<float>((seconds - data.startSeconds) / (data.endSeconds - data.startSeconds));
            const float y = static_cast<float>((error / data.halfRangeMilliseconds + 1) / 2);
            const auto gradeIndex = GraphColorIndex(sample.grade);
            const auto color = GraphColor(gradeIndex);
            data.rectangles.push_back(
                {{scatter.x + x * (scatter.width - 4), scatter.y + y * (scatter.height - 4), 4, 4}, color, 2});
            auto &bin = bins[HistogramMillisecond(sample.error)];
            ++bin.grades[gradeIndex];
            data.maximumBinCount = std::max(data.maximumBinCount, ++bin.total);
        }
        AppendHistogram(data, bins, histogram, minimumBarWidth);
        return data;
    }
} // namespace taiko_result_ui
