#include "OptionsPanelStyle.h"
#include "Presentation/ModePresentation.h"
#include "Presentation/OptionsPanel.h"
#include "App/SkinSetSelection.h"
#include <algorithm>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    std::wstring KeyName(std::uint16_t key, const TextCatalog &catalog)
    {
        UINT scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC_EX);
        LONG flags = static_cast<LONG>((scan & 0xff) << 16);
        if ((scan & 0xff00) != 0)
            flags |= 1 << 24;
        wchar_t name[128]{};
        if (GetKeyNameTextW(flags, name, 128) > 0)
            return name;
        return std::wstring(catalog.Get("Options.virtualKeyPrefix")) + std::to_wstring(key);
    }
} // namespace
void OptionsPanel::CreateKeyControls()
{
    float top = 38;
    for (const auto &layout : inputLayouts_)
    {
        modeLabels_.push_back(
            &CreateFieldLabel(*content_, contentHeight_ - top, PanelWidth, "Options.Keys." + layout.modeId));
        top += 52;
        const auto *presentation = FindModePresentation(layout.modeId);
        if (presentation && presentation->keyOverlay)
        {
            CreateKeyOverlay(layout, *presentation->keyOverlay, top);
            top += presentation->keyOverlay->settingsEntryHeight;
            continue;
        }
        for (const auto &binding : layout.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
            {
                KeyRow row{layout.modeId, binding.id, binding.displayName, slot};
                const auto name = "Options.Key." + row.mode + "." + row.binding + "." + std::to_string(slot);
                row.label = &CreateFieldLabel(*content_, contentHeight_ - top, PanelWidth, name + ".Label");
                row.label->GetComponent<TextVisualComponent>()->SetFontSize(15);
                row.button = &CreateButton(
                    *content_, {24, contentHeight_ - top - 46, PanelWidth - 48, 38}, L"", name);
                VisualStyle style{};
                style.normal = FieldColor;
                style.hovered = FieldHoverColor;
                style.pressed = ui_colors::options::Pressed;
                style.disabled = ui_colors::options::Disabled;
                RequireComponent<SpriteVisualComponent>(*row.button).SetStyle(style);
                RequireComponent<TextVisualComponent>(*row.button).SetFontSize(18);
                RequireComponent<TextVisualComponent>(*row.button).SetTextColor(HeadingColor);
                keyRows_.push_back(std::move(row));
                top += 84;
            }
    }
}
void OptionsPanel::CreateKeyOverlay(const ModeInputLayout &layout, const ModeKeyOverlay &description,
                                    const float top)
{
    const std::string prefix(description.nodePrefix);
    const std::wstring title(texts_.Get(description.titleTextKey));
    KeyOverlay overlay{&description};
    // The settings list keeps one entry; the editor itself is a centered overlay.
    overlay.open = &CreateButton(*content_, {24, contentHeight_ - top - 46, PanelWidth - 48, 38}, title,
                                 prefix + ".OpenKeys");
    SetPanelColor(*overlay.open, FieldColor);
    RequireComponent<TextVisualComponent>(*overlay.open).SetTextColor(HeadingColor);
    // Root-level Z order keeps this centered overlay above the options anchor.
    overlay.panel = &CreatePanel(canvas_->Root(), {-450, -325, 900, 650}, prefix + ".Keys");
    overlay.panel->SetZIndex(200);
    SetPanelColor(*overlay.panel, ui_colors::result::Surface);
    overlay.title = &CreateLabel(*overlay.panel, {40, 580, 700, 40}, title, prefix + ".Title");
    auto &caption = RequireComponent<TextVisualComponent>(*overlay.title);
    texts_.ApplyFont(caption); caption.SetFontSize(30); caption.SetTextColor(HeadingColor);
    overlay.close = &CreateButton(*overlay.panel, {834, 588, 42, 42}, L"×", prefix + ".CloseKeys");
    SetPanelColor(*overlay.close, ui_colors::sky::Accent);
    overlay.status = &CreateLabel(*overlay.panel, {60, 20, 780, 65}, L"", prefix + ".Status");
    RequireComponent<TextVisualComponent>(*overlay.status).SetTextColor(ui_colors::Error);
    RequireComponent<TextVisualComponent>(*overlay.status).SetFontSize(15);

    // Reuse the gameplay input panel image and key faces at the same layout.
    constexpr Rect LayoutImageBounds{365, 395, 170, 170};
    overlay.faces.assign(description.faces.size(), nullptr);
    if (visuals_)
    {
        const auto path = SkinSetSelection::Instance().Image("InGame", std::string(description.layoutImage));
        if (!path.empty())
            overlay.layoutImage = &CreateSprite(*overlay.panel, LayoutImageBounds, visuals_->RegisterImage(path),
                                                prefix + ".KeyLayout");
        const float scale = LayoutImageBounds.width / description.layoutImageSourceSize;
        for (std::size_t i = 0; i < description.faces.size(); ++i)
        {
            const auto &face = description.faces[i];
            const auto keyPath = SkinSetSelection::Instance().Image("InGame", std::string(face.skinImage));
            if (keyPath.empty()) continue;
            const auto &bounds = face.sourceBounds;
            auto &key = CreateSprite(*overlay.panel, {LayoutImageBounds.x + bounds.x * scale,
                LayoutImageBounds.y + (description.layoutImageSourceSize - bounds.y - bounds.height) * scale,
                bounds.width * scale, bounds.height * scale}, visuals_->RegisterImage(keyPath), prefix + ".KeyImage");
            RequireComponent<SpriteVisualComponent>(key).SetTint(face.tint);
            overlay.faces[i] = &key;
        }
    }

    // One column per binding. Rows preserve their slot number, including empty
    // auxiliary bindings. Four columns use the original 200px spacing.
    const auto columns = layout.bindings.size();
    const float columnStep = columns <= 4 ? 200.0F : 780.0F / static_cast<float>(columns);
    const float columnWidth = std::min(170.0F, columnStep - 30.0F);
    for (std::size_t column = 0; column < columns; ++column)
    {
        const auto &binding = layout.bindings[column];
        const float x = 60 + columnStep * static_cast<float>(column);
        auto &heading = CreateLabel(*overlay.panel, {x, 350, columnWidth, 30},
                                    column < description.faces.size() ? std::wstring(description.faces[column].heading)
                                                                      : WideText(binding.displayName),
                                    prefix + ".Heading");
        auto &headingText = RequireComponent<TextVisualComponent>(heading);
        texts_.ApplyFont(headingText); headingText.SetFontSize(22); headingText.SetTextColor(HeadingColor);
        for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
        {
            KeyRow row{layout.modeId, binding.id, binding.displayName, slot};
            const float y = 285.0F - 61.0F * static_cast<float>(slot);
            const auto name = "Options.Key." + row.mode + "." + row.binding + "." + std::to_string(slot);
            row.button = &CreateButton(*overlay.panel, {x, y, slot ? columnWidth - 38 : columnWidth, 48}, L"", name);
            SetPanelColor(*row.button, slot ? FieldColor : ui_colors::sky::Accent);
            auto &buttonText = RequireComponent<TextVisualComponent>(*row.button);
            buttonText.SetFontSize(20); buttonText.SetTextColor(slot ? HeadingColor : ui_colors::White);
            if (slot)
            {
                row.clear = &CreateButton(*overlay.panel, {x + columnWidth - 34, y, 34, 48}, L"×", name + ".Clear");
                SetPanelColor(*row.clear, FieldColor);
                RequireComponent<TextVisualComponent>(*row.clear).SetTextColor(HeadingColor);
            }
            keyRows_.push_back(std::move(row));
        }
    }
    overlay.panel->SetVisible(false);
    keyOverlays_.push_back(std::move(overlay));
}
OptionsPanel::KeyOverlay *OptionsPanel::VisibleKeyOverlay() noexcept
{
    const auto found = std::ranges::find_if(keyOverlays_, [](const KeyOverlay &overlay) {
        return overlay.panel->IsVisible();
    });
    return found == keyOverlays_.end() ? nullptr : &*found;
}
const OptionsPanel::KeyOverlay *OptionsPanel::VisibleKeyOverlay() const noexcept
{
    return const_cast<OptionsPanel *>(this)->VisibleKeyOverlay();
}
void OptionsPanel::HideKeyOverlays() noexcept
{
    for (auto &overlay : keyOverlays_)
        overlay.panel->SetVisible(false);
}
void OptionsPanel::RefreshKeyImages()
{
    if (!visuals_) return;
    const auto refresh = [this](Visual2DNode *node, std::string_view name) {
        if (!node) return;
        const auto path = SkinSetSelection::Instance().Image("InGame", std::string(name));
        if (!path.empty()) RequireComponent<SpriteVisualComponent>(*node).SetImage(visuals_->RegisterImage(path));
    };
    for (auto &overlay : keyOverlays_)
    {
        refresh(overlay.layoutImage, overlay.description->layoutImage);
        for (std::size_t i = 0; i < overlay.faces.size(); ++i)
            refresh(overlay.faces[i], overlay.description->faces[i].skinImage);
    }
}
void OptionsPanel::RefreshKeyControls()
{
    const auto &text = Options(texts_);
    for (const auto &overlay : keyOverlays_)
        for (auto *node : {overlay.open, overlay.title})
        {
            auto &caption = RequireComponent<TextVisualComponent>(*node);
            caption.SetText(std::wstring(texts_.Get(overlay.description->titleTextKey)));
            texts_.ApplyFont(caption);
        }
    std::size_t rowIndex = 0;
    for (std::size_t i = 0; i < inputLayouts_.size(); ++i)
    {
        auto &heading = RequireComponent<TextVisualComponent>(*modeLabels_[i]);
        heading.SetText(WideText(inputLayouts_[i].modeId) + L" · " + std::wstring(text.keys));
        texts_.ApplyFont(heading);
        const auto saved = options_.Values().modeKeys.find(inputLayouts_[i].modeId);
        const auto resolved = ResolveInputLayout(inputLayouts_[i], saved == options_.Values().modeKeys.end()
                                                                             ? InputKeyOverrides{}
                                                                             : saved->second);
        for (const auto &binding : resolved.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot, ++rowIndex)
            {
                auto &row = keyRows_[rowIndex];
                auto name = InputBindingName(texts_, row.mode, row.binding);
                if (row.label)
                {
                    auto &label = RequireComponent<TextVisualComponent>(*row.label);
                    label.SetText((name.empty() ? WideText(row.displayName) : std::wstring(name)) + L" · " +
                                  std::wstring(slot == 0 ? text.primary : text.auxiliary));
                    texts_.ApplyFont(label);
                }
                auto &button = RequireComponent<TextVisualComponent>(*row.button);
                button.SetText(captureRow_ == rowIndex ? std::wstring(text.pressKey)
                                                       : binding.keys[slot] == 0 ? L"—" : KeyName(binding.keys[slot], texts_));
                texts_.ApplyFont(button);
            }
    }
}
void OptionsPanel::CaptureKey(std::uint16_t key)
{
    const auto &row = keyRows_.at(*captureRow_);
    auto candidate = options_.Values();
    const auto layout = std::ranges::find(inputLayouts_, row.mode, &ModeInputLayout::modeId);
    const auto saved = candidate.modeKeys.find(row.mode);
    auto resolved = ResolveInputLayout(*layout, saved == candidate.modeKeys.end() ? InputKeyOverrides{}
                                                                                        : saved->second);
    const auto binding = std::ranges::find(resolved.bindings, row.binding, &ModeInputBinding::id);
    binding->keys[row.slot] = key;
    candidate.modeKeys[row.mode][row.binding] = binding->keys;
    // OptionSettings validates conflicts against all other default/changed keys.
    if (SaveOptions(candidate))
        captureRow_.reset();
    RefreshKeyControls();
    RefreshStatus();
}
bool OptionsPanel::ProcessFieldAction(const Action &action)
{
    if (action.type == ActionType::SelectionChanged && action.source == resolutionCombo_->Id())
    {
        CancelKeyboardEdit();
        SelectResolution(action.selectedIndex);
        return true;
    }
    if (action.type != ActionType::Clicked)
        return false;
    if (auto *overlay = VisibleKeyOverlay())
    {
        if (action.source == overlay->close->Id())
        {
            CancelKeyboardEdit();
            overlay->panel->SetVisible(false);
            return true;
        }
        // CanProcessAction admits only nodes inside the visible overlay.
        for (std::size_t i = 0; i < keyRows_.size(); ++i)
        {
            if (keyRows_[i].clear && action.source == keyRows_[i].clear->Id())
            {
                captureRow_ = i;
                CaptureKey(0);
                return true;
            }
            if (action.source == keyRows_[i].button->Id())
            {
                CancelKeyboardEdit(); captureRow_ = i; RefreshKeyControls(); return true;
            }
        }
        return true;
    }
    for (auto &overlay : keyOverlays_)
        if (action.source == overlay.open->Id())
        {
            CancelKeyboardEdit(); RefreshKeyImages(); overlay.panel->SetVisible(true); return true;
        }
    if (action.source == songsField_->Id())
    {
        CancelKeyboardEdit();
        ChooseSongsFolder();
        return true;
    }
    if (action.source == songsDefault_->Id())
    {
        auto candidate = options_.Values();
        candidate.songsPath.clear();
        SaveOptions(candidate);
        RefreshSongsControls();
        RefreshStatus();
        return true;
    }
    for (std::size_t i = 0; i < keyRows_.size(); ++i)
        if (action.source == keyRows_[i].button->Id())
        {
            CancelKeyboardEdit();
            captureRow_ = i;
            RefreshKeyControls();
            return true;
        }
    if (action.source == offsetField_->Id())
    {
        CancelKeyboardEdit();
        offsetDraft_ = std::to_wstring(options_.Values().globalOffsetMs);
        editingOffset_ = replaceOffsetDraft_ = true;
        RefreshOffsetControls();
        return true;
    }
    return false;
}
