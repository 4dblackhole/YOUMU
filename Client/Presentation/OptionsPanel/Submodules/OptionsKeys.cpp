#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include "App/SkinSetSelection.h"
#include <algorithm>

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
    float top = 1148;
    for (const auto &layout : inputLayouts_)
    {
        modeLabels_.push_back(
            &CreateFieldLabel(*content_, contentHeight_ - top, PanelWidth, "Options.Keys." + layout.modeId));
        top += 52;
        if (layout.modeId == "Taiko")
        {
            // The settings list keeps one entry; the editor itself is a centered overlay.
            openKeyPanel_ = &CreateButton(*content_, {24, contentHeight_ - top - 46, PanelWidth - 48, 38},
                                          std::wstring(texts_.Get("Options.taikoKeySettings")), "Options.Taiko.OpenKeys");
            SetPanelColor(*openKeyPanel_, FieldColor);
            auto &anchor = canvas_->AnchorNode(Anchor::Center);
            keyPanel_ = &CreatePanel(anchor, {-450, -325, 900, 650}, "Options.Taiko.Keys");
            keyPanel_->SetZIndex(200);
            SetPanelColor(*keyPanel_, ui_colors::result::Surface);
            auto &title = CreateLabel(*keyPanel_, {40, 580, 700, 40},
                                      std::wstring(texts_.Get("Options.taikoKeySettings")), "Options.Taiko.Title");
            keyPanelTitle_ = &title;
            auto &caption = RequireComponent<TextVisualComponent>(title);
            texts_.ApplyFont(caption); caption.SetFontSize(30); caption.SetTextColor(HeadingColor);
            closeKeyPanel_ = &CreateButton(*keyPanel_, {834, 588, 42, 42}, L"×", "Options.Taiko.CloseKeys");
            SetPanelColor(*closeKeyPanel_, ui_colors::sky::Accent);
            keyPanelStatus_ = &CreateLabel(*keyPanel_, {60, 20, 780, 65}, L"", "Options.Taiko.Status");
            RequireComponent<TextVisualComponent>(*keyPanelStatus_).SetTextColor(ui_colors::Error);
            RequireComponent<TextVisualComponent>(*keyPanelStatus_).SetFontSize(15);
            // Reuse the same skin images and tint colors as the gameplay input panel.
            if (visuals_)
            {
                const auto path = SkinSetSelection::Instance().Image("InGame", "InputPanel");
                if (!path.empty())
                    keyPanelImage_ = &CreateSprite(*keyPanel_, {365, 395, 170, 170}, visuals_->RegisterImage(path),
                                                   "Options.Taiko.KeyLayout");
                constexpr std::array names{"KeyButtonLeftKat", "KeyButtonLeftDon", "KeyButtonRightDon", "KeyButtonRightKat"};
                constexpr std::array<Color, 4> colors{{{.04F,.57F,.81F,1}, {.92F,.07F,.11F,1},
                                                       {.92F,.07F,.11F,1}, {.04F,.57F,.81F,1}}};
                constexpr std::array<Rect, 4> gameplayBounds{{
                    {16, 68, 39, 70}, {68, 92, 39, 70}, {120, 92, 39, 70}, {172, 68, 39, 70}}};
                constexpr float sourceSize = 228.0F;
                constexpr float scale = 170.0F / sourceSize;
                for (std::size_t i = 0; i < names.size(); ++i)
                {
                    const auto keyPath = SkinSetSelection::Instance().Image("InGame", names[i]);
                    if (keyPath.empty()) continue;
                    const auto &bounds = gameplayBounds[i];
                    auto &key = CreateSprite(*keyPanel_, {365.0F + bounds.x * scale,
                        395.0F + (sourceSize - bounds.y - bounds.height) * scale,
                        bounds.width * scale, bounds.height * scale}, visuals_->RegisterImage(keyPath),
                        "Options.Taiko.KeyImage");
                    RequireComponent<SpriteVisualComponent>(key).SetTint(colors[i]);
                    keyFaceImages_[i] = &key;
                }
            }
            // Rows preserve their slot number, including empty auxiliary bindings.
            constexpr std::array headings{L"Left K", L"Left D", L"Right D", L"Right K"};
            for (std::size_t column = 0; column < layout.bindings.size(); ++column)
            {
                const auto &binding = layout.bindings[column];
                const float x = 60 + 200.0F * static_cast<float>(column);
                auto &heading = CreateLabel(*keyPanel_, {x, 350, 170, 30},
                                            column < headings.size() ? headings[column] : WideText(binding.displayName),
                                            "Options.Taiko.Heading");
                auto &headingText = RequireComponent<TextVisualComponent>(heading);
                texts_.ApplyFont(headingText); headingText.SetFontSize(22); headingText.SetTextColor(HeadingColor);
                for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
                {
                    KeyRow row{layout.modeId, binding.id, binding.displayName, slot};
                    const float y = 285.0F - 61.0F * static_cast<float>(slot);
                    const auto name = "Options.Key." + row.mode + "." + row.binding + "." + std::to_string(slot);
                    row.button = &CreateButton(*keyPanel_, {x, y, slot ? 132.0F : 170.0F, 48}, L"", name);
                    SetPanelColor(*row.button, slot ? FieldColor : ui_colors::sky::Accent);
                    auto &buttonText = RequireComponent<TextVisualComponent>(*row.button);
                    buttonText.SetFontSize(20); buttonText.SetTextColor(slot ? HeadingColor : ui_colors::White);
                    if (slot)
                    {
                        row.clear = &CreateButton(*keyPanel_, {x + 136, y, 34, 48}, L"×", name + ".Clear");
                        SetPanelColor(*row.clear, FieldColor);
                        RequireComponent<TextVisualComponent>(*row.clear).SetTextColor(HeadingColor);
                    }
                    keyRows_.push_back(std::move(row));
                }
            }
            keyPanel_->SetVisible(false);
            top += 84;
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
void OptionsPanel::RefreshKeyImages()
{
    if (!visuals_) return;
    const auto refresh = [this](Visual2DNode *node, const char *name) {
        if (!node) return;
        const auto path = SkinSetSelection::Instance().Image("InGame", name);
        if (!path.empty()) RequireComponent<SpriteVisualComponent>(*node).SetImage(visuals_->RegisterImage(path));
    };
    refresh(keyPanelImage_, "InputPanel");
    constexpr std::array names{"KeyButtonLeftKat", "KeyButtonLeftDon", "KeyButtonRightDon", "KeyButtonRightKat"};
    for (std::size_t i = 0; i < names.size(); ++i) refresh(keyFaceImages_[i], names[i]);
}
void OptionsPanel::RefreshKeyControls()
{
    const auto &text = Options(texts_);
    for (auto *node : {openKeyPanel_, keyPanelTitle_})
        if (node)
        {
            auto &caption = RequireComponent<TextVisualComponent>(*node);
            caption.SetText(std::wstring(texts_.Get("Options.taikoKeySettings")));
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
    if (keyPanel_ && keyPanel_->IsVisible())
    {
        if (action.source == closeKeyPanel_->Id())
        {
            CancelKeyboardEdit();
            keyPanel_->SetVisible(false);
            return true;
        }
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
    if (openKeyPanel_ && action.source == openKeyPanel_->Id())
    {
        CancelKeyboardEdit(); RefreshKeyImages(); keyPanel_->SetVisible(true); return true;
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
