#include "TaikoResultBatchVisual.h"
#include "TaikoResultGraphs.h"
#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include <algorithm>
#include <cmath>

using namespace PhantomCore;

using namespace taiko_result_ui::layout;
namespace
{
    // Signed m:ss for the time axis; the start may lie before chart zero.
    std::wstring Clock(double seconds)
    {
        const auto whole = static_cast<long long>(std::floor(std::abs(seconds)));
        return std::format(L"{}{}:{:02}", seconds < 0 && whole > 0 ? L"−" : L"", whole / 60, whole % 60);
    }
} // namespace
void TaikoResultView::BuildGraphs(Visual2DNode &root)
{
    const auto &text = Result(texts_);
    const auto &c = columns_;
    Label(root, {c.rightX, 104, c.sideWidth, 20}, std::wstring(texts_.Get("Result.timing")), {14, 900, Ink});
    BuildTimingStatistics(root);
    Fill(root, HistogramChart(c), ui_colors::theme::Plot, "Result.HistogramChart");
    Fill(root, ScatterChart(c), ui_colors::theme::Plot, "Result.ScatterChart");
    const auto histogram = HistogramPlot(c), scatter = ScatterPlot(c);
    // Grid lines are decoration. The graph builder owns range/bin/point
    // derivation from the immutable mode result.
    for (int line = 0; line <= 3; ++line)
        Fill(root, {histogram.x, histogram.y + histogram.height * static_cast<float>(line) / 3, histogram.width, 1},
             ui_colors::theme::Grid);
    for (int line = 0; line <= 4; ++line)
        Fill(root, {scatter.x, scatter.y + scatter.height * static_cast<float>(line) / 4, scatter.width, 1},
             ui_colors::theme::Grid);
    // A one-pixel minimum keeps isolated 1ms bins visible without merging their counts.
    const float pixelsPerUnit = static_cast<float>(height_) / Height * contentScale_;
    const float pixel = pixelsPerUnit > 0 ? 1.0F / pixelsPerUnit : 1.0F;
    auto data = taiko_result_ui::BuildGraphs(result_, Bounds(scatter), Bounds(histogram), pixel);
    // HP: dashed clear line under the recorded curve.
    const auto &summary = result_.summary;
    const double clearPercent = summary.hpGauges.empty() ? 60.0 : summary.hpGauges.front().clearPercent;
    const auto hpPlot = Bounds(HpPlot);
    const float clearY = hpPlot.y + hpPlot.height * static_cast<float>(std::clamp(clearPercent / 100.0, 0.0, 1.0));
    for (float x = 0; x < hpPlot.width; x += 9)
        data.rectangles.push_back({{hpPlot.x + x, clearY - 0.75F, std::min(5.0F, hpPlot.width - x), 1.5F},
                                   ui_colors::theme::ClearLine, 0});
    const auto hp = taiko_result_ui::BuildHpCurve(summary, hpPlot);
    data.rectangles.insert(data.rectangles.end(), hp.begin(), hp.end());
    auto &graphs = root.CreateChild("Result.Graphs");
    graphs.SetSize({c.width, Height});
    graphs.AddComponent<taiko_result_ui::RectangleBatchVisual>(std::move(data.rectangles));
    BuildGraphLabels(root, data);
    if (result_.timingErrors.empty())
        for (const auto &plot : {histogram, scatter})
            Label(root, {plot.x, plot.y + plot.height * 0.5F - 10, plot.width, 20}, std::wstring(text.noTimingData),
                  {13, 700, ResultColor(0x5B7194), TextAlignment::Center});
}
void TaikoResultView::BuildTimingStatistics(Visual2DNode &root)
{
    const auto &text = Result(texts_);
    const auto &c = columns_;
    // The figures that read the graphs sit above them.
    const std::array<std::wstring, 4> names{std::wstring(text.mean), std::wstring(text.standardDeviation),
                                            std::wstring(text.variance), std::wstring(texts_.Get("Result.samples"))};
    const auto milliseconds = [this](const std::optional<double> &value)
    {
        return value ? Format(std::wstring(texts_.Get("Result.millisecondsFormat")), *value)
                     : std::wstring(texts_.Get("Result.emptyMilliseconds"));
    };
    const std::array<std::wstring, 4> values{
        milliseconds(result_.meanErrorMilliseconds), milliseconds(result_.standardDeviationMilliseconds),
        result_.varianceMillisecondsSquared
            ? Format(std::wstring(texts_.Get("Result.varianceFormat")), *result_.varianceMillisecondsSquared)
            : std::wstring(texts_.Get("Result.emptyVariance")),
        std::to_wstring(result_.timingErrors.size())};
    const float step = (c.sideWidth + 8) / 4;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const float x = c.rightX + 10 + static_cast<float>(i) * step;
        Fill(root, {x - 10, 138, 1, 38}, ui_colors::theme::RuleStrong);
        Label(root, {x, 138, step - 12, 16}, names[i], {13, 700, Caption});
        NumberLabel(root, {x, 156, step - 12, 20}, values[i], {15, 800, Ink}, false);
    }
}
void TaikoResultView::BuildGraphLabels(Visual2DNode &root, const taiko_result_ui::GraphData &data)
{
    const auto &text = Result(texts_);
    const auto &c = columns_;
    const TextStyle caption{13, 700, Ink}, axis{12, 500, Caption};
    const auto centered = [](TextStyle style, TextAlignment alignment) { style.alignment = alignment; return style; };
    const float third = c.sideWidth / 3;
    Label(root, {c.rightX, 190, c.sideWidth * 0.6F, 20}, std::wstring(text.histogram), caption);
    Label(root, {c.rightX + c.sideWidth * 0.5F, 192, c.sideWidth * 0.5F, 16}, Format(text.peakFormat, data.maximumBinCount),
          {12, 700, Caption, TextAlignment::Trailing});
    const float histogramAxis = HistogramTop + ChartHeight + 4;
    NumberLabel(root, {c.rightX, histogramAxis, third, 16},
                std::format(L"{} −{:.0f} ms", text.early, data.halfRangeMilliseconds), axis, false);
    NumberLabel(root, {c.rightX + third, histogramAxis, third, 16}, L"0", centered(axis, TextAlignment::Center), false);
    NumberLabel(root, {c.rightX + third * 2, histogramAxis, third, 16},
                std::format(L"+{:.0f} ms {}", data.halfRangeMilliseconds, text.late),
                centered(axis, TextAlignment::Trailing), false);
    Label(root, {c.rightX, 417, c.sideWidth, 20}, std::wstring(text.scatter), caption);
    const float scatterAxis = ScatterTop + ChartHeight + 4;
    NumberLabel(root, {c.rightX, scatterAxis, third, 16}, Clock(data.startSeconds), axis, false);
    NumberLabel(root, {c.rightX + third, scatterAxis, third, 16}, std::wstring(text.timeAxis),
                centered(axis, TextAlignment::Center), false);
    NumberLabel(root, {c.rightX + third * 2, scatterAxis, third, 16}, Clock(data.endSeconds),
                centered(axis, TextAlignment::Trailing), false);
}
