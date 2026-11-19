#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>

using namespace PhantomCore;

using namespace options_ui;
void OptionsPanel::CreateDisplayControls()
{
    const float top = NextField(ControlHeight);
    resolutionLabel_ = &CreateFieldLabel(*content_, top, "Options.Resolution.Label");
    resolutionCombo_ = &CreateDropdown(
        *content_, TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.Resolution.Combo");
    ConfigureCombo(*resolutionCombo_);
    AddFieldLine(*content_, *resolutionCombo_, "Options.Resolution.Line");
    // Song and chart texts: the first notation of the file, or the romanized line.
    const float notationTop = NextField(ControlHeight);
    notationLabel_ = &CreateFieldLabel(*content_, notationTop, "Options.Notation.Label");
    notationCombo_ = &CreateDropdown(
        *content_, TopLeft({ContentLeft, notationTop + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), {},
        "Options.Notation.Combo");
    ConfigureCombo(*notationCombo_);
    AddFieldLine(*content_, *notationCombo_, "Options.Notation.Line");
    CreateKeyBeamOpacityControl();
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
    auto &combo = RequireDropdown(*resolutionCombo_);
    combo.SetItems(std::move(names));
    combo.SetSelectedIndex(
        static_cast<std::size_t>(std::ranges::find(resolutions_, current) - resolutions_.begin()));
    resolutionCombo_->SetEnabled(window_ != nullptr);
    SetCaption(*notationLabel_, std::wstring(texts_.Get("Options.notation")));
    auto &notation = RequireDropdown(*notationCombo_);
    notation.SetItems({std::wstring(texts_.Get("Options.notationFirst")), std::wstring(texts_.Get("Options.notationRomanized"))});
    notation.SetSelectedIndex(options_.Values().notation == TextNotation::Romanized ? 1 : 0);
}

void OptionsPanel::SelectNotation(const std::size_t index)
{
    auto candidate = options_.Values();
    candidate.notation = index == 1 ? TextNotation::Romanized : TextNotation::First;
    // Screens read the saved value, so a failed save leaves the shown notation unchanged.
    static_cast<void>(SaveOptions(candidate));
    RefreshDisplayControls();
    RefreshStatus();
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
