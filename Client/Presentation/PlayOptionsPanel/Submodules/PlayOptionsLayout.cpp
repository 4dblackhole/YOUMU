#include "Presentation/PlayOptionsPanel.h"
#include "Presentation/UiDropdown.h"
#include <algorithm>
#include <array>

using namespace PhantomCore;
using namespace ui_modal;

// Body geometry from popups.css Play options: three 293.3 columns 16 apart, a 1px rule before the second and
// third, content of those two indented 16. Fields are 40 tall, switch rows 48 with a bottom rule, groups 12 apart.
namespace
{
    constexpr float BodyWidth = 912, Gap = 16, ColumnWidth = (BodyWidth - Gap * 2) / 3, Indent = 16;
    constexpr float HeadingHeight = 20, FirstTop = 32, FieldGap = 12, LabelHeight = 18, FieldHeight = 40;
    constexpr float SwitchHeight = 48, SubIndent = 14, NumberWidth = 112, RandomGap = 14;
    [[nodiscard]] constexpr float ColumnLeft(const std::size_t column) noexcept
    {
        return (ColumnWidth + Gap) * static_cast<float>(column) + (column == 0 ? 0.0F : Indent);
    }
    [[nodiscard]] constexpr float ContentWidth(const std::size_t column) noexcept
    {
        return column == 0 ? ColumnWidth : ColumnWidth - Indent;
    }
    // Difficulty: scroll speed, accelerate (+ pitch switches), JudgeLevel override (+ box), HP gauge.
    [[nodiscard]] constexpr float DifficultyHeight(const bool expanded, const bool judge) noexcept
    {
        float top = FirstTop + LabelHeight + 6 + FieldHeight + FieldGap; // Scroll speed.
        top += 24 + 6 + FieldHeight + FieldGap;                          // Accelerate.
        if (expanded)
            top += 8 + SwitchHeight * 2 + 8;
        top += SwitchHeight + (judge ? 8 + FieldHeight : 0.0F) + FieldGap;
        return top + LabelHeight + 6 + FieldHeight; // HP gauge.
    }
    [[nodiscard]] constexpr float LayoutHeight(const bool random) noexcept
    {
        const float switches = FirstTop + SwitchHeight * 2 + FieldGap;
        return random ? switches + RandomGap + (LabelHeight + 6 + FieldHeight) * 2 + 8 : switches;
    }
    // Assist: Ignore scroll effects and Metronome switches, then the max strokes label and its 1..4 tabs.
    constexpr float StrokeTabWidth = 28, StrokeTabHeight = 28;
    [[nodiscard]] constexpr float AssistHeight() noexcept
    {
        return FirstTop + SwitchHeight * 2 + FieldGap + LabelHeight + 6 + StrokeTabHeight;
    }
} // namespace

void PlayOptionsPanel::SetViewport(const std::uint32_t height)
{
    height_ = height;
    if (canvas_)
        Layout();
}

float PlayOptionsPanel::ColumnHeight(const std::size_t column) const noexcept
{
    // A hidden invalid random value must remain repairable without enabling Random.
    const bool randomDetails = draft_.random || errorField_ == Seed || errorField_ == Probability;
    if (column == 0)
        return DifficultyHeight(rateExpanded_, overrideJudge_);
    if (column == 1)
        return LayoutHeight(randomDetails);
    return AssistHeight();
}

void PlayOptionsPanel::PlaceSwitchRow(const Flag flag, const float left, const float top, const float width,
                                      const float height)
{
    auto &row = flags_[flag];
    row.row->SetVisible(true);
    Place(*row.row, {left, top, width, SwitchHeight}, height);
    row.label->SetBounds({0, 0, width - 52, SwitchHeight});
    row.toggle->SetBounds({width - 40, (SwitchHeight - 24) / 2, 40, 24});
    row.rule->SetBounds({0, 0, width, 1});
}

float PlayOptionsPanel::LayoutDifficulty(const float height)
{
    constexpr float left = ColumnLeft(0), width = ContentWidth(0);
    float top = FirstTop;
    Place(*labels_[Speed], {left, top, width, LabelHeight}, height);
    top += LabelHeight + 6;
    // The x / BPM tabs keep the mockup's measured widths (13px Nunito 800); the field follows 8 after them.
    constexpr std::array UnitWidths{11.3F, 33.1F};
    float tabLeft = left;
    for (std::size_t i = 0; i < units_.size(); ++i)
    {
        Place(*units_[i], {tabLeft, top, UnitWidths[i], 28}, height);
        SetTabSelected(*units_[i], (draft_.scrollUnit == PlayOptions::ScrollUnit::Bpm) == (i == 1));
        tabLeft += UnitWidths[i] + 16;
    }
    PlaceField(*fields_[Speed], {tabLeft - 8, top, NumberWidth, FieldHeight}, height);
    top += FieldHeight + FieldGap;

    const float rateWidth = EstimateWidth(labels_[Rate]->GetComponent<TextVisualComponent>()->Text(), 13);
    Place(*labels_[Rate], {left, top, rateWidth + 4, 24}, height);
    Place(*rateRange_, {left + rateWidth + 8, top + 4, width - rateWidth - 8 - 32, 16}, height);
    Place(*rateMore_, {left + width - 24, top, 24, 24}, height);
    rateMoreIcon_->SetBounds({5, 5, 14, 14});
    top += 24 + 6;
    // Accelerate has the same x / BPM tabs before its field as the scroll speed.
    tabLeft = left;
    for (std::size_t i = 0; i < rateUnits_.size(); ++i)
    {
        Place(*rateUnits_[i], {tabLeft, top, UnitWidths[i], 28}, height);
        SetTabSelected(*rateUnits_[i], (draft_.accelerateUnit == PlayOptions::ScrollUnit::Bpm) == (i == 1));
        tabLeft += UnitWidths[i] + 16;
    }
    PlaceField(*fields_[Rate], {tabLeft - 8, top, NumberWidth, FieldHeight}, height);
    top += FieldHeight + FieldGap;
    // The pitch switches hang 8 under the field and keep the usual gap below them.
    if (rateExpanded_)
        top += 8 - FieldGap;

    flags_[MusicPitch].row->SetVisible(rateExpanded_);
    flags_[HitPitch].row->SetVisible(rateExpanded_);
    subRules_[0]->SetVisible(rateExpanded_);
    if (rateExpanded_)
    {
        Place(*subRules_[0], {left, top, 2, SwitchHeight * 2 + 8}, height);
        PlaceSwitchRow(MusicPitch, left + SubIndent, top, width - SubIndent, height);
        PlaceSwitchRow(HitPitch, left + SubIndent, top + SwitchHeight + 8, width - SubIndent, height);
        top += SwitchHeight * 2 + 8 + FieldGap;
    }

    PlaceSwitchRow(OverrideJudge, left, top, width, height);
    top += SwitchHeight;
    fields_[Judge]->SetVisible(overrideJudge_);
    judgeUnit_->SetVisible(overrideJudge_);
    subRules_[1]->SetVisible(overrideJudge_);
    if (overrideJudge_)
    {
        top += 8;
        Place(*subRules_[1], {left, top, 2, FieldHeight}, height);
        PlaceField(*fields_[Judge], {left + SubIndent, top, NumberWidth, FieldHeight}, height);
        Place(*judgeUnit_, {left + SubIndent + NumberWidth + 8, top, 60, FieldHeight}, height);
        top += FieldHeight;
    }
    top += FieldGap;

    const float gaugeWidth = EstimateWidth(gaugeLabel_->GetComponent<TextVisualComponent>()->Text(), 13);
    Place(*gaugeLabel_, {left, top, gaugeWidth + 4, LabelHeight}, height);
    Place(*gaugeClear_, {left + gaugeWidth + 8, top + 1, 120, 16}, height);
    top += LabelHeight + 6;
    Place(*gauge_, {left, top, width, FieldHeight}, height);
    return top + FieldHeight;
}

float PlayOptionsPanel::LayoutLayoutColumn(const float height)
{
    constexpr float left = ColumnLeft(1), width = ContentWidth(1);
    PlaceSwitchRow(Mirror, left, FirstTop, width, height);
    PlaceSwitchRow(Random, left, FirstTop + SwitchHeight + FieldGap, width, height);
    const bool randomDetails = draft_.random || errorField_ == Seed || errorField_ == Probability;
    for (auto *node : {labels_[Seed], fields_[Seed], labels_[Probability], fields_[Probability], probabilityUnit_})
        node->SetVisible(randomDetails);
    if (!randomDetails)
        return FirstTop + SwitchHeight * 2 + FieldGap;
    const float subLeft = left + SubIndent, subWidth = width - SubIndent;
    float top = FirstTop + SwitchHeight * 2 + FieldGap + RandomGap;
    Place(*labels_[Seed], {subLeft, top, subWidth, LabelHeight}, height);
    PlaceField(*fields_[Seed], {subLeft, top + LabelHeight + 6, subWidth, FieldHeight}, height);
    top += LabelHeight + 6 + FieldHeight + 8;
    Place(*labels_[Probability], {subLeft, top, subWidth, LabelHeight}, height);
    PlaceField(*fields_[Probability], {subLeft, top + LabelHeight + 6, NumberWidth, FieldHeight}, height);
    Place(*probabilityUnit_, {subLeft + NumberWidth + 8, top + LabelHeight + 6, 20, FieldHeight}, height);
    return top + LabelHeight + 6 + FieldHeight;
}

void PlayOptionsPanel::LayoutAssist(const float height)
{
    constexpr float left = ColumnLeft(2), width = ContentWidth(2);
    PlaceSwitchRow(IgnoreScroll, left, FirstTop, width, height);
    PlaceSwitchRow(Metronome, left, FirstTop + SwitchHeight + FieldGap, width, height);
    float top = FirstTop + SwitchHeight * 2 + FieldGap;
    const float labelWidth = EstimateWidth(strokeLabel_->GetComponent<TextVisualComponent>()->Text(), 13);
    Place(*strokeLabel_, {left, top, labelWidth + 4, LabelHeight}, height);
    Place(*strokeRange_, {left + labelWidth + 8, top + 1, width - labelWidth - 8, 16}, height);
    top += LabelHeight + 6;
    for (std::size_t i = 0; i < strokeTabs_.size(); ++i)
    {
        Place(*strokeTabs_[i], {left + (StrokeTabWidth + 8) * static_cast<float>(i), top, StrokeTabWidth,
                                StrokeTabHeight}, height);
        SetTabSelected(*strokeTabs_[i], draft_.maxStrokes == static_cast<int>(i) + 1);
    }
}

void PlayOptionsPanel::Layout()
{
    if (!canvas_ || !gauge_ || height_ == 0)
        return;
    const float height = std::max({ColumnHeight(0), ColumnHeight(1), ColumnHeight(2)});
    modal_.Layout(height);
    for (std::size_t i = 0; i < headings_.size(); ++i)
        Place(*headings_[i], {ColumnLeft(i), 0, ContentWidth(i), HeadingHeight}, height);
    for (std::size_t i = 0; i < columnRules_.size(); ++i)
        Place(*columnRules_[i], {(ColumnWidth + Gap) * static_cast<float>(i + 1), 0, 1, height}, height);
    static_cast<void>(LayoutDifficulty(height));
    static_cast<void>(LayoutLayoutColumn(height));
    LayoutAssist(height);
    for (std::size_t i = 0; i < fields_.size(); ++i)
        StyleField(*fields_[i], inputs_[i] && inputs_[i]->HasFocus(), true);
    UpdateInputs();
}

void PlayOptionsPanel::UpdateInputs()
{
    if (!canvas_ || !gauge_)
        return;
    const bool available = IsVisible() && !RequireDropdown(*gauge_).IsExpanded();
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        const bool visible = available && fields_[i]->IsVisible();
        SetText(*fields_[i], inputs_[i] && visible ? std::wstring_view{} : std::wstring_view{numbers_[i]});
        if (!inputs_[i])
            continue;
        PlaceTextInput(*inputs_[i], *fields_[i], *canvas_, height_, modal_.Scale());
        inputs_[i]->SetVisible(visible);
    }
}
