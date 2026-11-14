#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include "Presentation/AccuracyText.h"
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <numbers>

using namespace PhantomCore;

using namespace taiko_result_ui::layout;
namespace
{
    std::wstring Decode(std::string_view text)
    {
        if (text.empty())
            return L"—";
        if (text.size() > static_cast<std::size_t>(INT_MAX))
            return L"�";
        const auto size = static_cast<int>(text.size());
        const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
        if (length == 0)
            return L"�";
        std::wstring value(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, value.data(), length) != length)
            return L"�";
        return value;
    }
    constexpr Color White{1, 1, 1, 1};
    constexpr float ColumnRight = Margin + LeftWidth;
} // namespace
void TaikoResultView::BuildSongBanner(Visual2DNode &root)
{
    const float width = columns_.width;
    Image(root, {0, 0, width, BannerHeight}, banner_, "Result.SongBanner");
    Image(root, {32, 21, 93, 30}, modeBadge_, "Result.ModeBadge");
    Image(root, {50, 28, 16, 16}, modeIcon_, "Result.ModeIcon");
    NumberLabel(root, {70, 21, 52, 30}, std::wstring(texts_.Get("Result.modeName")), {12, 800, White}, false);

    // Right block: difficulty name with its difficulty-value badge, then the chart creator.
    // Both rows start at the same x and the block ends at the right margin.
    const auto pattern = Decode(result_.summary.patternName);
    const auto creator = context_.patternCreator.empty() ? std::wstring(L"—") : context_.patternCreator;
    constexpr float badgeWidth = 40, badgeGap = 8, creatorIconSize = 14, creatorGap = 6;
    const float patternWidth = EstimateTextWidth(pattern, 16), creatorWidth = EstimateTextWidth(creator, 13);
    const float blockWidth = std::clamp(std::max(patternWidth + badgeGap + badgeWidth, creatorIconSize + creatorGap + creatorWidth),
                                        82.0F, width * 0.34F);
    const float blockX = width - Margin - blockWidth;
    const float nameWidth = std::min(patternWidth, blockWidth - badgeGap - badgeWidth);
    Label(root, {blockX, 14, nameWidth + 2, 24}, pattern, {16, 700, BrandText});
    // The difficulty value has no formula yet, so the badge shows a dash.
    Fill(root, {blockX + nameWidth + badgeGap, 15, badgeWidth, 22}, ui_colors::theme::Badge, "Result.LevelBadge");
    NumberLabel(root, {blockX + nameWidth + badgeGap, 15, badgeWidth, 22}, L"—",
                {14, 800, BrandText, TextAlignment::Center});
    Image(root, {blockX, 41, creatorIconSize, creatorIconSize}, creatorIcon_, "Result.CreatorIcon", Ink);
    Label(root, {blockX + creatorIconSize + creatorGap, 38, blockWidth - creatorIconSize - creatorGap, 20}, creator,
          {13, 600, Ink});

    constexpr float titleX = 145;
    const float titleWidth = std::max(blockX - 20 - titleX, 1.0F);
    Label(root, {titleX, 11, titleWidth, 30}, context_.songTitle.empty() ? L"—" : context_.songTitle, {24, 900, Ink});
    Label(root, {titleX, 41, titleWidth, 20}, context_.songArtist.empty() ? L"—" : context_.songArtist,
          {14, 700, Caption});
}
void TaikoResultView::BuildHpGraph(Visual2DNode &root)
{
    const auto &text = Result(texts_);
    const auto &summary = result_.summary;
    const bool applicable = !summary.hpGauges.empty() && summary.hpGauges.front().applicable;
    const double clearPercent = summary.hpGauges.empty() ? 60.0 : summary.hpGauges.front().clearPercent;
    // The title row carries the failure mark and the pause/Rewind usage so
    // nothing is written over the curve.
    Label(root, {Margin, 107, 24, 16}, std::wstring(texts_.Get("Result.hp")), {13, 700, Caption});
    if (summary.endReason == PlayEndReason::Completed && summary.CanUseHp() && !summary.IsCleared() &&
        std::ranges::any_of(summary.hpGauges, [](const auto &hp) { return hp.applicable; }))
    {
        const std::wstring failed(text.failed);
        const float chipWidth = EstimateTextWidth(failed, 12) + 16;
        Fill(root, {Margin + 28, 105, chipWidth, 20}, ui_colors::theme::DangerSurface, "Result.FailureChip");
        auto &label = Label(root, {Margin + 28, 105, chipWidth, 20}, failed,
                            {12, 800, ui_colors::theme::Danger, TextAlignment::Center});
        label.SetName("Result.Failure");
    }
    std::wstring usage;
    if (summary.pauseMenuUsed)
        usage = text.pauseUsed;
    if (std::ranges::any_of(summary.controls, [](const auto &control)
                           { return control.type == PlayControlType::Rewind; }))
    {
        if (!usage.empty()) usage += L" · ";
        usage += text.rewindUsed;
    }
    if (!usage.empty())
    {
        auto &label = Label(root, {Margin + 100, 107, LeftWidth - 100, 16}, std::move(usage),
                            {12, 700, Caption, TextAlignment::Trailing});
        label.SetName("Result.ControlUsage");
    }

    Fill(root, HpChart, ui_colors::theme::Plot, "Result.HpChart");
    for (int line = 0; line <= 4; ++line)
        Fill(root, {HpPlot.x, HpPlot.y + HpPlot.height * static_cast<float>(line) / 4, HpPlot.width, 1},
             ui_colors::theme::Grid);
    // Axis: 100, the gauge's clear threshold and 0. No text inside the plot.
    const auto axisLabel = [&](double percent, Color color)
    {
        const float y = HpPlot.y + HpPlot.height * static_cast<float>(1.0 - percent / 100.0);
        Label(root, {HpChart.x, y - 8, HpPlot.x - HpChart.x - 8, 16}, Format(L"{:.0f}%", percent),
              {12, 700, color, TextAlignment::Trailing});
    };
    axisLabel(100, ResultColor(0x4A6C9C));
    axisLabel(clearPercent, ui_colors::theme::Brand);
    axisLabel(0, ResultColor(0x4A6C9C));
    if (!summary.hpHistory || !applicable)
        Label(root, {HpPlot.x, HpPlot.y + HpPlot.height * 0.72F - 10, HpPlot.width, 20}, std::wstring(text.noHpData),
              {13, 700, ResultColor(0x5B7194), TextAlignment::Center});
}
void TaikoResultView::BuildJudgements(Visual2DNode &root)
{
    const std::array<std::wstring_view, 8> names{texts_.Get("Judgements.MAX"),   texts_.Get("Judgements.PERFECT"),
                                                 texts_.Get("Judgements.GREAT"), texts_.Get("Judgements.GOOD"),
                                                 texts_.Get("Judgements.BAD"),   texts_.Get("Judgements.OK"),
                                                 texts_.Get("Judgements.MISS"),  texts_.Get("Judgements.POOR")};
    const std::array<std::size_t, 8> counts{result_.judgements[0], result_.judgements[1], result_.judgements[2],
                                            result_.judgements[3], result_.judgements[4], result_.okCount,
                                            result_.judgements[5], result_.poorCount};
    // Two columns of four: MAX..GOOD on the left, BAD..POOR on the right.
    constexpr float top = 346, rowHeight = 32, columnWidth = 160, columnStep = 180;
    constexpr float slant = -14.0F * std::numbers::pi_v<float> / 180.0F;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const float x = Margin + static_cast<float>(i / 4) * columnStep;
        const float y = top + static_cast<float>(i % 4) * rowHeight;
        // Slanted judgement-colour mark with a thin dark edge so white MAX stays visible.
        for (const auto &[inset, color] : {std::pair{0.0F, ResultColor(0x0F2A47, .25F)}, std::pair{1.0F, Markers[i]}})
        {
            auto &mark = Fill(root, {x + 2 + inset, y + 8 + inset, 5 - inset * 2, 16 - inset * 2}, color,
                              "Result.JudgementMark");
            mark.SetPivot({.5F, .5F});
            mark.SetPosition({x + 4.5F, Height - (y + 16)});
            mark.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, slant);
        }
        NumberLabel(root, {x + 14, y, 96, rowHeight - 1}, std::wstring(names[i]), {14, 700, InkSoft});
        NumberLabel(root, {x + 60, y, columnWidth - 60, rowHeight - 1}, std::to_wstring(counts[i]),
                    {22, 800, Ink, TextAlignment::Trailing});
        Fill(root, {x, y + rowHeight - 1, columnWidth, 1}, ui_colors::theme::Rule);
    }
    const auto &text = Result(texts_);
    Label(root, {Margin, 482, 150, 21}, std::wstring(text.ticks), {13, 400, InkSoft});
    NumberLabel(root, {ColumnRight - 200, 482, 200, 21}, Format(text.tickFormat, result_.acceptedTicks, result_.missedTicks),
                {17, 800, Ink, TextAlignment::Trailing});
}
void TaikoResultView::BuildGroups(Visual2DNode &root)
{
    const auto &text = Result(texts_);
    Label(root, {Margin, 513, LeftWidth, 20}, std::wstring(text.groupTitle), {14, 900, Ink});
    constexpr float top = 541, rowHeight = 28;
    for (std::size_t i = 0; i < result_.groups.size(); ++i)
    {
        const float y = top + static_cast<float>(i) * rowHeight;
        Label(root, {Margin, y, 220, rowHeight - 4}, std::wstring(text.groups[i]), {13, 400, InkSoft});
        const auto &group = result_.groups[i];
        NumberLabel(root, {ColumnRight - 160, y, 160, rowHeight - 4},
                    group.total ? AccuracyText(group.accuracy) : std::wstring(texts_.Get("Result.emptyPercent")),
                    {17, 800, Ink, TextAlignment::Trailing});
        Fill(root, {Margin, y + rowHeight - 4, LeftWidth, 1}, ui_colors::theme::Rule);
    }
    const auto &summary = result_.summary;
    Label(root, {Margin, 629, LeftWidth, 16},
          Format(text.noteCounts, summary.totalNotes, summary.processedNotes, summary.unprocessedNotes),
          {12, 400, Caption});
}
void TaikoResultView::BuildSummary(Visual2DNode &root)
{
    const auto &text = Result(texts_);
    BuildHpGraph(root);
    // One summary line: accuracy is the primary figure, maximum combo the smaller
    // one whose right edge lines up with the judgement counts below.
    Label(root, {Margin, 240, 200, 16}, std::wstring(text.accuracy), {13, 700, Caption});
    Label(root, {ColumnRight - 160, 240, 160, 16}, std::wstring(texts_.Get("Result.maximumCombo")),
          {13, 700, Caption, TextAlignment::Trailing});
    const auto accuracy = AccuracyText(result_.summary.accuracy);
    const auto number = accuracy.substr(0, accuracy.size() - 1);
    const float numberWidth = EstimateNumberWidth(number, 56);
    auto &accuracyLabel = NumberLabel(root, {Margin, 258, 250, 70}, number, {56, 800, Ink});
    accuracyLabel.SetName("Result.Accuracy");
    NumberLabel(root, {Margin + numberWidth + 4, 283, 32, 31}, L"%", {26, 700, InkSoft}, false);
    auto &combo = NumberLabel(root, {ColumnRight - 160, 276, 160, 40}, std::to_wstring(result_.summary.maximumCombo),
                              {32, 800, Ink, TextAlignment::Trailing});
    combo.SetName("Result.MaximumCombo");
    Fill(root, {Margin, 337, LeftWidth, 1}, ui_colors::theme::RuleStrong);
    BuildJudgements(root);
    BuildGroups(root);
}
