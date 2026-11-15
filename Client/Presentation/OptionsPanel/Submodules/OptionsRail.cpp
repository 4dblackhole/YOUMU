#include "OptionsPanelStyle.h"
#include "Presentation/ModePresentation.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/SkinVisualComponent.h"
#include <algorithm>
#include <cmath>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    constexpr std::array SectionNames{"General", "Display", "Audio", "Input"};
    constexpr std::array SectionIcons{"Icons.settings", "Icons.display", "Icons.volume", "Icons.keyboard"};
    constexpr Color Transparent{1, 1, 1, 0};
    constexpr auto RailHover = ui_colors::Rgb(0xFFFFFF, .07F), RailOn = ui_colors::Rgb(0xFFFFFF, .11F);
    // Songs: path box with Browse, then the default-folder button below.
    constexpr float SongsControlHeight = ControlHeight + 6 + 32;
    // A rail click scrolls a section's top to the body's top; the spy uses the same margin.
    constexpr float SectionSpyMargin = 24;

    float FieldHeight(const float controlHeight)
    {
        return FieldGap + ControlOffset + controlHeight;
    }
} // namespace

void OptionsPanel::CreateRail()
{
    rail_ = &CreatePanel(*panel_, TopLeft({0, 0, RailWidth, PanelHeight}, PanelHeight), "Options.Rail");
    rail_->SetZIndex(30);
    SetPanelColor(*rail_, RailColor);
    BindPart(*rail_, "Rail");
    rail_->AddComponent<RectangleCollider2DComponent>();
    rail_->AddComponent<PointerReceiverComponent>();
    for (std::size_t i = 0; i < sections_.size(); ++i)
    {
        auto &section = sections_[i];
        const float top = RailButtonTop + RailButtonHeight * static_cast<float>(i);
        // Buttons keep the expanded width; the rail's clip shows only the icon column while collapsed.
        section.railButton = &CreateButton(*rail_, TopLeft({0, top, RailExpandedWidth, RailButtonHeight}, PanelHeight),
                                           L"", std::string("Options.Rail.") + SectionNames[i]);
        SetColors(*section.railButton, Transparent, RailHover, RailOn, Transparent);
        BindPart(*section.railButton, "RailButton");
        auto &marker = CreateSprite(*section.railButton, TopLeft({0, 0, 4, RailButtonHeight}, RailButtonHeight), {},
                                    "Marker");
        SetPanelColor(marker, ui_colors::theme::Sun);
        section.railIcon = &CreateIcon(*section.railButton, {20, (RailButtonHeight - RailIconSize) * 0.5F,
                                                             RailIconSize, RailIconSize},
                                       SectionIcons[i], RailIdleColor, "Icon");
        section.railLabel = &CreateCaption(*section.railButton, {62, 0, RailExpandedWidth - 74, RailButtonHeight}, 15,
                                           800, ui_colors::White, "Label");
    }
    ApplyRailWidth();
}

void OptionsPanel::LayoutSections()
{
    std::array<std::vector<float>, SectionCount> controls{
        std::vector<float>{ControlHeight, SongsControlHeight},
        std::vector<float>{ControlHeight, ControlHeight},
        std::vector<float>(volumes_.size() + 4, ControlHeight),
        std::vector<float>{}};
    for (const auto &layout : inputLayouts_)
    {
        const auto *presentation = FindModePresentation(layout.modeId);
        if (presentation && presentation->keyOverlay)
        {
            controls[3].push_back(KeyCardHeight);
            continue;
        }
        for (const auto &binding : layout.bindings)
            controls[3].insert(controls[3].end(), binding.keys.size(), ControlHeight);
    }
    float top = 0;
    for (std::size_t i = 0; i < sections_.size(); ++i)
    {
        float height = SectionPaddingTop + SectionHeaderHeight + SectionPaddingBottom;
        for (const float control : controls[i])
            height += FieldHeight(control);
        sections_[i].top = top;
        sections_[i].height = height;
        top += height;
    }
    contentHeight_ = top;
}

void OptionsPanel::CreateSections()
{
    LayoutSections();
    content_->SetSize({BodyWidth, contentHeight_});
    for (std::size_t i = 0; i < sections_.size(); ++i)
    {
        CreateSectionHeader(i);
        layoutCursor_ = sections_[i].top + SectionPaddingTop + SectionHeaderHeight;
        switch (i)
        {
        case 0:
            CreateLanguageControls();
            CreateSongsControls();
            break;
        case 1:
            CreateSkinControls();
            CreateDisplayControls();
            break;
        case 2:
            CreateVolumeControls();
            CreateAudioControls();
            CreateOffsetControls();
            break;
        default:
            CreateKeyControls();
            break;
        }
        const float end = layoutCursor_ + SectionPaddingBottom;
        if (std::abs(end - (sections_[i].top + sections_[i].height)) > 0.5F)
            throw std::logic_error("Options section fields must match the section layout.");
    }
    UpdateContentLayout();
}

void OptionsPanel::CreateSectionHeader(const std::size_t index)
{
    auto &section = sections_[index];
    const float top = section.top;
    if (index > 0)
    {
        section.rule = &CreateSprite(*content_, TopLeft({ContentLeft, top, FieldWidth, 1}, contentHeight_), {},
                                     std::string("Options.Section.") + SectionNames[index] + ".Rule");
        SetPanelColor(*section.rule, ui_colors::theme::Rule);
    }
    const float headerTop = top + SectionPaddingTop;
    section.headerIcon = &CreateIcon(*content_, {ContentLeft, headerTop + 2, 22, 22}, SectionIcons[index],
                                     ui_colors::theme::Brand,
                                     std::string("Options.Section.") + SectionNames[index] + ".Icon");
    section.headerLabel = &CreateCaption(*content_, {ContentLeft + 32, headerTop, FieldWidth - 32, SectionHeaderHeight},
                                         20, 900, HeadingColor,
                                         std::string("Options.Section.") + SectionNames[index]);
}

float OptionsPanel::NextField(const float controlHeight)
{
    const float top = layoutCursor_ + FieldGap;
    layoutCursor_ += FieldHeight(controlHeight);
    return top;
}

void OptionsPanel::RefreshRail()
{
    if (rail_ == nullptr)
        return;
    // The lit section's part changes only when the section does; hover colours follow every frame.
    const bool sectionChanged = railSkinSection_ != activeSection_;
    railSkinSection_ = activeSection_;
    for (std::size_t i = 0; i < sections_.size(); ++i)
    {
        auto &section = sections_[i];
        const bool active = i == activeSection_;
        if (sectionChanged)
        {
            SetSkinBaseStyle(*section.railButton, VisualStyle{active ? RailOn : Transparent,
                                                              active ? RailOn : RailHover, RailOn, Transparent});
            BindPart(*section.railButton, active ? "RailButton.On" : "RailButton");
            for (const auto &child : section.railButton->Children())
                if (child->Name() == "Marker")
                    child->SetVisible(active);
        }
        const bool lit = active || section.railButton->IsHovered();
        const auto colour = lit ? ui_colors::White : RailIdleColor;
        TintIcon(*section.railIcon, colour);
        RequireComponent<TextVisualComponent>(*section.railLabel).SetTextColor(colour);
    }
}

void OptionsPanel::UpdateRailExpansion(const double deltaSeconds)
{
    if (railPointerInside_ && VisibleKeyOverlay() == nullptr)
        railHoverSeconds_ += static_cast<float>(deltaSeconds);
    else
        railHoverSeconds_ = 0.0F;
    const bool hovered = railHoverSeconds_ >= RailExpandDelaySeconds;
    const bool expand = targetOpen_ && (railKeyboard_ || hovered);
    if (expand == railExpanded_)
        return;
    railExpanded_ = expand;
    ApplyRailWidth();
    hitTestDirty_ = true;
}

void OptionsPanel::ApplyRailWidth()
{
    // The expanded rail covers the body; section names appear only then.
    const float width = railExpanded_ ? RailExpandedWidth : RailWidth;
    rail_->SetBounds(TopLeft({0, 0, width, PanelHeight}, PanelHeight));
    rail_->SetClipRect({0, 0, width, PanelHeight});
    for (const auto &section : sections_)
        section.railLabel->SetVisible(railExpanded_);
}

void OptionsPanel::ScrollToSection(const std::size_t index)
{
    if (index >= sections_.size())
        return;
    CancelKeyboardEdit();
    CollapseCombos();
    heldSection_ = index;
    scrollOffset_ = sections_[index].top;
    dragging_ = false;
    previousDragPoint_.reset();
    UpdateContentLayout();
    hitTestDirty_ = true;
}

void OptionsPanel::StepSection(const int delta)
{
    const auto last = static_cast<int>(sections_.size()) - 1;
    const auto next = std::clamp(static_cast<int>(activeSection_) + delta, 0, last);
    railKeyboard_ = true;
    ScrollToSection(static_cast<std::size_t>(next));
}

void OptionsPanel::SpySection()
{
    if (heldSection_)
    {
        activeSection_ = *heldSection_;
        return;
    }
    // The section hanging at the top is lit; at the very bottom the last one is.
    const float maximum = std::max(contentHeight_ - viewportHeight_, 0.0F);
    std::size_t current = 0;
    if (maximum > 0.0F && scrollOffset_ >= maximum - 2.0F)
        current = sections_.size() - 1;
    else
        for (std::size_t i = 0; i < sections_.size(); ++i)
            if (sections_[i].top <= scrollOffset_ + SectionSpyMargin)
                current = i;
    activeSection_ = current;
}

void OptionsPanel::CollapseCombos()
{
    for (auto *node : {languageCombo_, skinCombo_, middlewareCombo_, outputCombo_, driverCombo_, resolutionCombo_})
        if (node)
            RequireDropdown(*node).Collapse();
}

void OptionsPanel::UpdateContentLayout()
{
    viewportHeight_ = PanelHeight - HeaderHeight - footerHeight_;
    viewport_->SetBounds({BodyLeft, footerHeight_, BodyWidth, viewportHeight_});
    viewport_->SetClipRect({0, 0, BodyWidth, viewportHeight_});
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, std::max(contentHeight_ - viewportHeight_, 0.0F));
    content_->SetPosition({0, viewportHeight_ - contentHeight_ + scrollOffset_});
    UpdateScrollbar();
    SpySection();
    RefreshRail();
}

void OptionsPanel::UpdateScrollbar()
{
    const float maximum = contentHeight_ - viewportHeight_;
    const bool scrollable = maximum > 0.0F;
    scrollRail_->SetVisible(scrollable);
    scrollThumb_->SetVisible(scrollable);
    if (!scrollable)
        return;
    const float left = PanelWidth - ScrollbarWidth;
    scrollRail_->SetBounds({left, footerHeight_, ScrollbarWidth, viewportHeight_});
    for (const auto &child : scrollRail_->Children())
        child->SetBounds({3, 0, 6, viewportHeight_});
    const float thumbHeight = std::max(32.0F, viewportHeight_ * viewportHeight_ / contentHeight_);
    const float thumbTop = HeaderHeight + (viewportHeight_ - thumbHeight) * (scrollOffset_ / maximum);
    scrollThumb_->SetBounds(TopLeft({left + 3, thumbTop, 6, thumbHeight}, PanelHeight));
}

bool OptionsPanel::CanProcessAction(NodeId source) const noexcept
{
    const auto *node = canvas_->FindNode(source);
    // A visible key overlay is modal: only its own descendants receive actions.
    const auto *overlay = VisibleKeyOverlay();
    bool admitted = overlay == nullptr;
    if (!node)
        return false;
    for (; node; node = node->Parent())
    {
        if (!node->IsVisible() || !node->IsEnabled())
            return false;
        admitted |= overlay && node == overlay->panel;
    }
    return admitted;
}
