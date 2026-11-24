#include "RecordFilterPanel.h"
#include "SongSelectionText.h"
#include <algorithm>
#include <utility>

using namespace PhantomCore;
using namespace ui_modal;

namespace
{
    // Body geometry (popups.css Record filters): options on the left, ranges, seed and results on the right.
    constexpr float Width = 760, ColumnWidth = 344, RightLeft = 368, HeadingHeight = 20;
    constexpr float ChoiceTop = 30, ChoicePitch = 42, ChoiceHeight = 36, TabHeight = 34, TabGap = 10;
    constexpr float RangeHeadTop = 30, RangeTop = 54, RangePitch = 48, RowHeight = 40;
    constexpr float LabelLeft = RightLeft + 52, MinimumLeft = RightLeft + 160, FieldWidth = 88;
    constexpr float MaximumLeft = MinimumLeft + FieldWidth + 8;
    constexpr float ResultsTop = RangeTop + RangePitch * static_cast<float>(RecordFilterValueRowCount) + 12;
    constexpr std::size_t ResultFlagBegin = 7; // Completed, Cleared, PauseMenuUsed sit under the ranges.
    constexpr float BodyHeight =
        ResultsTop + ChoiceTop + ChoicePitch * static_cast<float>(RecordFilterFlagCount - ResultFlagBegin - 1) + ChoiceHeight;
    constexpr std::array ChoiceKeys{"PlayOptions.mirror", "PlayOptions.random", "PlayOptions.metronome",
                                    "PlayOptions.ignoreScroll", "PlayOptions.musicPitch", "PlayOptions.hitPitch",
                                    "RecordFilter.judgeOverride", "RecordFilter.completed", "RecordFilter.cleared",
                                    "RecordFilter.pauseMenuUsed", "RecordFilter.scrollUnit"};
    static_assert(ChoiceKeys.size() == RecordFilterFlagCount + 1);
    constexpr std::array RowKeys{"RecordFilter.judgeLevel", "RecordFilter.accelerate", "RecordFilter.scrollSpeed",
                                 "RecordFilter.probability", "RecordFilter.seed"};
    static_assert(RowKeys.size() == RecordFilterValueRowCount);
    constexpr std::array<std::wstring_view, 9> Placeholders{L"1.00", L"100.00", L"0.5", L"2.0", L"", L"", L"0", L"100",
                                                            L"uint64"};

    // Choice rows: options from the top of the left column, then the scroll unit and the stroke count; results
    // under the ranges.
    struct ChoicePlace
    {
        float left, top;
    };
    [[nodiscard]] ChoicePlace ChoiceAt(const std::size_t index) noexcept
    {
        if (index >= ResultFlagBegin && index < RecordFilterFlagCount)
            return {RightLeft, ResultsTop + ChoiceTop + ChoicePitch * static_cast<float>(index - ResultFlagBegin)};
        const std::size_t slot = index == RecordFilterFlagCount ? ResultFlagBegin : index;
        return {0, ChoiceTop + ChoicePitch * static_cast<float>(slot)};
    }
    constexpr float StrokeTop = ChoiceTop + ChoicePitch * static_cast<float>(ResultFlagBegin + 1);
    static_assert(StrokeTop + ChoiceHeight <= BodyHeight, "The stroke row fits in the left column.");
    [[nodiscard]] float RangeY(const std::size_t row) noexcept
    {
        return RangeTop + RangePitch * static_cast<float>(row);
    }
} // namespace

void RecordFilterPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window,
                                   ScreenVisual2DManager *visuals)
{
    canvas_ = &canvas;
    texts_ = &texts;
    modal_.Create(canvas, {"RecordFilter", Width, true, true, visuals});
    auto &body = modal_.Body();
    optionsHeading_ = &CreateText(body, "RecordFilter.Options", 14, 900, Ink);
    rangesHeading_ = &CreateText(body, "RecordFilter.Ranges", 14, 900, Ink);
    resultsHeading_ = &CreateText(body, "RecordFilter.Results", 14, 900, Ink);
    minimum_ = &CreateText(body, "RecordFilter.Minimum", 13, 700, Caption);
    maximum_ = &CreateText(body, "RecordFilter.Maximum", 13, 700, Caption);
    for (std::size_t i = 0; i < ChoiceCount; ++i)
    {
        const auto name = "RecordFilter.Choice." + std::to_string(i);
        choiceLabels_[i] = &CreateText(body, name + ".Label", 13, 600, Ink);
        choiceRules_[i] = &CreateRule(body, name + ".Rule");
        for (std::size_t item = 0; item < 3; ++item)
            choices_[i][item] = &CreateTab(body, name + "." + std::to_string(item), {}, 12);
    }
    strokeLabel_ = &CreateText(body, "RecordFilter.MaxStrokes.Label", 13, 600, Ink);
    strokeRule_ = &CreateRule(body, "RecordFilter.MaxStrokes.Rule");
    for (std::size_t item = 0; item < StrokeItemCount; ++item)
        strokeTabs_[item] = &CreateTab(body, "RecordFilter.MaxStrokes." + std::to_string(item), {}, 12);
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
    {
        switches_[row] = &CreateSwitch(body, "RecordFilter.Switch." + std::to_string(row));
        rowLabels_[row] = &CreateText(body, "RecordFilter.RangeLabel." + std::to_string(row), 13, 600, Ink);
    }
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        fields_[i] = &CreateField(body, "RecordFilter.Field." + std::to_string(i), true);
        if (!window)
            continue;
        inputs_[i] = window->CreateTextInput();
        if (!inputs_[i])
            throw std::runtime_error("Cannot create record filter input.");
        inputs_[i]->SetColors(0x16294A, 0xFFFFFF);
        inputs_[i]->SetPlaceholder(Placeholders[i]);
    }
    RefreshTexts();
    Layout();
}

void RecordFilterPanel::Open(const RecordFilter &filter)
{
    draft_ = FormatRecordFilter(filter);
    error_.clear();
    applied_.reset();
    modal_.SetVisible(true);
    RefreshTexts();
    RefreshValues();
    Layout();
}

void RecordFilterPanel::Close() noexcept
{
    for (auto &input : inputs_)
        if (input)
        {
            input->Blur();
            input->SetVisible(false);
        }
    modal_.SetVisible(false);
}

void RecordFilterPanel::Shutdown() noexcept
{
    Close();
    for (auto &input : inputs_)
        input.reset();
    modal_.Forget();
    canvas_ = nullptr;
    texts_ = nullptr;
}

void RecordFilterPanel::SetViewport(const std::uint32_t height)
{
    height_ = height;
    if (canvas_)
        Layout();
}

void RecordFilterPanel::Layout()
{
    if (!canvas_ || height_ == 0)
        return;
    modal_.Layout(BodyHeight);
    const float h = BodyHeight;
    Place(*optionsHeading_, {0, 0, ColumnWidth, HeadingHeight}, h);
    Place(*rangesHeading_, {RightLeft, 0, ColumnWidth, HeadingHeight}, h);
    Place(*resultsHeading_, {RightLeft, ResultsTop, ColumnWidth, HeadingHeight}, h);
    Place(*minimum_, {MinimumLeft, RangeHeadTop, FieldWidth, 16}, h);
    Place(*maximum_, {MaximumLeft, RangeHeadTop, FieldWidth, 16}, h);
    for (std::size_t i = 0; i < ChoiceCount; ++i)
    {
        const auto place = ChoiceAt(i);
        Place(*choiceLabels_[i], {place.left, place.top, ColumnWidth - 120, ChoiceHeight}, h);
        // Tabs sit at the right end of the column over a 1px rule.
        float right = place.left + ColumnWidth;
        float tabsLeft = right;
        for (std::size_t item = 3; item-- > 0;)
        {
            auto &tab = *choices_[i][item];
            const float width = EstimateWidth(tab.GetComponent<TextVisualComponent>()->Text(), 12) + 4;
            Place(tab, {right - width, place.top, width, TabHeight}, h);
            SetTabSelected(tab, draft_.choices[i] == item);
            tabsLeft = right - width;
            right -= width + TabGap;
        }
        Place(*choiceRules_[i], {tabsLeft, place.top + TabHeight - 1, place.left + ColumnWidth - tabsLeft, 1}, h);
    }
    LayoutStrokes(h);
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
    {
        const float top = RangeY(row);
        Place(*switches_[row], {RightLeft, top + 8, 40, 24}, h);
        Place(*rowLabels_[row], {LabelLeft, top, MinimumLeft - LabelLeft - 4, RowHeight}, h);
    }
    for (std::size_t i = 0; i + 1 < fields_.size(); ++i)
        PlaceField(*fields_[i], {i % 2 == 0 ? MinimumLeft : MaximumLeft, RangeY(i / 2), FieldWidth, RowHeight}, h);
    PlaceField(*fields_.back(), {MinimumLeft, RangeY(RecordFilterValueRowCount - 1), MaximumLeft + FieldWidth - MinimumLeft,
                                 RowHeight},
               h);
    RefreshRows();
    UpdateInputs();
}

void RecordFilterPanel::LayoutStrokes(const float h)
{
    // Same look as the choice rows: label on the left, tabs at the right end of the column over a rule.
    Place(*strokeLabel_, {0, StrokeTop, ColumnWidth - 160, ChoiceHeight}, h);
    float right = ColumnWidth;
    float tabsLeft = right;
    for (std::size_t item = StrokeItemCount; item-- > 0;)
    {
        auto &tab = *strokeTabs_[item];
        const float width = std::max(EstimateWidth(tab.GetComponent<TextVisualComponent>()->Text(), 12) + 4, 18.0F);
        Place(tab, {right - width, StrokeTop, width, TabHeight}, h);
        tabsLeft = right - width;
        right -= width + TabGap;
    }
    Place(*strokeRule_, {tabsLeft, StrokeTop + TabHeight - 1, ColumnWidth - tabsLeft, 1}, h);
}

void RecordFilterPanel::UpdateInputs()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        if (!inputs_[i])
            continue;
        PlaceTextInput(*inputs_[i], *fields_[i], *canvas_, height_, modal_.Scale(), true);
        inputs_[i]->SetVisible(IsVisible());
    }
}

void RecordFilterPanel::RefreshTexts()
{
    if (!canvas_ || !optionsHeading_)
        return;
    modal_.SetTexts(texts_->Get("RecordFilter.title"), texts_->Get("RecordFilter.reset"),
                    texts_->Get("RecordFilter.cancel"), texts_->Get("RecordFilter.apply"));
    SetText(*optionsHeading_, texts_->Get("RecordFilter.options"));
    SetText(*rangesHeading_, texts_->Get("RecordFilter.ranges"));
    SetText(*resultsHeading_, texts_->Get("RecordFilter.results"));
    SetText(*minimum_, texts_->Get("RecordFilter.minimum"));
    SetText(*maximum_, texts_->Get("RecordFilter.maximum"));
    const std::array<std::wstring, 3> flagItems{std::wstring{texts_->Get("RecordFilter.any")},
                                                std::wstring{texts_->Get("RecordFilter.on")},
                                                std::wstring{texts_->Get("RecordFilter.off")}};
    const std::array<std::wstring, 3> unitItems{flagItems[0], L"x", L"BPM"};
    for (std::size_t i = 0; i < ChoiceCount; ++i)
    {
        SetText(*choiceLabels_[i], texts_->Get(ChoiceKeys[i]));
        const auto &items = i == RecordFilterFlagCount ? unitItems : flagItems;
        for (std::size_t item = 0; item < 3; ++item)
            SetText(*choices_[i][item], items[item]);
    }
    SetText(*strokeLabel_, texts_->Get("PlayOptions.maxStrokes"));
    SetText(*strokeTabs_[0], flagItems[0]);
    for (std::size_t item = 1; item < StrokeItemCount; ++item)
        SetText(*strokeTabs_[item], std::to_wstring(item));
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
        SetText(*rowLabels_[row], texts_->Get(RowKeys[row]));
    modal_.SetStatus(error_.empty() ? std::wstring{}
                                    : std::wstring{texts_->Get("RecordFilter.error")} +
                                          song_select::DecodeDisplayText(error_));
    if (IsVisible())
        Layout();
}

void RecordFilterPanel::RefreshValues()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        SetFieldText(*fields_[i], inputs_[i] ? std::wstring_view{} : std::wstring_view{draft_.values[i]}, true);
        if (inputs_[i])
            inputs_[i]->SetText(draft_.values[i]);
    }
    RefreshRows();
}

void RecordFilterPanel::RefreshRows()
{
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
        SetSwitch(*switches_[row], draft_.IsRowOn(row), draft_.HasValue(row));
    for (std::size_t i = 0; i < ChoiceCount; ++i)
        for (std::size_t item = 0; item < 3; ++item)
            SetTabSelected(*choices_[i][item], draft_.choices[i] == item);
    for (std::size_t item = 0; item < StrokeItemCount; ++item)
        SetTabSelected(*strokeTabs_[item], draft_.maxStrokes == item);
    for (std::size_t i = 0; i < fields_.size(); ++i)
        StyleField(*fields_[i], inputs_[i] && inputs_[i]->HasFocus(), true);
}

void RecordFilterPanel::SynchronizeText()
{
    bool changed = false;
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        std::wstring text;
        if (!inputs_[i] || !inputs_[i]->TakeText(text))
            continue;
        draft_.SetValue(i, std::move(text));
        changed = true;
    }
    if (changed)
        RefreshRows();
}

void RecordFilterPanel::TryApply()
{
    for (auto &input : inputs_)
        if (input)
            input->Blur();
    SynchronizeText();
    try
    {
        applied_ = ParseRecordFilter(draft_);
        Close();
    }
    catch (const std::exception &exception)
    {
        error_ = exception.what();
        RefreshTexts();
    }
}

void RecordFilterPanel::Reset()
{
    for (auto &input : inputs_)
        if (input)
            input->Blur();
    draft_ = {};
    error_.clear();
    RefreshValues();
    RefreshTexts();
}

bool RecordFilterPanel::ProcessAction(const Action &action)
{
    if (!IsVisible() || action.type != ActionType::Clicked)
        return false;
    if (modal_.IsPrimary(action.source))
    {
        TryApply();
        return true;
    }
    if (modal_.IsClose(action.source) || modal_.IsCancel(action.source))
    {
        Close();
        return true;
    }
    if (modal_.IsReset(action.source))
    {
        Reset();
        return true;
    }
    for (std::size_t i = 0; i < ChoiceCount; ++i)
        for (std::size_t item = 0; item < 3; ++item)
            if (action.source == choices_[i][item]->Id())
            {
                draft_.choices[i] = item;
                RefreshRows();
                return true;
            }
    for (std::size_t item = 0; item < StrokeItemCount; ++item)
        if (action.source == strokeTabs_[item]->Id())
        {
            draft_.maxStrokes = item;
            RefreshRows();
            return true;
        }
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
        if (action.source == switches_[row]->Id())
        {
            SynchronizeText();
            if (draft_.ToggleRow(row))
                RefreshRows();
            return true;
        }
    return false;
}

void RecordFilterPanel::ProcessInput(const InputState &input)
{
    if (!IsVisible())
        return;
    UpdateInputs();
    SynchronizeText();
    bool focus = false, submit = false, cancel = false;
    for (auto &field : inputs_)
        if (field)
        {
            focus = field->HasFocus() || focus;
            submit = field->TakeSubmit() || submit;
            cancel = field->TakeCancel() || cancel;
        }
    RefreshRows();
    if (cancel || (!focus && input.WasKeyPressed(VK_ESCAPE)))
        Close();
    else if (submit || (!focus && input.WasKeyPressed(VK_RETURN)))
        TryApply();
}

std::optional<RecordFilter> RecordFilterPanel::TakeApplied()
{
    return std::exchange(applied_, std::nullopt);
}
