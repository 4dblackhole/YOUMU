#include "PlayOptionsPanel.h"
#include "Presentation/UiDropdown.h"
#include <algorithm>
#include <charconv>
#include <format>
#include <utility>

using namespace PhantomCore;
using namespace ui_modal;

namespace
{
    constexpr float Width = 960;
    constexpr std::array FieldKeys{"scrollSpeed", "accelerate", "judgeLevel", "seed", "probability"};
    constexpr std::array FlagKeys{"mirror", "random", "ignoreScroll", "metronome", "musicPitch", "hitPitch", "overrideJudge"};
    constexpr std::array GroupKeys{"difficultyGroup", "layoutGroup", "assistGroup"};
    template <class T> T Number(const std::wstring &value)
    {
        std::string ascii;
        for (const auto ch : value)
        {
            if (ch > 127)
                throw std::invalid_argument("Enter an ASCII number (decimal point: .).");
            ascii.push_back(static_cast<char>(ch));
        }
        T number{};
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != ascii.data() + ascii.size())
            throw std::invalid_argument("Enter a valid number (decimal point: .).");
        return number;
    }
    [[nodiscard]] std::wstring Wide(const std::string_view text)
    {
        return {text.begin(), text.end()};
    }
    [[nodiscard]] std::size_t GaugeIndex(const HpGaugeKind kind) noexcept
    {
        const auto found = std::ranges::find(HpGauges, kind, &HpGaugeDescription::kind);
        return found == HpGauges.end() ? 0 : static_cast<std::size_t>(found - HpGauges.begin());
    }
} // namespace

void PlayOptionsPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window,
                                  ScreenVisual2DManager *visuals)
{
    canvas_ = &canvas;
    texts_ = &texts;
    visuals_ = visuals;
    modal_.Create(canvas, {"PlayOptions", Width, false, false, visuals});
    auto &body = modal_.Body();
    for (std::size_t i = 0; i < headings_.size(); ++i)
        headings_[i] = &CreateText(body, "PlayOptions.Group." + std::to_string(i), 14, 900, Ink);
    for (std::size_t i = 0; i < columnRules_.size(); ++i)
        columnRules_[i] = &CreateRule(body, "PlayOptions.ColumnRule." + std::to_string(i));
    for (std::size_t i = 0; i < subRules_.size(); ++i)
    {
        subRules_[i] = &CreateRule(body, "PlayOptions.SubRule." + std::to_string(i));
        subRules_[i]->GetComponent<SpriteVisualComponent>()->SetStyle(
            {FieldLine, FieldLine, FieldLine, FieldLine});
    }
    units_[0] = &CreateTab(body, "PlayOptions.Unit.0", L"x", 13);
    units_[1] = &CreateTab(body, "PlayOptions.Unit.1", L"BPM", 13);
    rateUnits_[0] = &CreateTab(body, "PlayOptions.RateUnit.0", L"x", 13);
    rateUnits_[1] = &CreateTab(body, "PlayOptions.RateUnit.1", L"BPM", 13);
    rateRange_ = &CreateText(body, "PlayOptions.accelerate.Range", 12, 600, Caption);
    rateMore_ = &CreateButton(body, {}, L"", "PlayOptions.accelerate.More");
    rateMore_->GetComponent<SpriteVisualComponent>()->SetStyle(
        {{1, 1, 1, 0}, ui_colors::Rgb(0xFFFFFF, .8F), ui_colors::Rgb(0xE6EEFC), {1, 1, 1, 0}});
    rateMoreIcon_ = &CreateIcon(*rateMore_, "Icon", visuals, L"Icons.plus", BrandText);
    judgeUnit_ = &CreateText(body, "PlayOptions.judgeLevel.Unit", 13, 400, Caption);
    probabilityUnit_ = &CreateText(body, "PlayOptions.probability.Unit", 13, 400, Caption);
    SetText(*probabilityUnit_, L"%");
    gaugeLabel_ = &CreateText(body, "PlayOptions.HpGauge.Label", 13, 600, Ink);
    gaugeClear_ = &CreateText(body, "PlayOptions.HpGauge.Clear", 12, 600, Caption);
    std::vector<std::wstring> gauges;
    for (const auto &gauge : HpGauges)
        gauges.push_back(Wide(gauge.name));
    gauge_ = &CreateDropdown(body, {}, std::move(gauges), "PlayOptions.HpGauge");
    gauge_->SetZIndex(20);
    gauge_->GetComponent<SpriteVisualComponent>()->SetStyle(
        {ui_colors::Rgb(0xFFFFFF, .74F), ui_colors::White, ui_colors::White, ui_colors::Rgb(0xFFFFFF, .74F)});
    RequireDropdown(*gauge_).SetValueStyle(14, 600, Ink);
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        const auto name = std::string("PlayOptions.") + FieldKeys[i];
        labels_[i] = &CreateText(body, name + ".Label", 13, 600, Ink);
        fields_[i] = &CreateField(body, name + ".Field", i != Seed);
        if (!window)
            continue;
        inputs_[i] = window->CreateTextInput();
        if (!inputs_[i])
            throw std::runtime_error("Cannot create play option input.");
        inputs_[i]->SetColors(0x16294A, 0xFFFFFF);
    }
    labels_[Judge]->SetVisible(false); // The override switch names the JudgeLevel box.
    for (std::size_t i = 0; i < flags_.size(); ++i)
    {
        auto &flag = flags_[i];
        flag.row = &CreateButton(body, {}, L"", std::string("PlayOptions.") + FlagKeys[i]);
        flag.row->GetComponent<SpriteVisualComponent>()->SetStyle({{1, 1, 1, 0}, {1, 1, 1, 0}, {1, 1, 1, 0}, {1, 1, 1, 0}});
        flag.label = &CreateText(*flag.row, "Label", 14, 600, Ink);
        flag.toggle = &CreateSwitch(*flag.row, "Switch");
        flag.rule = &CreateRule(*flag.row, "Rule");
    }
    Dismiss();
    RefreshTexts();
}

bool &PlayOptionsPanel::FlagValue(const Flag flag)
{
    switch (flag)
    {
    case Mirror: return draft_.mirror;
    case Random: return draft_.random;
    case IgnoreScroll: return draft_.ignoreScrollEffects;
    case Metronome: return draft_.metronome;
    case MusicPitch: return draft_.preserveMusicPitch;
    case HitPitch: return draft_.preserveHitSoundPitch;
    case OverrideJudge: return overrideJudge_;
    default: throw std::logic_error("Unknown play option flag.");
    }
}

void PlayOptionsPanel::SetFlag(const Flag flag, const bool value)
{
    FlagValue(flag) = value;
    SetSwitch(*flags_[flag].toggle, value, true);
}

void PlayOptionsPanel::Open(const PlayOptions &options)
{
    draft_ = options;
    overrideJudge_ = options.judgeLevel.has_value();
    applied_.reset();
    error_.clear();
    errorField_.reset();
    rateExpanded_ = false;
    modal_.SetVisible(true);
    RefreshValues();
    RefreshTexts();
    Layout();
}

void PlayOptionsPanel::Reset()
{
    for (auto &input : inputs_)
        if (input)
            input->Blur();
    draft_ = {};
    overrideJudge_ = rateExpanded_ = false;
    error_.clear();
    errorField_.reset();
    RequireDropdown(*gauge_).Collapse();
    RefreshValues();
    RefreshTexts();
    Layout();
}

void PlayOptionsPanel::RefreshValues()
{
    numbers_ = {std::format(L"{:g}", draft_.scrollSpeed), std::format(L"{:g}", draft_.accelerate),
                std::format(L"{:.2f}", draft_.judgeLevel.value_or(JudgeLevel{}).Value()),
                std::to_wstring(draft_.randomSeed), std::format(L"{:g}", draft_.randomAttemptProbability * 100)};
    for (std::size_t i = 0; i < inputs_.size(); ++i)
        if (inputs_[i])
            inputs_[i]->SetText(numbers_[i]);
    RequireDropdown(*gauge_).SetSelectedIndex(GaugeIndex(draft_.hpGauge));
    for (std::size_t i = 0; i < flags_.size(); ++i)
        SetSwitch(*flags_[i].toggle, FlagValue(static_cast<Flag>(i)), true);
}

void PlayOptionsPanel::SynchronizeText()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        std::wstring text;
        if (inputs_[i] && inputs_[i]->TakeText(text))
            numbers_[i] = std::move(text);
    }
}

void PlayOptionsPanel::Close()
{
    if (!IsVisible())
        return;
    RequireDropdown(*gauge_).Collapse();
    for (auto &input : inputs_)
        if (input)
            input->Blur();
    SynchronizeText();
    auto candidate = draft_;
    // Validate each assignment to associate domain errors with the edited field.
    Field field = Speed;
    try
    {
        candidate.scrollSpeed = Number<double>(numbers_[field]);
        candidate.Validate();
        field = Rate;
        candidate.accelerate = Number<double>(numbers_[field]);
        candidate.Validate();
        field = Seed;
        candidate.randomSeed = Number<std::uint64_t>(numbers_[field]);
        field = Probability;
        candidate.randomAttemptProbability = Number<double>(numbers_[field]) / 100;
        candidate.Validate();
        field = Judge;
        candidate.judgeLevel = overrideJudge_ ? std::optional{JudgeLevel{Number<double>(numbers_[field])}} : std::nullopt;
        applied_ = candidate;
        Dismiss();
    }
    catch (const std::invalid_argument &exception)
    {
        error_ = exception.what();
        errorField_ = field;
        RefreshTexts();
        Layout();
        if (inputs_[field])
            inputs_[field]->Focus(true);
    }
}

void PlayOptionsPanel::Dismiss() noexcept
{
    for (auto &input : inputs_)
        if (input)
            input->SetVisible(false);
    if (gauge_)
        RequireDropdown(*gauge_).Collapse();
    modal_.SetVisible(false);
}

void PlayOptionsPanel::Shutdown() noexcept
{
    Dismiss();
    for (auto &input : inputs_)
        input.reset();
    modal_.Forget();
    canvas_ = nullptr;
    texts_ = nullptr;
    gauge_ = nullptr;
    applied_.reset();
}

void PlayOptionsPanel::RefreshTexts()
{
    if (!canvas_ || !gauge_)
        return;
    const auto get = [this](std::string_view key) { return texts_->Get(std::string("PlayOptions.") + std::string(key)); };
    modal_.SetTexts(get("title"), get("reset"), {}, get("done"));
    for (std::size_t i = 0; i < headings_.size(); ++i)
        SetText(*headings_[i], get(GroupKeys[i]));
    for (std::size_t i = 0; i < labels_.size(); ++i)
        SetText(*labels_[i], get(FieldKeys[i]));
    for (std::size_t i = 0; i < flags_.size(); ++i)
        SetText(*flags_[i].label, get(FlagKeys[i]));
    SetText(*rateRange_, get(draft_.accelerateUnit == PlayOptions::ScrollUnit::Bpm ? "accelerateBpmRange" : "accelerateRange"));
    SetText(*judgeUnit_, get("judgeRange"));
    SetText(*gaugeLabel_, get("hpGauge"));
    const auto *gauge = FindHpGauge(draft_.hpGauge);
    SetText(*gaugeClear_, texts_->Format("PlayOptions.clear", std::format(L"{:g}", gauge ? gauge->clearPercent : 0.0)));
    if (errorField_)
    {
        const std::wstring detail(error_.begin(), error_.end()); // Existing numeric/domain diagnostics are ASCII.
        modal_.SetStatus(texts_->Format("PlayOptions.invalidValue", get(FieldKeys.at(*errorField_)), detail));
    }
    else
        modal_.SetStatus({});
}

bool PlayOptionsPanel::ProcessAction(const Action &action)
{
    if (!IsVisible() || !modal_.Owns(action.source))
        return false;
    SynchronizeText();
    if (action.type == ActionType::SelectionChanged && action.source == gauge_->Id())
    {
        if (action.selectedIndex < HpGauges.size())
            draft_.hpGauge = HpGauges[action.selectedIndex].kind;
        RefreshTexts();
        return true;
    }
    if (action.type != ActionType::Clicked)
        return false;
    if (modal_.IsPrimary(action.source))
    {
        Close();
        return true;
    }
    if (modal_.IsReset(action.source))
    {
        Reset();
        return true;
    }
    for (std::size_t i = 0; i < units_.size(); ++i)
        if (action.source == units_[i]->Id())
        {
            draft_.scrollUnit = i == 1 ? PlayOptions::ScrollUnit::Bpm : PlayOptions::ScrollUnit::Multiplier;
            Layout();
            return true;
        }
    // Accelerate as a multiplier or as a target BPM that each chart divides by its base BPM.
    for (std::size_t i = 0; i < rateUnits_.size(); ++i)
        if (action.source == rateUnits_[i]->Id())
        {
            draft_.accelerateUnit = i == 1 ? PlayOptions::ScrollUnit::Bpm : PlayOptions::ScrollUnit::Multiplier;
            RefreshTexts();
            Layout();
            return true;
        }
    for (std::size_t i = 0; i < flags_.size(); ++i)
    {
        const bool row = action.source == flags_[i].row->Id();
        if (!row && action.source != flags_[i].toggle->Id())
            continue;
        const auto flag = static_cast<Flag>(i);
        SetFlag(flag, !FlagValue(flag));
        Layout();
        return true;
    }
    if (action.source == rateMore_->Id())
    {
        rateExpanded_ = !rateExpanded_;
        Layout();
        return true;
    }
    for (std::size_t i = 0; i < fields_.size(); ++i)
        if (action.source == fields_[i]->Id())
        {
            RequireDropdown(*gauge_).Collapse();
            UpdateInputs();
            if (inputs_[i])
                inputs_[i]->Focus(false); // Preserve native caret/selection after its mouse click.
            return true;
        }
    return false;
}

void PlayOptionsPanel::ProcessInput(const InputState &input)
{
    if (!IsVisible())
        return;
    SynchronizeText();
    bool focused = false, submit = false, cancel = false;
    for (auto &field : inputs_)
        if (field)
        {
            focused |= field->HasFocus();
            submit |= field->TakeSubmit();
            cancel |= field->TakeCancel();
        }
    // F2 restores the defaults like Reset, and F1 closes the panel it opened (osu!'s mod select keys).
    if (input.WasKeyPressed(VK_F2))
        Reset();
    const bool closeKey = !focused && (input.WasKeyPressed(VK_ESCAPE) || input.WasKeyPressed(VK_RETURN));
    const bool shortcutClose = closeKey || input.WasKeyPressed(VK_F1);
    if (cancel || submit || shortcutClose)
    {
        Close();
        return;
    }
    UpdateInputs();
}

std::optional<PlayOptions> PlayOptionsPanel::TakeApplied()
{
    return std::exchange(applied_, std::nullopt);
}
