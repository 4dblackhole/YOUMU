#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>

using namespace PhantomCore;

using namespace options_ui;
void OptionsPanel::CreateDisplayControls()
{
    const float top = NextField(ControlHeight);
    resolutionLabel_ = &CreateFieldLabel(*content_, top, "Options.Resolution.Label");
    resolutionCombo_ = &CreateComboBox(
        *content_, TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.Resolution.Combo");
    ConfigureCombo(*resolutionCombo_, 4);
    resolutionCombo_->SetZIndex(static_cast<std::int32_t>(200 - 2 * comboOrder_++));
    AddFieldLine(*content_, *resolutionCombo_, "Options.Resolution.Line");
}

void OptionsPanel::RefreshDisplayControls()
{
    SetCaption(*resolutionLabel_, std::wstring(Options(texts_).resolution));
    resolutions_.clear();
    const auto current =
        window_ ? window_->ClientSize()
                : WindowResolution{options_.Values().windowWidth, options_.Values().windowHeight};
    try
    {
        if (window_)
            resolutions_ = window_->DisplayResolutions();
    }
    catch (const std::exception &error)
    {
        status_ = Status::WindowFailed;
        statusDetail_ = error.what();
    }
    if (std::ranges::find(resolutions_, current) == resolutions_.end())
        resolutions_.push_back(current);
    std::vector<std::wstring> names;
    for (const auto size : resolutions_)
        names.push_back(std::to_wstring(size.width) + L" × " + std::to_wstring(size.height));
    auto &combo = RequireComponent<ComboBoxBehaviorComponent>(*resolutionCombo_);
    combo.SetItems(std::move(names));
    combo.SetSelectedIndex(
        static_cast<std::size_t>(std::ranges::find(resolutions_, current) - resolutions_.begin()));
    texts_.ApplyFont(combo);
    resolutionCombo_->SetEnabled(window_ != nullptr);
}

void OptionsPanel::SelectResolution(std::size_t index)
{
    if (!window_ || index >= resolutions_.size())
        return;
    const auto previous = window_->ClientSize();
    const auto size = resolutions_[index];
    if (!window_->SetClientSize(size, statusDetail_))
        status_ = Status::WindowFailed;
    else
    {
        auto candidate = options_.Values();
        candidate.windowWidth = size.width;
        candidate.windowHeight = size.height;
        if (!SaveOptions(candidate))
        {
            std::string error;
            if (!window_->SetClientSize(previous, error))
                statusDetail_ += " | Restore failed: " + error;
        }
    }
    RefreshDisplayControls();
    RefreshStatus();
}
