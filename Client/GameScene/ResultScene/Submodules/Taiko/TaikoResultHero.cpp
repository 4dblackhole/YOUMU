#include "Presentation/UiFonts.h"
#include "TaikoResultBatchVisual.h"
#include "TaikoResultLayout.h"
#include "TaikoResultView.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>
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
    double Hundredths(const double value)
    {
        return std::round(value * 100) / 100;
    }
    // A multiplier keeps one decimal (×2.0, ×1.25); a BPM target shows as set.
    std::wstring ScrollCaption(const PlayOptions &options)
    {
        if (options.scrollUnit == PlayOptions::ScrollUnit::Bpm)
            return std::format(L"{:g}BPM", options.scrollSpeed);
        auto text = std::format(L"{:g}", options.scrollSpeed);
        if (text.find(L'.') == std::wstring::npos)
            text += L".0";
        return L"×" + text;
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
    Hint(score, {std::wstring(texts_.Get("Tooltips.score"))});
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
    // Mode and option tiles under the ring, like osu!'s mod icons: the mode first, then only the
    // options that differ from a plain play. A tile is the skin's white tile tinted (navy mode,
    // gold Accelerate, cobalt others) with a line icon and a short value under it. Resting on a
    // tile names the option and its settings. A new option adds one tile to this list.
    using ui_colors::theme::Brand, ui_colors::theme::BrandDeep, ui_colors::theme::Sun;
    const auto &o = result_.summary.playOptions;
    struct Tile
    {
        std::string key;
        ImageHandle icon;
        std::wstring caption;
        TooltipText hint;
        Color fill{Brand};
        Color ink{White};
        std::wstring glyph{}; // Drawn in place of a missing icon.
    };
    const auto text = [&](const char *key) { return std::wstring(texts_.Get(key)); };
    const auto option = [&](const char *key) { return std::wstring(texts_.Get(std::string("PlayOptions.") + key)); };
    std::vector<Tile> tiles;
    tiles.push_back({"mode", modeIcon_, text("Result.modeName"),
                     {text("Tooltips.gameMode"), {}, text("Result.modeName")}, BrandDeep});
    if (o.accelerate != 1.0)
    {
        auto note = std::format(L"{:.2f}x", o.accelerate);
        if (o.preserveMusicPitch)
            note += L" · " + option("musicPitch");
        if (o.preserveHitSoundPitch)
            note += L" · " + option("hitPitch");
        tiles.push_back({"accelerate", optionImages_.accelerate, std::format(L"{:.2f}×", o.accelerate),
                         {option("accelerate"), {}, std::move(note)}, Sun, BrandDeep});
    }
    if (o.mirror)
        tiles.push_back({"mirror", optionImages_.mirror, text("Result.optionMirror"), {option("mirror"), {}, L"Don / Kat"}});
    if (o.random)
    {
        const double percent = Hundredths(o.randomAttemptProbability * 100);
        tiles.push_back({"random", optionImages_.random, std::format(L"{:g}%", percent),
                         {option("random"), {},
                          std::format(L"{} {:g}% · {} {}", option("probability"), percent, option("seed"), o.randomSeed)}});
    }
    if (o.judgeLevel)
    {
        const double level = o.judgeLevel->Value();
        tiles.push_back({"judgeLevel", optionImages_.judgeLevel, std::format(L"{:g}", Hundredths(level)),
                         {option("judgeLevel"), {}, std::format(L"{:.2f}", level)}});
    }
    if (o.scrollUnit == PlayOptions::ScrollUnit::Bpm || o.scrollSpeed != 1.0)
    {
        const bool bpm = o.scrollUnit == PlayOptions::ScrollUnit::Bpm;
        tiles.push_back({"scrollSpeed", optionImages_.scrollSpeed, ScrollCaption(o),
                         {option("scrollSpeed"), {}, std::format(L"{:g}{}", o.scrollSpeed, bpm ? L" BPM" : L"x")}});
    }
    if (o.metronome)
        tiles.push_back({"metronome", optionImages_.metronome, text("Result.optionMetronome"), {option("metronome")}});
    if (o.ignoreScrollEffects)
        tiles.push_back({"ignoreScrollEffects", optionImages_.ignoreScrollEffects, text("Result.optionIgnoreScroll"),
                         {option("ignoreScroll")}});
    // The default stroke count is a plain play; another one shows the afterimage icon and the count.
    if (o.maxStrokes != PlayOptions{}.maxStrokes)
        tiles.push_back({"maxStrokes", optionImages_.maxStrokes, std::to_wstring(o.maxStrokes),
                         {option("maxStrokes"), {}, std::format(L"{} {}", option("maxStrokesRange"), o.maxStrokes)}});
    // The default gauge is a plain play; another chosen gauge shows its name (HP-02 has none yet).
    if (const auto *gauge = FindHpGauge(o.hpGauge); gauge && o.hpGauge != HpGaugeKind::Default)
        tiles.push_back({"hpGauge", {}, std::wstring(gauge->name.begin(), gauge->name.end()), {option("hpGauge")},
                         Brand, White, text("Result.hp")});

    // Rows of up to eight tiles fit the 448-wide middle of the centre column; each row is centred.
    constexpr float tileWidth = 46, tileHeight = 50, gap = 6, top = 525, iconSize = 24, captionSize = 11;
    const float centerX = columns_.centerX + columns_.sideWidth * 0.5F;
    const float rowLimit = std::min(448.0F, columns_.sideWidth);
    const auto perRow = std::max<std::size_t>(1, static_cast<std::size_t>((rowLimit + gap) / (tileWidth + gap)));
    for (std::size_t first = 0; first < tiles.size(); first += perRow)
    {
        const auto count = std::min(perRow, tiles.size() - first);
        const float rowWidth = static_cast<float>(count) * (tileWidth + gap) - gap;
        const float y = top + static_cast<float>(first / perRow) * (tileHeight + gap);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto &tile = tiles[first + i];
            const float x = centerX - rowWidth * 0.5F + static_cast<float>(i) * (tileWidth + gap);
            auto &plate = Image(root, {x, y, tileWidth, tileHeight}, optionImages_.tile, "Result.Option." + tile.key, tile.fill);
            Hint(plate, tile.hint);
            if (tile.icon)
                Image(root, {x + (tileWidth - iconSize) * 0.5F, y + 7, iconSize, iconSize}, tile.icon, "Result.OptionIcon",
                      tile.ink);
            else
                NumberLabel(root, {x, y + 7, tileWidth, iconSize}, tile.glyph, {18, 800, tile.ink, TextAlignment::Center});
            // A long value shrinks to stay inside its tile.
            const float width = std::max(EstimateNumberWidth(tile.caption, captionSize), 1.0F);
            const float size = std::min(captionSize, captionSize * (tileWidth - 6) / width);
            auto &caption = NumberLabel(root, {x, y + 34, tileWidth, 13}, tile.caption, {size, 800, tile.ink, TextAlignment::Center});
            caption.SetName("Result.OptionValue." + tile.key);
        }
    }
}
