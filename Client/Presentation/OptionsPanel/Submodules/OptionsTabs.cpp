#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>

using namespace options_ui;
namespace
{
    constexpr std::array Names{"General", "Display", "Audio", "Input"};
}
void OptionsPanel::CreateTabs()
{
    contentHeights_.fill(ContentViewportHeight);
    float inputHeight = 38 + 24;
    for (const auto &layout : inputLayouts_)
    {
        inputHeight += 52;
        if (layout.modeId == "Taiko") inputHeight += 84;
        else for (const auto &binding : layout.bindings)
            inputHeight += 84 * static_cast<float>(binding.keys.size());
    }
    contentHeights_[3] = std::max(inputHeight, ContentViewportHeight);
    for (std::size_t i = 0; i < Names.size(); ++i)
    {
        tabButtons_[i] = &CreateButton(*panel_, {16 + 72 * static_cast<float>(i), 606, 72, 32}, L"",
                                      std::string("Options.Tab.") + Names[i]);
        tabButtons_[i]->GetComponent<TextVisualComponent>()->SetFontSize(14);
        tabContents_[i] = &viewport_->CreateChild(std::string("Options.Content.") + Names[i]);
        tabContents_[i]->SetSize({PanelWidth, contentHeights_[i]});
        content_ = tabContents_[i];
        contentHeight_ = contentHeights_[i];
        switch (i)
        {
        case 0: CreateLanguageControls(); CreateSongsControls(); break;
        case 1: CreateSkinControls(); CreateDisplayControls(); break;
        case 2: CreateAudioControls(); CreateOffsetControls(); break;
        case 3: CreateKeyControls(); break;
        }
    }
    SelectTab(0);
}
void OptionsPanel::CollapseCombos()
{
    for (auto *node : {languageCombo_, skinCombo_, middlewareCombo_, outputCombo_, driverCombo_, resolutionCombo_})
        if (node) RequireComponent<ComboBoxBehaviorComponent>(*node).Collapse();
}
void OptionsPanel::SelectTab(std::size_t index)
{
    if (index >= TabCount || (activeTab_ == index && content_ == tabContents_[index])) return;
    CollapseCombos();
    CancelKeyboardEdit();
    if (keyPanel_) keyPanel_->SetVisible(false);
    scrollOffsets_.at(activeTab_) = scrollOffset_;
    activeTab_ = index;
    content_ = tabContents_[index];
    contentHeight_ = contentHeights_[index];
    scrollOffset_ = scrollOffsets_[index];
    dragging_ = false;
    previousDragPoint_.reset();
    for (std::size_t i = 0; i < TabCount; ++i) tabContents_[i]->SetVisible(i == index);
    UpdateContentLayout();
    RefreshTabs();
}
void OptionsPanel::RefreshTabs()
{
    for (std::size_t i = 0; i < TabCount; ++i)
    {
        auto &label = RequireComponent<TextVisualComponent>(*tabButtons_[i]);
        label.SetText(std::wstring(texts_.Get(std::string("Options.tabs.") + Names[i])));
        texts_.ApplyFont(label);
        label.SetTextColor(i == activeTab_ ? ui_colors::White : HeadingColor);
        VisualStyle style{};
        style.normal = i == activeTab_ ? ui_colors::sky::Accent : FieldColor;
        style.hovered = i == activeTab_ ? ui_colors::sky::AccentHover : FieldHoverColor;
        style.pressed = ui_colors::options::Pressed;
        style.disabled = ui_colors::options::Disabled;
        RequireComponent<SpriteVisualComponent>(*tabButtons_[i]).SetStyle(style);
    }
}
void OptionsPanel::UpdateContentLayout()
{
    const float footer = statusLabel_->IsVisible() ? 156.0F : 0.0F;
    viewportHeight_ = ContentViewportHeight - footer;
    viewport_->SetBounds({0, 24 + footer, PanelWidth, viewportHeight_});
    viewport_->SetClipRect({0, 0, PanelWidth, viewportHeight_});
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, std::max(contentHeight_ - viewportHeight_, 0.0F));
    scrollOffsets_.at(activeTab_) = scrollOffset_;
    content_->SetPosition({0, viewportHeight_ - contentHeight_ + scrollOffset_});
}
bool OptionsPanel::CanProcessAction(NodeId source) const noexcept
{
    const auto *node = canvas_->FindNode(source);
    bool modal = !(keyPanel_ && keyPanel_->IsVisible());
    if (!node) return false;
    for (; node; node = node->Parent())
    {
        if (!node->IsVisible() || !node->IsEnabled()) return false;
        modal |= node == keyPanel_;
    }
    return modal;
}
