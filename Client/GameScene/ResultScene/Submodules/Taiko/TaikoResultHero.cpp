#include "Presentation/UiFonts.h"
#include "TaikoResultBatchVisual.h"
#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include <algorithm>
#include <format>
#include <stdexcept>
using namespace PhantomCore;

using namespace taiko_result_ui::layout;
namespace
{
    constexpr Color White{1, 1, 1, 1};
    std::wstring GroupedDigits(std::uint64_t value)
    {
        auto digits = std::to_wstring(value);
        for (auto position = static_cast<std::ptrdiff_t>(digits.size()) - 3; position > 0; position -= 3)
            digits.insert(static_cast<std::size_t>(position), 1, L',');
        return digits;
    }
} // namespace
void TaikoResultView::BuildHero(Visual2DNode &root)
{
    // Centre column: the rank and score sit inside a ring whose band shows each
    // judgement's share, clockwise from the gold mark at 12 o'clock.
    const float centerX = columns_.centerX + columns_.sideWidth * 0.5F;
    Image(root, {centerX - RingSize * 0.5F, RingCenterY - RingSize * 0.5F, RingSize, RingSize}, rankRing_,
          "Result.RankRing");
    const std::array<std::size_t, taiko_result_ui::RingJudgementCount> counts{
        result_.judgements[0], result_.judgements[1], result_.judgements[2], result_.judgements[3],
        result_.judgements[4], result_.okCount,       result_.judgements[5], result_.poorCount};
    // One row per physical pixel keeps the band edge as fine as the screen allows.
    const float pixelsPerUnit = static_cast<float>(height_) / Height * contentScale_;
    ring_ = taiko_result_ui::BuildJudgementRing(counts, Markers, {centerX, Height - RingCenterY}, RingInnerRadius,
                                                RingOuterRadius, pixelsPerUnit > 0 ? std::max(1.0F / pixelsPerUnit, 0.25F) : 1.0F);
    auto &slices = root.CreateChild("Result.RingSlices");
    slices.SetSize({columns_.width, Height});
    slices.AddComponent<taiko_result_ui::RectangleBatchVisual>(ring_.rectangles);

    constexpr Rect rankBox{-118, 260, 236, 118};
    const auto nativeSize = visuals_.GetImageSize(rankImage_);
    if (nativeSize.width <= 0 || nativeSize.height <= 0)
        throw std::logic_error("Result rank image must have a non-zero size.");
    const float scale = std::min(rankBox.width / nativeSize.width, rankBox.height / nativeSize.height);
    const float width = nativeSize.width * scale, height = nativeSize.height * scale;
    Image(root,
          {centerX + rankBox.x + (rankBox.width - width) * 0.5F, rankBox.y + (rankBox.height - height) * 0.5F, width, height},
          rankImage_, "Result.Rank");
    auto &score = NumberLabel(root, {centerX - 170, 376, 340, 44}, GroupedDigits(result_.summary.score),
                              {40, 800, Ink, TextAlignment::Center});
    score.SetName("Result.Score");
    BuildOptions(root);

    // Slice tooltip: hidden until the pointer rests on the band.
    auto &tooltip = Fill(root, {0, 0, 10, 26}, ui_colors::theme::BrandDeep, "Result.RingTooltip");
    tooltip.SetZIndex(20);
    tooltip.SetVisible(false);
    auto &caption = CreateLabel(tooltip, {0, 0, 10, 26}, L"", "Result.RingTooltipText");
    auto &captionText = *caption.GetComponent<TextVisualComponent>();
    captionText.SetFont(ui_fonts::Text());
    captionText.SetFontSize(12);
    captionText.SetFontWeight(700);
    captionText.SetTextColor(White);
    captionText.SetHorizontalAlignment(TextAlignment::Center);
    tooltipId_ = tooltip.Id();
}
void TaikoResultView::UpdateRingTooltip(std::optional<Point> pointer)
{
    auto *tooltip = tooltipId_ ? canvas_->FindNode(tooltipId_) : nullptr;
    if (!tooltip)
        return;
    std::optional<taiko_result_ui::RingSlice> slice;
    Point local{};
    if (pointer && contentScale_ > 0)
    {
        local = {(pointer->x - contentOrigin_.x) / contentScale_, (pointer->y - contentOrigin_.y) / contentScale_};
        slice = taiko_result_ui::RingSliceAt(ring_, local);
    }
    if (!slice)
    {
        if (hoveredSlice_)
            tooltip->SetVisible(false);
        hoveredSlice_.reset();
        return;
    }
    constexpr std::array<const char *, taiko_result_ui::RingJudgementCount> keys{
        "Judgements.MAX", "Judgements.PERFECT", "Judgements.GREAT", "Judgements.GOOD",
        "Judgements.BAD", "Judgements.OK",      "Judgements.MISS",  "Judgements.POOR"};
    std::size_t total = 0;
    for (const auto &entry : ring_.slices)
        total += entry.count;
    // Same truncation as accuracy: the share never rounds up.
    const auto hundredths = total ? slice->count * 10'000 / total : 0;
    auto text = std::format(L"{} {} · {}.{:02}%", texts_.Get(keys[slice->judgement]), slice->count, hundredths / 100,
                            hundredths % 100);
    const float width = EstimateTextWidth(text, 12) + 20;
    constexpr float height = 26;
    const float x = std::clamp(local.x - width * 0.5F, 4.0F, columns_.width - width - 4.0F);
    tooltip->SetBounds({x, local.y + 12, width, height});
    auto &caption = *tooltip->Children().front();
    caption.SetBounds({0, 0, width, height});
    caption.GetComponent<TextVisualComponent>()->SetText(std::move(text));
    tooltip->SetVisible(true);
    hoveredSlice_ = slice->judgement;
}
void TaikoResultView::BuildOptions(Visual2DNode &root)
{
    const auto &o = result_.summary.playOptions;
    struct Chip
    {
        const char *key;
        std::wstring name, value;
        float width{};
    };
    std::vector<Chip> chips;
    const auto add = [&](const char *key, std::wstring value)
    {
        chips.push_back({key, std::wstring(texts_.Get(std::string("PlayOptions.") + key)), std::move(value)});
    };
    // Only options that differ from a plain play appear under the ring.
    if (o.scrollUnit == PlayOptions::ScrollUnit::Bpm || o.scrollSpeed != 1.0 || o.ignoreScrollEffects)
        add("scrollSpeed", std::format(L"{:g}{}{}", o.scrollSpeed,
                                       o.scrollUnit == PlayOptions::ScrollUnit::Bpm ? L" BPM" : L"x",
                                       o.ignoreScrollEffects ? L" FIX" : L""));
    if (o.metronome)
        add("metronome", L"ON");
    if (o.mirror)
        add("mirror", L"Don / Kat");
    if (o.random)
        add("random", std::format(L"{:g}%  #{}", o.randomAttemptProbability * 100, o.randomSeed));
    if (o.accelerate != 1.0)
        add("accelerate", std::format(L"{:.2f}x  BGM {} / HIT {}", o.accelerate, o.preserveMusicPitch ? L"=" : L"↗",
                                      o.preserveHitSoundPitch ? L"=" : L"↗"));
    if (o.judgeLevel)
        add("judgeLevel", std::format(L"{:.2f}", o.judgeLevel->Value()));

    constexpr float chipHeight = 26, gap = 6, padding = 10, top = 529;
    const float centerX = columns_.centerX + columns_.sideWidth * 0.5F;
    const float rowLimit = std::min(448.0F, columns_.sideWidth);
    for (auto &chip : chips)
        chip.width = padding + EstimateTextWidth(chip.name, 12) + gap + EstimateNumberWidth(chip.value, 12) + padding;
    // Centre each wrapped row under the ring.
    std::size_t first = 0;
    float y = top;
    while (first < chips.size())
    {
        std::size_t last = first;
        float rowWidth = chips[first].width;
        while (last + 1 < chips.size() && rowWidth + gap + chips[last + 1].width <= rowLimit)
            rowWidth += gap + chips[++last].width;
        float x = centerX - rowWidth * 0.5F;
        for (std::size_t i = first; i <= last; ++i)
        {
            const auto &chip = chips[i];
            const float nameWidth = EstimateTextWidth(chip.name, 12);
            Fill(root, {x, y, chip.width, chipHeight}, ui_colors::theme::Chip, std::string("Result.Option.") + chip.key);
            Label(root, {x + padding, y, nameWidth + 2, chipHeight}, chip.name, {12, 700, InkSoft});
            NumberLabel(root, {x + padding + nameWidth + gap, y, chip.width - padding * 2 - nameWidth - gap + 2, chipHeight},
                        chip.value, {12, 700, BrandText}, false);
            x += chip.width + gap;
        }
        first = last + 1;
        y += chipHeight + gap;
    }
}
