#include "OptionsPanelStyle.h"
#include "Presentation/ModePresentation.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/SkinVisualComponent.h"
#include "App/SkinSetSelection.h"
#include <algorithm>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    // Key settings modal (UI-02): 432 wide, four binding columns of one main and three auxiliary slots.
    constexpr float ModalWidth = 432, ModalHeight = 314, ModalPadding = 24;
    constexpr float GridTop = 78, ColumnGap = 10, HeadHeight = 34, FaceHeight = 24;
    constexpr float MainTop = GridTop + HeadHeight + 6, MainHeight = 48, SubTop = MainTop + MainHeight + 10;
    constexpr float SubHeight = 34, SubPitch = 40, ClearSize = 22;
    constexpr float StatusHeight = 46, StatusTextWidth = 346;
    constexpr Color Transparent{1, 1, 1, 0};
    constexpr auto SlotColor = ui_colors::Rgb(0xFFFFFF, .8F), EmptySlotColor = ui_colors::Rgb(0xFFFFFF, .45F);

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
    float ColumnWidth(const std::size_t columns)
    {
        const auto count = static_cast<float>(std::max<std::size_t>(columns, 1));
        return (ModalWidth - 2 * ModalPadding - ColumnGap * (count - 1)) / count;
    }
    Rect SlotBounds(const std::size_t column, const std::size_t slot, const float width)
    {
        const float x = ModalPadding + (width + ColumnGap) * static_cast<float>(column);
        if (slot == 0)
            return {x, MainTop, width, MainHeight};
        return {x, SubTop + SubPitch * static_cast<float>(slot - 1), width, SubHeight};
    }
    InputKeyOverrides SavedOverrides(const OptionValues &values, const std::string &mode)
    {
        const auto saved = values.modeKeys.find(mode);
        return saved == values.modeKeys.end() ? InputKeyOverrides{} : saved->second;
    }
    // A newly assigned key leaves every other slot of the same mode that used it unassigned,
    // whichever binding or slot (primary included) that was. The changed bindings are saved too.
    void UnassignKeyElsewhere(ModeInputLayout &layout, std::map<std::string, std::vector<std::uint16_t>> &saved,
                              const std::uint16_t key, const std::string_view keptBinding, const std::size_t keptSlot)
    {
        if (key == 0)
            return;
        for (auto &binding : layout.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
            {
                const bool kept = binding.id == keptBinding && slot == keptSlot;
                if (kept || binding.keys[slot] != key)
                    continue;
                binding.keys[slot] = 0;
                saved[binding.id] = binding.keys;
            }
    }
} // namespace

void OptionsPanel::CreateKeyControls()
{
    CreateRestartKeyControl();
    for (const auto &layout : inputLayouts_)
    {
        const auto *presentation = FindModePresentation(layout.modeId);
        if (presentation && presentation->keyOverlay)
        {
            CreateKeyOverlay(layout, *presentation->keyOverlay);
            continue;
        }
        // A mode without an overlay edits each binding slot on its own row.
        modeLabels_.push_back(nullptr);
        for (const auto &binding : layout.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
            {
                KeyRow row{layout.modeId, binding.id, binding.displayName, slot};
                const auto name = "Options.Key." + row.mode + "." + row.binding + "." + std::to_string(slot);
                const float top = NextField(ControlHeight);
                row.label = &CreateFieldLabel(*content_, top, name + ".Label");
                row.button = &CreateButton(
                    *content_, TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), L"",
                    name);
                StyleFieldButton(*row.button, true);
                AddFieldLine(*content_, *row.button, name + ".Line");
                keyRows_.push_back(std::move(row));
            }
    }
}

void OptionsPanel::CreateKeyOverlay(const ModeInputLayout &layout, const ModeKeyOverlay &description)
{
    const std::string prefix(description.nodePrefix);
    KeyOverlay overlay{&description, layout.modeId};

    // Input section card: the mode name above, the key summary inside.
    const float top = NextField(KeyCardHeight);
    modeLabels_.push_back(&CreateFieldLabel(*content_, top, "Options.Keys." + layout.modeId));
    overlay.open = &CreateButton(*content_,
                                 TopLeft({ContentLeft, top + ControlOffset, FieldWidth, KeyCardHeight}, contentHeight_),
                                 L"", prefix + ".OpenKeys");
    SetColors(*overlay.open, ui_colors::Rgb(0xFFFFFF, .7F), FieldHoverColor, FieldPressedColor, FieldDisabledColor);
    BindPart(*overlay.open, "KeyCard");
    auto &marker = CreateSprite(*overlay.open, TopLeft({0, 0, 4, KeyCardHeight}, KeyCardHeight), {}, "Marker");
    SetPanelColor(marker, ui_colors::theme::Brand);
    CreateIcon(*overlay.open, {16, 18, 28, 28}, "Icons.drum", ui_colors::theme::BrandText, "Icon");
    overlay.openTitle = &CreateCaption(*overlay.open, {56, 14, 206, 20}, 15, 700, HeadingColor, "Title");
    overlay.openSummary = &PhantomCore::CreateLabel(*overlay.open, TopLeft({56, 34, 206, 16}, KeyCardHeight), L"",
                                                    "Summary");
    StyleNumber(RequireComponent<TextVisualComponent>(*overlay.openSummary), 12, 500, ui_colors::theme::Caption,
                TextAlignment::Leading, false);
    CreateIcon(*overlay.open, {274, 23, 18, 18}, "Icons.chev-right", ui_colors::theme::BrandText, "Chevron");

    // Modal: a scrim over the whole canvas and the centred panel above it.
    auto &root = canvas_->Root();
    overlay.scrim = &CreatePanel(root, {}, prefix + ".KeysScrim");
    overlay.scrim->SetZIndex(199);
    SetPanelColor(*overlay.scrim, ScrimColor);
    overlay.scrim->AddComponent<RectangleCollider2DComponent>();
    overlay.scrim->AddComponent<PointerReceiverComponent>();
    overlay.panel = &CreatePanel(root, {-ModalWidth * 0.5F, -ModalHeight * 0.5F, ModalWidth, ModalHeight}, prefix + ".Keys");
    overlay.panel->SetZIndex(200);
    SetPanelColor(*overlay.panel, ModalColor);
    overlay.title = &CreateCaption(*overlay.panel, {ModalPadding, 28, 300, 32}, 26, 900, HeadingColor, prefix + ".Title");
    overlay.close = &CreateButton(*overlay.panel, TopLeft({ModalWidth - ModalPadding - 32, 28, 32, 32}, ModalHeight), L"",
                                  prefix + ".CloseKeys");
    SetColors(*overlay.close, Transparent, ui_colors::Rgb(0x1F4C99, .08F), ui_colors::Rgb(0x1F4C99, .16F), Transparent);
    CreateIcon(*overlay.close, {6, 6, 20, 20}, "Icons.x", ui_colors::theme::BrandText, "Icon");

    // Waiting and error notices sit on a strip that extends the panel downward.
    overlay.status = &CreatePanel(*overlay.panel, {0, -StatusHeight, ModalWidth, StatusHeight}, prefix + ".Status");
    SetPanelColor(*overlay.status, ModalColor);
    auto &statusBar = CreateSprite(*overlay.status, TopLeft({ModalPadding, 0, 3, 30}, StatusHeight), {}, "Bar");
    SetPanelColor(statusBar, ui_colors::theme::Brand);
    overlay.statusIcon = &CreateIcon(*overlay.status, {ModalPadding + 12, 6, 18, 18}, "Icons.info",
                                     ui_colors::theme::Brand, "Icon");
    auto &statusText = PhantomCore::CreateLabel(*overlay.status, TopLeft({ModalPadding + 38, 0, 346, 30}, StatusHeight),
                                                L"", "Text");
    StyleCaption(RequireComponent<TextVisualComponent>(statusText), 13, 600, ui_colors::theme::Brand);
    overlay.status->SetVisible(false);

    // Column heads reuse the gameplay input panel's key faces, scaled to one height.
    const auto columns = layout.bindings.size();
    const float width = ColumnWidth(columns);
    float faceTop = 0, faceHeight = 0;
    for (std::size_t i = 0; i < description.faces.size(); ++i)
    {
        const auto &bounds = description.faces[i].sourceBounds;
        faceTop = i == 0 ? bounds.y : std::min(faceTop, bounds.y);
        faceHeight = std::max(faceHeight, bounds.height);
    }
    const float faceScale = faceHeight > 0 ? FaceHeight / faceHeight : 0.0F;
    overlay.faces.assign(description.faces.size(), nullptr);
    for (std::size_t i = 0; i < description.faces.size() && i < columns; ++i)
    {
        const auto &face = description.faces[i];
        const float faceWidth = face.sourceBounds.width * faceScale;
        const float left = SlotBounds(i, 0, width).x + (width - faceWidth) * 0.5F;
        auto &key = CreateSprite(*overlay.panel,
                                 TopLeft({left, GridTop + (face.sourceBounds.y - faceTop) * faceScale, faceWidth,
                                          face.sourceBounds.height * faceScale},
                                         ModalHeight),
                                 {}, prefix + ".KeyImage");
        RequireComponent<SpriteVisualComponent>(key).SetTint(face.tint);
        overlay.faces[i] = &key;
    }

    // Slots keep their slot number, including empty auxiliary keys.
    overlay.firstRow = keyRows_.size();
    for (std::size_t column = 0; column < columns; ++column)
    {
        const auto &binding = layout.bindings[column];
        for (std::size_t slot = 0; slot < binding.keys.size(); ++slot)
        {
            KeyRow row{layout.modeId, binding.id, binding.displayName, slot};
            const auto name = "Options.Key." + row.mode + "." + row.binding + "." + std::to_string(slot);
            const auto bounds = SlotBounds(column, slot, width);
            row.button = &CreateButton(*overlay.panel, TopLeft(bounds, ModalHeight), L"", name);
            SetPanelColor(*row.button, slot == 0 ? SlotColor : EmptySlotColor);
            if (slot > 0)
            {
                // The clear button shows only while its slot is pointed at.
                row.clear = &CreateButton(*overlay.panel,
                                          TopLeft({bounds.x + bounds.width - ClearSize - 2,
                                                   bounds.y + (bounds.height - ClearSize) * 0.5F, ClearSize, ClearSize},
                                                  ModalHeight),
                                          L"", name + ".Clear");
                row.clear->SetZIndex(2);
                SetColors(*row.clear, Transparent, ui_colors::Rgb(0x1F4C99, .08F), ui_colors::Rgb(0x1F4C99, .16F),
                          Transparent);
                CreateIcon(*row.clear, {4, 4, 14, 14}, "Icons.x", ui_colors::theme::Caption, "Icon");
                row.clear->SetVisible(false);
            }
            keyRows_.push_back(std::move(row));
        }
    }
    overlay.rowCount = keyRows_.size() - overlay.firstRow;
    overlay.scrim->SetVisible(false);
    overlay.panel->SetVisible(false);
    keyOverlays_.push_back(std::move(overlay));
    RefreshKeyImages();
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
    {
        overlay.panel->SetVisible(false);
        overlay.scrim->SetVisible(false);
    }
}

void OptionsPanel::ShowKeyOverlay(KeyOverlay &overlay)
{
    CancelKeyboardEdit();
    CollapseCombos();
    RefreshKeyImages();
    RefreshIcons();
    // The scrim follows the current logical size, which grows on wide windows.
    const auto size = canvas_->LogicalSize();
    overlay.scrim->SetBounds({-size.width * 0.5F, -size.height * 0.5F, size.width, size.height});
    overlay.scrim->SetVisible(true);
    overlay.panel->SetVisible(true);
    RefreshKeyOverlay(overlay);
    hitTestDirty_ = true;
}

void OptionsPanel::RefreshKeyImages()
{
    if (!visuals_)
        return;
    for (auto &overlay : keyOverlays_)
        for (std::size_t i = 0; i < overlay.faces.size(); ++i)
        {
            if (!overlay.faces[i])
                continue;
            const auto path = SkinSetSelection::Instance().Image("InGame", std::string(overlay.description->faces[i].skinImage));
            auto &sprite = RequireComponent<SpriteVisualComponent>(*overlay.faces[i]);
            overlay.faces[i]->SetVisible(!path.empty());
            if (!path.empty())
                sprite.SetImage(visuals_->RegisterImage(path));
        }
}

void OptionsPanel::CreateRestartKeyControl()
{
    const float top = NextField(ControlHeight);
    restartKeyLabel_ = &CreateFieldLabel(*content_, top, "Options.RestartKey.Label");
    restartKeyButton_ = &CreateButton(
        *content_, TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_), L"",
        "Options.RestartKey");
    StyleFieldButton(*restartKeyButton_, true);
    AddFieldLine(*content_, *restartKeyButton_, "Options.RestartKey.Line");
}

void OptionsPanel::RefreshRestartKeyControl()
{
    if (!restartKeyButton_)
        return;
    SetCaption(*restartKeyLabel_, std::wstring(Options(texts_).restartKey));
    RequireComponent<TextVisualComponent>(*restartKeyButton_)
        .SetText(capturingRestartKey_ ? std::wstring(L"···") : KeyName(options_.Values().restartKey, texts_));
}

void OptionsPanel::CaptureRestartKey(const std::uint16_t key)
{
    auto candidate = options_.Values();
    candidate.restartKey = key;
    // OptionSettings refuses a play key of any mode and the fixed game controls; the wait then continues.
    if (SaveOptions(candidate))
        capturingRestartKey_ = false;
    RefreshRestartKeyControl();
    RefreshStatus();
}

void OptionsPanel::RefreshKeyControls()
{
    RefreshRestartKeyControl();
    const auto &text = Options(texts_);
    std::size_t rowIndex = 0;
    for (std::size_t i = 0; i < inputLayouts_.size(); ++i)
    {
        if (auto *label = modeLabels_[i])
            SetCaption(*label, WideText(inputLayouts_[i].modeId));
        const auto resolved = ResolveInputLayout(inputLayouts_[i], SavedOverrides(options_.Values(), inputLayouts_[i].modeId));
        for (const auto &binding : resolved.bindings)
            for (std::size_t slot = 0; slot < binding.keys.size(); ++slot, ++rowIndex)
            {
                auto &row = keyRows_[rowIndex];
                if (row.label == nullptr)
                    continue;
                const auto name = InputBindingName(texts_, row.mode, row.binding);
                SetCaption(*row.label, (name.empty() ? WideText(row.displayName) : std::wstring(name)) + L" · " +
                                           std::wstring(slot == 0 ? text.primary : text.auxiliary));
                RequireComponent<TextVisualComponent>(*row.button)
                    .SetText(captureRow_ == rowIndex ? std::wstring(L"···")
                             : binding.keys[slot] == 0 ? std::wstring(L"—")
                                                       : KeyName(binding.keys[slot], texts_));
            }
    }
    for (auto &overlay : keyOverlays_)
        RefreshKeyOverlay(overlay);
}

void OptionsPanel::RefreshKeyOverlay(KeyOverlay &overlay)
{
    const auto &text = Options(texts_);
    const std::wstring title(texts_.Get(overlay.description->titleTextKey));
    SetCaption(*overlay.title, title);
    SetCaption(*overlay.openTitle, title);
    const auto layout = std::ranges::find(inputLayouts_, overlay.mode, &ModeInputLayout::modeId);
    const auto resolved = ResolveInputLayout(*layout, SavedOverrides(options_.Values(), overlay.mode));

    // Card summary: main keys, then the first auxiliary keys.
    std::wstring mains, subs;
    for (const auto &binding : resolved.bindings)
    {
        const auto separator = mains.empty() ? L"" : L" · ";
        mains += separator + (binding.keys.empty() || binding.keys[0] == 0 ? L"—" : KeyName(binding.keys[0], texts_));
        const bool hasSub = binding.keys.size() > 1 && binding.keys[1] != 0;
        subs += separator + (hasSub ? KeyName(binding.keys[1], texts_) : std::wstring(L"—"));
    }
    RequireComponent<TextVisualComponent>(*overlay.openSummary).SetText(mains + L"  /  " + subs);

    std::size_t rowIndex = overlay.firstRow;
    for (const auto &binding : resolved.bindings)
        for (std::size_t slot = 0; slot < binding.keys.size(); ++slot, ++rowIndex)
        {
            auto &row = keyRows_[rowIndex];
            const bool waiting = captureRow_ == rowIndex;
            const bool empty = binding.keys[slot] == 0;
            row.assigned = !empty;
            const char *part = waiting ? "Keys.Slot.Waiting" : slot == 0 ? "Keys.Slot.Main"
                                                           : empty       ? "Keys.Slot.Empty"
                                                                         : "Keys.Slot.Sub";
            const auto fill = waiting ? FieldHoverColor : empty ? EmptySlotColor : SlotColor;
            SetSkinBaseStyle(*row.button, VisualStyle{fill, FieldHoverColor, FieldPressedColor, FieldDisabledColor});
            BindPart(*row.button, part);
            auto &label = RequireComponent<TextVisualComponent>(*row.button);
            if (waiting)
            {
                label.SetText(L"···");
                StyleCaption(label, 17, 800, ui_colors::theme::Brand, TextAlignment::Center);
            }
            else if (empty)
            {
                label.SetText(L"+");
                StyleCaption(label, 18, 600, ui_colors::Rgb(0x4B6184, .6F), TextAlignment::Center);
            }
            else
            {
                label.SetText(KeyName(binding.keys[slot], texts_));
                StyleNumber(label, slot == 0 ? 24.0F : 17.0F, 800, HeadingColor, TextAlignment::Center);
            }
        }

    // The notice appears only while waiting for a key or after a rejected one.
    bool error = false;
    std::wstring notice;
    if (captureRow_ && *captureRow_ >= overlay.firstRow && *captureRow_ < overlay.firstRow + overlay.rowCount)
        notice = text.pressKey;
    if (status_ != Status::None)
        notice = StatusText(error);
    overlay.status->SetVisible(!notice.empty());
    // The strip grows downward with the wrapped notice.
    notice = WrapText(notice, 13, StatusTextWidth);
    const float textHeight = 12 + 18 * static_cast<float>(1 + std::ranges::count(notice, L'\n'));
    const float stripHeight = std::max(StatusHeight, textHeight + 16);
    overlay.status->SetBounds({0, -stripHeight, ModalWidth, stripHeight});
    const auto colour = error ? ui_colors::theme::Danger : ui_colors::theme::Brand;
    for (const auto &child : overlay.status->Children())
    {
        if (child->Name() == "Text")
        {
            child->SetBounds(TopLeft({ModalPadding + 38, 0, StatusTextWidth, textHeight}, stripHeight));
            SetCaption(*child, notice);
            RequireComponent<TextVisualComponent>(*child).SetTextColor(colour);
        }
        else if (child->Name() == "Bar")
        {
            child->SetBounds(TopLeft({ModalPadding, 0, 3, textHeight}, stripHeight));
            SetPanelColor(*child, colour);
        }
        else if (child.get() == overlay.statusIcon)
            child->SetBounds(TopLeft({ModalPadding + 12, (std::min(textHeight, 30.0F) - 18) * 0.5F, 18, 18}, stripHeight));
    }
    SetIconKey(*overlay.statusIcon, error ? "Icons.alert" : "Icons.info");
    TintIcon(*overlay.statusIcon, colour);
}

void OptionsPanel::UpdateKeyClearButtons(const NodeId hoveredNode)
{
    const auto *overlay = VisibleKeyOverlay();
    if (overlay == nullptr)
        return;
    for (std::size_t rowIndex = overlay->firstRow; rowIndex < overlay->firstRow + overlay->rowCount; ++rowIndex)
    {
        auto &row = keyRows_[rowIndex];
        if (row.clear == nullptr)
            continue;
        const bool pointed = hoveredNode == row.button->Id() || hoveredNode == row.clear->Id();
        const bool clearable = row.assigned && captureRow_ != rowIndex;
        row.clear->SetVisible(pointed && clearable);
    }
}

void OptionsPanel::CaptureKey(std::uint16_t key)
{
    const auto &row = keyRows_.at(*captureRow_);
    auto candidate = options_.Values();
    const auto layout = std::ranges::find(inputLayouts_, row.mode, &ModeInputLayout::modeId);
    auto resolved = ResolveInputLayout(*layout, SavedOverrides(candidate, row.mode));
    const auto binding = std::ranges::find(resolved.bindings, row.binding, &ModeInputBinding::id);
    binding->keys[row.slot] = key;
    auto &savedKeys = candidate.modeKeys[row.mode];
    savedKeys[row.binding] = binding->keys;
    UnassignKeyElsewhere(resolved, savedKeys, key, row.binding, row.slot);
    // OptionSettings still rejects reserved keys and any key the layout cannot leave unassigned.
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
    if (action.type == ActionType::SelectionChanged && action.source == windowModeCombo_->Id())
    {
        CancelKeyboardEdit();
        SelectWindowMode(action.selectedIndex);
        return true;
    }
    if (action.type == ActionType::SelectionChanged && action.source == notationCombo_->Id())
    {
        CancelKeyboardEdit();
        SelectNotation(action.selectedIndex);
        return true;
    }
    if (action.type == ActionType::SelectionChanged && action.source == renderRateCombo_->Id())
    {
        CancelKeyboardEdit();
        SelectRenderRate(action.selectedIndex);
        return true;
    }
    if (action.type != ActionType::Clicked)
        return false;
    if (auto *overlay = VisibleKeyOverlay())
    {
        if (action.source == overlay->close->Id())
        {
            CancelKeyboardEdit();
            HideKeyOverlays();
            hitTestDirty_ = true;
            return true;
        }
        // CanProcessAction admits only nodes inside the visible overlay.
        for (std::size_t i = 0; i < keyRows_.size(); ++i)
        {
            if (keyRows_[i].clear && action.source == keyRows_[i].clear->Id())
            {
                captureRow_ = i;
                CaptureKey(0);
                keyRows_[i].clear->SetVisible(false);
                return true;
            }
            if (action.source == keyRows_[i].button->Id())
            {
                CancelKeyboardEdit();
                captureRow_ = i;
                RefreshKeyControls();
                return true;
            }
        }
        return true;
    }
    for (auto &overlay : keyOverlays_)
        if (action.source == overlay.open->Id())
        {
            ShowKeyOverlay(overlay);
            return true;
        }
    if (action.source == songsField_->Id() || action.source == songsBrowse_->Id())
    {
        CancelKeyboardEdit();
        ChooseSongsFolder();
        return true;
    }
    if (action.source == restartKeyButton_->Id())
    {
        CancelKeyboardEdit();
        capturingRestartKey_ = true;
        RefreshRestartKeyControl();
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
