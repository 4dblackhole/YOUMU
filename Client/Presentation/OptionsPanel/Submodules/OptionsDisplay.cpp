#include "OptionsDisplay.h"
#include "App/SkinSetSelection.h"
#include "OptionsPanelStyle.h"
#include <algorithm>
#include <array>

using namespace PhantomCore;

using namespace options_ui;

namespace
{
    // In drop-down order.
    constexpr std::array RenderRateChoices{RenderRateMode::Display, RenderRateMode::DisplayDouble,
                                           RenderRateMode::Unlimited};
    constexpr std::size_t KeyBeamOpacitySlider = 0, BackgroundBrightnessSlider = 1;
} // namespace

std::vector<float> OptionsDisplay::FieldHeights() const
{
    constexpr float control = FieldHeight(ControlHeight);
    // Skin, resolution, window mode, screen position, notation, frame limit, key beam opacity, background brightness.
    return {control, control, control, FieldHeight(PositionControlHeight), control, control, control, control};
}

void OptionsDisplay::Create()
{
    CreateSkinControls();
    resolutionCombo_ = &CreateComboField("Resolution", resolutionLabel_);
    windowModeCombo_ = &CreateComboField("WindowMode", windowModeLabel_);
    CreatePositionControls();
    // Song and chart texts: the first notation of the file, or the romanized line.
    notationCombo_ = &CreateComboField("Notation", notationLabel_);
    // How often the engine renders: the display refresh rate, twice that rate, or after every update.
    renderRateCombo_ = &CreateComboField("RenderRate", renderRateLabel_);
    sliders_[KeyBeamOpacitySlider].Create(
        context_,
        {[](const OptionValues &values) { return values.keyBeamOpacity; },
         [](OptionValues &values, const int percent) { values.keyBeamOpacity = percent; }, "Options.keyBeamOpacity"},
        "Options.KeyBeamOpacity");
    sliders_[BackgroundBrightnessSlider].Create(
        context_,
        {[](const OptionValues &values) { return values.backgroundBrightness; },
         [](OptionValues &values, const int percent) { values.backgroundBrightness = percent; },
         "Options.backgroundBrightness"},
        "Options.BackgroundBrightness");
}

void OptionsDisplay::CreateSkinControls()
{
    skinNames_ = SkinSetSelection::Instance().AvailableNames();
    skinCombo_ = &CreateComboField("Skin", skinLabel_, skinNames_);
}

void OptionsDisplay::Refresh()
{
    SetCaption(*skinLabel_, std::wstring(Options(context_.Texts()).skin));
    auto &skinCombo = RequireDropdown(*skinCombo_);
    const auto selected = std::ranges::find(skinNames_, SkinSetSelection::Instance().CurrentName());
    if (selected != skinNames_.end())
        skinCombo.SetSelectedIndex(static_cast<std::size_t>(selected - skinNames_.begin()));
    for (auto &slider : sliders_)
        slider.Refresh(context_);
    RefreshControls();
}

void OptionsDisplay::RefreshSkinSets()
{
    skinNames_ = SkinSetSelection::Instance().AvailableNames();
    if (skinCombo_ == nullptr)
        return;
    RequireDropdown(*skinCombo_).SetItems(skinNames_);
    context_.RefreshTexts();
}

void OptionsDisplay::RefreshControls()
{
    auto &texts = context_.Texts();
    const auto &saved = context_.Settings().Values();
    auto *window = context_.Window();
    SetCaption(*resolutionLabel_, std::wstring(Options(texts).resolution));
    resolutions_.clear();
    const auto current = window ? window->ClientSize() : WindowResolution{saved.windowWidth, saved.windowHeight};
    try
    {
        if (window)
            resolutions_ = window->DisplayResolutions();
    }
    catch (const std::exception &error)
    {
        context_.SetStatus(OptionsStatus::WindowFailed);
        context_.StatusDetail() = error.what();
    }
    if (std::ranges::find(resolutions_, current) == resolutions_.end())
        resolutions_.push_back(current);
    std::vector<std::wstring> names;
    for (const auto size : resolutions_)
        names.push_back(std::to_wstring(size.width) + L" × " + std::to_wstring(size.height));
    auto &combo = RequireDropdown(*resolutionCombo_);
    combo.SetItems(std::move(names));
    combo.SetSelectedIndex(static_cast<std::size_t>(std::ranges::find(resolutions_, current) - resolutions_.begin()));
    resolutionCombo_->SetEnabled(window != nullptr);
    SetCaption(*windowModeLabel_, std::wstring(texts.Get("Options.windowMode")));
    auto &windowMode = RequireDropdown(*windowModeCombo_);
    windowMode.SetItems({std::wstring(texts.Get("Options.windowModeWindowed")),
                         std::wstring(texts.Get("Options.windowModeBorderless"))});
    const auto shownMode = window ? window->Mode() : saved.windowMode;
    windowMode.SetSelectedIndex(shownMode == WindowMode::Borderless ? 1 : 0);
    windowModeCombo_->SetEnabled(window != nullptr);
    RefreshPositionControls();
    SetCaption(*notationLabel_, std::wstring(texts.Get("Options.notation")));
    auto &notation = RequireDropdown(*notationCombo_);
    notation.SetItems({std::wstring(texts.Get("Options.notationFirst")), std::wstring(texts.Get("Options.notationRomanized"))});
    notation.SetSelectedIndex(saved.notation == TextNotation::Romanized ? 1 : 0);
    SetCaption(*renderRateLabel_, std::wstring(texts.Get("Options.renderRate")));
    auto &renderRate = RequireDropdown(*renderRateCombo_);
    renderRate.SetItems({std::wstring(texts.Get("Options.renderRateDisplay")),
                         std::wstring(texts.Get("Options.renderRateDouble")),
                         std::wstring(texts.Get("Options.renderRateUnlimited"))});
    const auto shown = std::ranges::find(RenderRateChoices, saved.renderRate);
    renderRate.SetSelectedIndex(shown == RenderRateChoices.end() ? 0
                                                                  : static_cast<std::size_t>(shown - RenderRateChoices.begin()));
}

bool OptionsDisplay::ProcessAction(const Action &action)
{
    if (action.type == ActionType::Clicked)
        return ProcessPositionAction(action);
    if (action.type != ActionType::SelectionChanged)
        return false;
    if (action.source == skinCombo_->Id())
        return SelectSkin(action.selectedIndex);
    // The window and text choices end any number being typed first.
    const std::array choices{std::pair{resolutionCombo_, &OptionsDisplay::SelectResolution},
                             std::pair{windowModeCombo_, &OptionsDisplay::SelectWindowMode},
                             std::pair{notationCombo_, &OptionsDisplay::SelectNotation},
                             std::pair{renderRateCombo_, &OptionsDisplay::SelectRenderRate}};
    for (const auto &[combo, select] : choices)
        if (action.source == combo->Id())
        {
            context_.CancelKeyboardEdit();
            (this->*select)(action.selectedIndex);
            return true;
        }
    return false;
}

bool OptionsDisplay::SelectSkin(const std::size_t index)
{
    if (index >= skinNames_.size())
        return false;
    auto candidate = context_.Settings().Values();
    candidate.skinSet = skinNames_[index];
    if (context_.Save(candidate))
    {
        std::string error;
        if (!SkinSetSelection::Instance().Select(candidate.skinSet, error))
        {
            context_.SetStatus(OptionsStatus::SaveFailed);
            context_.StatusDetail() = std::move(error);
        }
        context_.SkinChanged();
    }
    context_.RefreshTexts();
    return true;
}

void OptionsDisplay::SelectRenderRate(const std::size_t index)
{
    if (index >= RenderRateChoices.size())
        return;
    auto candidate = context_.Settings().Values();
    candidate.renderRate = RenderRateChoices[index];
    // The game applies the saved value to the engine on its next update.
    static_cast<void>(context_.Save(candidate));
    RefreshControls();
    context_.RefreshStatus();
}

void OptionsDisplay::SelectNotation(const std::size_t index)
{
    auto candidate = context_.Settings().Values();
    candidate.notation = index == 1 ? TextNotation::Romanized : TextNotation::First;
    // Screens read the saved value, so a failed save leaves the shown notation unchanged.
    static_cast<void>(context_.Save(candidate));
    RefreshControls();
    context_.RefreshStatus();
}

void OptionsDisplay::SelectWindowMode(const std::size_t index)
{
    auto *window = context_.Window();
    if (!window)
        return;
    const auto previous = window->Mode();
    const auto mode = index == 1 ? WindowMode::Borderless : WindowMode::Windowed;
    if (!window->SetMode(mode, context_.StatusDetail()))
        context_.SetStatus(OptionsStatus::WindowFailed);
    else
    {
        auto candidate = context_.Settings().Values();
        candidate.windowMode = mode;
        if (!context_.Save(candidate))
        {
            std::string error;
            if (!window->SetMode(previous, error))
                context_.StatusDetail() += " | Restore failed: " + error;
        }
    }
    RefreshControls();
    context_.RefreshStatus();
}

void OptionsDisplay::SelectResolution(const std::size_t index)
{
    auto *window = context_.Window();
    if (!window || index >= resolutions_.size())
        return;
    const auto previous = window->ClientSize();
    const auto size = resolutions_[index];
    if (!window->SetClientSize(size, context_.StatusDetail()))
        context_.SetStatus(OptionsStatus::WindowFailed);
    else
    {
        auto candidate = context_.Settings().Values();
        candidate.windowWidth = size.width;
        candidate.windowHeight = size.height;
        if (!context_.Save(candidate))
        {
            std::string error;
            if (!window->SetClientSize(previous, error))
                context_.StatusDetail() += " | Restore failed: " + error;
        }
    }
    RefreshControls();
    context_.RefreshStatus();
}

OptionsPointer OptionsDisplay::ProcessPointer(const InputState &input, const std::optional<Point> canvasPointer,
                                              const NodeId hoveredNode)
{
    for (auto &slider : sliders_)
        if (slider.ProcessPointer(context_, input, canvasPointer, hoveredNode))
            return OptionsPointer::Sliding;
    return ProcessPositionDrag(input, canvasPointer, hoveredNode) ? OptionsPointer::Consumed : OptionsPointer::Free;
}

void OptionsDisplay::HandleKey(const std::uint16_t virtualKey)
{
    EditPosition(virtualKey);
}

void OptionsDisplay::CancelKeyboardEdit()
{
    positionEdit_.reset();
    replacePositionDraft_ = false;
    if (positionBox_)
        RefreshPositionControls();
}

void OptionsDisplay::AppendCombos(std::vector<Visual2DNode *> &combos) const
{
    combos.insert(combos.end(), {skinCombo_, resolutionCombo_, notationCombo_, renderRateCombo_, windowModeCombo_});
}

void OptionsDisplay::OnToggle()
{
    for (auto &slider : sliders_)
        slider.Stop();
    positionDrag_.reset();
}

void OptionsDisplay::OnOpened()
{
    RefreshSkinSets();
    RefreshControls();
}

void OptionsDisplay::OnClosed()
{
    // Only toggling stops a screen position drag; after Escape it still moves the window on its release.
    for (auto &slider : sliders_)
        slider.Stop();
}

void OptionsDisplay::Shutdown() noexcept
{
    skinLabel_ = skinCombo_ = nullptr;
    skinNames_.clear();
    resolutionLabel_ = resolutionCombo_ = nullptr;
    resolutions_.clear();
    windowModeLabel_ = windowModeCombo_ = nullptr;
    notationLabel_ = notationCombo_ = nullptr;
    renderRateLabel_ = renderRateCombo_ = nullptr;
    for (auto &slider : sliders_)
        slider.Shutdown();
    positionLabel_ = positionBox_ = positionMonitor_ = positionPicture_ = positionCenter_ = nullptr;
    positionFields_ = {};
    positionDrag_.reset();
    positionEdit_.reset();
    positionDraft_.clear();
    replacePositionDraft_ = false;
}
