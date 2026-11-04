#include "PlayOptionsPanel.h"
#include "Presentation/UiColors.h"
#include <algorithm>
#include <charconv>
#include <format>
#include <utility>

namespace
{
    constexpr std::array FieldKeys{"scrollSpeed", "accelerate", "judgeLevel", "seed", "probability"};
    constexpr std::array FlagKeys{"mirror", "random", "ignoreScroll", "metronome", "musicPitch", "hitPitch", "overrideJudge"};
    constexpr std::array GroupKeys{"difficultyGroup", "layoutGroup", "assistGroup"};
    template<class T> T Number(const std::wstring &value)
    {
        std::string ascii;
        for (const auto ch : value)
        {
            if (ch > 127) throw std::invalid_argument("Enter an ASCII number (decimal point: .).");
            ascii.push_back(static_cast<char>(ch));
        }
        T number{};
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != ascii.data() + ascii.size())
            throw std::invalid_argument("Enter a valid number (decimal point: .).");
        return number;
    }
    void Style(Visual2DNode &node, bool selected = false)
    {
        VisualStyle style{};
        style.normal = selected ? ui_colors::sky::Accent : ui_colors::sky::Pale;
        style.hovered = selected ? ui_colors::sky::AccentHover : ui_colors::sky::Focus;
        style.pressed = ui_colors::sky::Accent;
        node.GetComponent<SpriteVisualComponent>()->SetStyle(style);
        if (auto *text = node.GetComponent<TextVisualComponent>())
            text->SetTextColor(selected ? ui_colors::White : ui_colors::Ink);
    }
    void Text(Visual2DNode &node, std::wstring_view value)
    {
        node.GetComponent<TextVisualComponent>()->SetText(std::wstring(value));
    }
}

Visual2DNode &PlayOptionsPanel::Label(Visual2DNode &parent, std::string name, float size)
{
    auto &node = CreateLabel(parent, {}, L"", std::move(name));
    auto &text = *node.GetComponent<TextVisualComponent>();
    text.SetFontSize(size);
    text.SetTextColor(ui_colors::Ink);
    texts_->ApplyFont(text);
    return node;
}

void PlayOptionsPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts, IWindowControl *window)
{
    canvas_ = &canvas;
    texts_ = &texts;
    overlay_ = &CreatePanel(canvas.Root(), {}, "PlayOptions.Overlay");
    overlay_->SetZIndex(1000);
    VisualStyle backdrop{};
    backdrop.normal = backdrop.hovered = backdrop.pressed = {0, 0, 0, .3F};
    overlay_->GetComponent<SpriteVisualComponent>()->SetStyle(backdrop);
    overlay_->AddComponent<RectangleCollider2DComponent>();
    overlay_->AddComponent<PointerReceiverComponent>();
    panel_ = &CreatePanel(*overlay_, {}, "PlayOptions.Panel");
    VisualStyle surface{};
    surface.normal = surface.hovered = surface.pressed = ui_colors::sky::Surface;
    panel_->GetComponent<SpriteVisualComponent>()->SetStyle(surface);
    title_ = &Label(*panel_, "PlayOptions.Title", 26);
    close_ = &CreateButton(*panel_, {}, L"", "PlayOptions.Close");
    reset_ = &CreateButton(*panel_, {}, L"", "PlayOptions.Reset");
    status_ = &Label(*panel_, "PlayOptions.Status", 14);
    viewport_ = &panel_->CreateChild("PlayOptions.Viewport");
    content_ = &viewport_->CreateChild("PlayOptions.Content");
    for (std::size_t i = 0; i < headings_.size(); ++i)
        headings_[i] = &Label(*content_, "PlayOptions.Group." + std::to_string(i), 22);
    unitLabel_ = &Label(*content_, "PlayOptions.UnitLabel");
    unit_ = &CreateComboBox(*content_, {}, {L"x", L"BPM"}, "PlayOptions.Unit");
    Style(*unit_);
    unit_->SetZIndex(10);
    auto &combo = *unit_->GetComponent<ComboBoxBehaviorComponent>();
    combo.SetMaxVisibleItems(2);
    combo.SetItemHeight(34);
    combo.SetFontSize(16);
    combo.SetTextColor(ui_colors::Ink);
    combo.SetSelectedTextColor(ui_colors::Ink);
    combo.SetPopupBackgroundColor(ui_colors::White);
    for (std::size_t i = 0; i < fields_.size(); ++i)
    {
        const auto name = std::string("PlayOptions.") + FieldKeys[i];
        labels_[i] = i == Rate ? &CreateButton(*content_, {}, L"", name + ".Label")
                              : &Label(*content_, name + ".Label");
        if (i == Rate) Style(*labels_[i]);
        labels_[i]->GetComponent<TextVisualComponent>()->SetFontSize(16);
        fields_[i] = &CreateButton(*content_, {}, L"", name + ".Field");
        Style(*fields_[i]);
        auto &text = *fields_[i]->GetComponent<TextVisualComponent>();
        text.SetFontSize(16);
        text.SetHorizontalAlignment(TextAlignment::Leading);
        if (window)
        {
            inputs_[i] = window->CreateTextInput();
            if (!inputs_[i]) throw std::runtime_error("Cannot create play option input.");
            inputs_[i]->SetColors(0x144785, 0xEFF8FE);
        }
    }
    for (std::size_t i = 0; i < flags_.size(); ++i)
    {
        flags_[i] = &CreateToggle(*content_, {}, L"", false, std::string("PlayOptions.") + FlagKeys[i]);
        Style(*flags_[i]);
        auto &text = *flags_[i]->GetComponent<TextVisualComponent>();
        text.SetFontSize(16);
        text.SetHorizontalAlignment(TextAlignment::Leading);
    }
    for (auto *button : {close_, reset_})
    {
        Style(*button);
        button->GetComponent<TextVisualComponent>()->SetFontSize(16);
    }
    Dismiss();
    RefreshTexts();
    Layout();
}

bool &PlayOptionsPanel::FlagValue(Flag flag)
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

void PlayOptionsPanel::Open(const PlayOptions &options)
{
    draft_ = options;
    overrideJudge_ = options.judgeLevel.has_value();
    applied_.reset();
    error_.clear();
    errorField_.reset();
    rateExpanded_ = false;
    progress_ = scroll_ = 0;
    overlay_->SetVisible(true);
    RefreshValues();
    RefreshTexts();
    Layout();
}

void PlayOptionsPanel::Reset()
{
    for (auto &input : inputs_) if (input) input->Blur();
    draft_ = {};
    overrideJudge_ = rateExpanded_ = false;
    error_.clear();
    errorField_.reset();
    scroll_ = 0;
    unit_->GetComponent<ComboBoxBehaviorComponent>()->Collapse();
    RefreshValues();
    RefreshTexts();
    Layout();
}

void PlayOptionsPanel::RefreshValues()
{
    numbers_ = {std::format(L"{:g}", draft_.scrollSpeed), std::format(L"{:g}", draft_.accelerate),
                std::format(L"{:.2f}", draft_.judgeLevel.value_or(JudgeLevel{}).Value()),
                std::to_wstring(draft_.randomSeed), std::format(L"{:g}", draft_.randomAttemptProbability * 100)};
    for (std::size_t i = 0; i < inputs_.size(); ++i) if (inputs_[i]) inputs_[i]->SetText(numbers_[i]);
    unit_->GetComponent<ComboBoxBehaviorComponent>()->SetSelectedIndex(draft_.scrollUnit == PlayOptions::ScrollUnit::Bpm ? 1 : 0);
    for (std::size_t i = 0; i < flags_.size(); ++i)
        flags_[i]->GetComponent<ToggleBehaviorComponent>()->SetChecked(FlagValue(static_cast<Flag>(i)));
}

void PlayOptionsPanel::SynchronizeText()
{
    for (std::size_t i = 0; i < inputs_.size(); ++i)
    {
        std::wstring text;
        if (inputs_[i] && inputs_[i]->TakeText(text)) numbers_[i] = std::move(text);
    }
}

void PlayOptionsPanel::Close()
{
    if (!IsVisible()) return;
    unit_->GetComponent<ComboBoxBehaviorComponent>()->Collapse();
    for (auto &input : inputs_) if (input) input->Blur();
    SynchronizeText();
    auto candidate = draft_;
    // Validate each assignment to associate domain errors with the edited field.
    Field field = Speed;
    try
    {
        candidate.scrollSpeed = Number<double>(numbers_[field]); candidate.Validate();
        field = Rate;
        candidate.accelerate = Number<double>(numbers_[field]); candidate.Validate();
        field = Seed;
        candidate.randomSeed = Number<std::uint64_t>(numbers_[field]);
        field = Probability;
        candidate.randomAttemptProbability = Number<double>(numbers_[field]) / 100; candidate.Validate();
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
        const auto bounds = fields_[field]->BoundsInCanvas();
        const auto viewport = viewport_->BoundsInCanvas();
        if (bounds.y < viewport.y) scroll_ += viewport.y - bounds.y;
        else if (bounds.y + bounds.height > viewport.y + viewport.height)
            scroll_ -= bounds.y + bounds.height - viewport.y - viewport.height;
        Layout();
        if (inputs_[field]) inputs_[field]->Focus(true);
    }
}

void PlayOptionsPanel::Dismiss() noexcept
{
    for (auto &input : inputs_) if (input) input->SetVisible(false);
    if (overlay_) overlay_->SetVisible(false);
    if (unit_) unit_->GetComponent<ComboBoxBehaviorComponent>()->Collapse();
}

void PlayOptionsPanel::Shutdown() noexcept
{
    Dismiss();
    for (auto &input : inputs_) input.reset();
    canvas_ = nullptr;
    texts_ = nullptr;
    overlay_ = panel_ = unit_ = nullptr;
    applied_.reset();
}

void PlayOptionsPanel::RefreshTexts()
{
    if (!panel_) return;
    const auto set = [this](Visual2DNode &node, std::string_view key) {
        Text(node, texts_->Get(std::string("PlayOptions.") + std::string(key)));
        texts_->ApplyFont(*node.GetComponent<TextVisualComponent>());
    };
    set(*title_, "title"); set(*close_, "close"); set(*reset_, "reset"); set(*unitLabel_, "scrollUnit");
    for (std::size_t i = 0; i < headings_.size(); ++i) set(*headings_[i], GroupKeys[i]);
    for (std::size_t i = 0; i < labels_.size(); ++i) set(*labels_[i], FieldKeys[i]);
    Text(*labels_[Rate], std::wstring(texts_->Get("PlayOptions.accelerate")) + (rateExpanded_ ? L"  −" : L"  +"));
    for (std::size_t i = 0; i < flags_.size(); ++i) set(*flags_[i], FlagKeys[i]);
    for (auto *field : fields_) texts_->ApplyFont(*field->GetComponent<TextVisualComponent>());
    texts_->ApplyFont(*unit_->GetComponent<ComboBoxBehaviorComponent>());
    if (errorField_)
    {
        const std::wstring detail(error_.begin(), error_.end()); // Existing numeric/domain diagnostics are ASCII.
        Text(*status_, texts_->Format("PlayOptions.invalidValue", texts_->Get(std::string("PlayOptions.") + FieldKeys.at(*errorField_)), detail));
    }
    else Text(*status_, texts_->Get("PlayOptions.closeHint"));
    auto &status = *status_->GetComponent<TextVisualComponent>();
    texts_->ApplyFont(status);
    status.SetTextColor(errorField_ ? ui_colors::Error : ui_colors::sky::Muted);
}

bool PlayOptionsPanel::Accepts(NodeId source) const noexcept
{
    const auto *node = canvas_->FindNode(source);
    for (; node; node = node->Parent())
    {
        if (!node->IsVisible() || !node->IsEnabled()) return false;
        if (node == panel_) return true;
    }
    return false;
}

bool PlayOptionsPanel::ProcessAction(const Action &action)
{
    if (!IsVisible() || progress_ < 1 || !Accepts(action.source)) return false;
    SynchronizeText();
    if (action.type == ActionType::SelectionChanged && action.source == unit_->Id() && action.selectedIndex < 2)
    {
        draft_.scrollUnit = action.selectedIndex == 1 ? PlayOptions::ScrollUnit::Bpm : PlayOptions::ScrollUnit::Multiplier;
        return true;
    }
    if (action.type == ActionType::ValueChanged)
        for (std::size_t i = 0; i < flags_.size(); ++i) if (action.source == flags_[i]->Id())
        {
            FlagValue(static_cast<Flag>(i)) = action.value != 0;
            flags_[i]->GetComponent<ToggleBehaviorComponent>()->SetChecked(action.value != 0);
            Layout();
            return true;
        }
    if (action.type != ActionType::Clicked) return false;
    if (action.source == close_->Id()) { Close(); return true; }
    if (action.source == reset_->Id()) { Reset(); return true; }
    if (action.source == labels_[Rate]->Id())
    {
        rateExpanded_ = !rateExpanded_;
        RefreshTexts(); Layout(); return true;
    }
    for (std::size_t i = 0; i < fields_.size(); ++i) if (action.source == fields_[i]->Id())
    {
        unit_->GetComponent<ComboBoxBehaviorComponent>()->Collapse();
        UpdateInputs();
        if (inputs_[i]) inputs_[i]->Focus(false); // Preserve native caret/selection after its mouse click.
        return true;
    }
    return false;
}

void PlayOptionsPanel::ProcessInput(const InputState &input, std::optional<Point> pointer)
{
    if (!IsVisible()) return;
    SynchronizeText();
    bool focused = false, submit = false, cancel = false;
    for (auto &field : inputs_) if (field)
    {
        focused |= field->HasFocus();
        submit |= field->TakeSubmit();
        cancel |= field->TakeCancel();
    }
    if (cancel || submit || (!focused && (input.WasKeyPressed(VK_ESCAPE) || input.WasKeyPressed(VK_RETURN))))
    { Close(); return; }
    if (pointer && viewport_->BoundsInCanvas().Contains(*pointer) && input.MouseWheelDelta() != 0)
    {
        scroll_ -= input.MouseWheelDelta() * 36;
        Layout();
    }
    UpdateInputs();
}

void PlayOptionsPanel::Update(double deltaSeconds)
{
    if (!IsVisible()) return;
    if (progress_ < 1)
    {
        progress_ = std::clamp(progress_ + static_cast<float>(deltaSeconds / .2), 0.0F, 1.0F);
        Layout();
    }
}

std::optional<PlayOptions> PlayOptionsPanel::TakeApplied() { return std::exchange(applied_, std::nullopt); }
