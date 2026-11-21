#include "OptionsPanel.h"
#include "Presentation/SceneWheel.h"
#include "Mode/PlayGameModeFactory.h"
#include "OptionsPanel/Submodules/OptionsPanelStyle.h"
#include "Presentation/SkinVisualComponent.h"

#include "Texts/Options/OptionTexts.h"
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

using namespace PhantomCore;

using namespace options_ui;

namespace
{
    constexpr Color Transparent{1, 1, 1, 0};
    constexpr float StatusTextWidth = 282, StatusLineHeight = 18;
} // namespace

void OptionsPanel::Initialize(Visual2DCanvas &canvas, AudioSystem &audio, IWindowControl *window)
{
    canvas_ = &canvas;
    audio_ = &audio;
    window_ = window;
    inputLayouts_ = AvailableInputLayouts();
    CreateFrame();
    CreateRail();
    CreateSections();
    tooltip_.Initialize(canvas);
    RefreshTexts();
    panel_->SetVisible(false);
}

void OptionsPanel::CreateFrame()
{
    auto &leftAnchor = canvas_->AnchorNode(Anchor::MiddleLeft);
    leftAnchor.SetZIndex(100);
    panel_ = &CreatePanel(leftAnchor, {-PanelWidth, -PanelHeight * 0.5F, PanelWidth, PanelHeight}, "Options.Panel");
    panel_->SetZIndex(100);
    SetPanelColor(*panel_, DrawerColor);
    BindPart(*panel_, "Drawer");
    panel_->AddComponent<RectangleCollider2DComponent>();
    panel_->AddComponent<PointerReceiverComponent>();
    panelId_ = panel_->Id();

    // Header: title and close button above the scrolling body.
    title_ = &CreateCaption(*panel_, {BodyLeft + 24, 21, 240, 30}, 26, 900, HeadingColor, "Options.Title");
    close_ = &CreateButton(*panel_, TopLeft({PanelWidth - 46, 18, 32, 32}, PanelHeight), L"", "Options.Close");
    SetColors(*close_, Transparent, ui_colors::Rgb(0x1F4C99, .08F), ui_colors::Rgb(0x1F4C99, .16F), Transparent);
    CreateIcon(*close_, {6, 6, 20, 20}, "Icons.x", ui_colors::theme::BrandText, "Icon");
    headerRule_ = &CreateSprite(*panel_, TopLeft({BodyLeft, HeaderHeight - 1, BodyWidth, 1}, PanelHeight), {},
                                "Options.HeaderRule");
    SetPanelColor(*headerRule_, ui_colors::theme::Rule);

    viewport_ = &panel_->CreateChild("Options.Viewport");
    content_ = &viewport_->CreateChild("Options.Content");

    scrollbar_.Attach(*panel_, "Options.Scrollbar");
    scrollbar_.SetZIndex(5);

    // Footer: recovery and error notices stay outside the scrolling body.
    footer_ = &panel_->CreateChild("Options.Footer");
    footerRule_ = &CreateSprite(*footer_, {}, {}, "Rule");
    SetPanelColor(*footerRule_, ui_colors::theme::Rule);
    statusBar_ = &CreateSprite(*footer_, {}, {}, "Bar");
    statusIcon_ = &CreateSprite(*footer_, {}, {}, "Icon");
    ApplyIcon(*statusIcon_, "Icons.alert");
    icons_.emplace_back(statusIcon_, "Icons.alert");
    statusLabel_ = &CreateLabel(*footer_, {}, L"", "Options.Status");
    StyleCaption(RequireComponent<TextVisualComponent>(*statusLabel_), 13, 400, ui_colors::theme::Danger);
    footer_->SetVisible(false);
}

void OptionsPanel::BindPart(Visual2DNode &node, std::string key)
{
    if (visuals_)
        BindSkinPart(node, *visuals_, "Options", std::move(key));
}

Visual2DNode &OptionsPanel::CreateIcon(Visual2DNode &parent, const Rect bounds, std::string key, const Color tint,
                                       std::string name)
{
    auto &icon = CreateSprite(parent, TopLeft(bounds, parent.NodeSize().height), {}, std::move(name));
    SetPanelColor(icon, tint);
    ApplyIcon(icon, key);
    icons_.emplace_back(&icon, std::move(key));
    return icon;
}

void OptionsPanel::SetIconKey(Visual2DNode &icon, std::string key)
{
    const auto entry = std::ranges::find(icons_, &icon, &std::pair<Visual2DNode *, std::string>::first);
    if (entry == icons_.end())
        throw std::logic_error("An options icon must be created before its key changes.");
    if (entry->second == key)
        return;
    entry->second = std::move(key);
    ApplyIcon(icon, entry->second);
}

void OptionsPanel::ApplyIcon(Visual2DNode &icon, const std::string &key)
{
    const auto path = visuals_ ? SkinSetSelection::Instance().Image("Common", key) : std::filesystem::path{};
    auto &sprite = RequireComponent<SpriteVisualComponent>(icon);
    // Without an icon image the node would draw a solid tinted square, so it stays transparent.
    auto style = sprite.Style();
    const float alpha = path.empty() ? 0.0F : 1.0F;
    style.normal.alpha = style.hovered.alpha = style.pressed.alpha = style.disabled.alpha = alpha;
    sprite.SetStyle(style);
    if (!path.empty())
        sprite.SetImage(visuals_->RegisterImage(path));
}

void OptionsPanel::RefreshIcons()
{
    for (const auto &[icon, key] : icons_)
        ApplyIcon(*icon, key);
}

void OptionsPanel::CreateLanguageControls()
{
    const float top = NextField(ControlHeight);
    languageLabel_ = &CreateFieldLabel(*content_, top, "Options.Language.Label");
    std::vector<std::wstring> languageNames;
    for (const LanguageProfile &profile : texts_.Languages())
        languageNames.emplace_back(profile.displayName);
    languageCombo_ = &CreateDropdown(*content_,
                                     TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_),
                                     std::move(languageNames), "Options.Language.Combo");
    languageComboId_ = languageCombo_->Id();
    ConfigureCombo(*languageCombo_);
    AddFieldLine(*content_, *languageCombo_, "Options.Language.Line");
}

void OptionsPanel::CreateSkinControls()
{
    const float top = NextField(ControlHeight);
    skinLabel_ = &CreateFieldLabel(*content_, top, "Options.Skin.Label");
    skinNames_ = SkinSetSelection::Instance().AvailableNames();
    skinCombo_ = &CreateDropdown(*content_,
                                 TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_),
                                 skinNames_, "Options.Skin.Combo");
    skinComboId_ = skinCombo_->Id();
    ConfigureCombo(*skinCombo_);
    AddFieldLine(*content_, *skinCombo_, "Options.Skin.Line");
}

void OptionsPanel::CreateAudioControls()
{
    const std::array fields{std::pair{&middlewareLabel_, &middlewareCombo_}, std::pair{&outputLabel_, &outputCombo_},
                            std::pair{&driverLabel_, &driverCombo_}, std::pair{&bufferLabel_, &bufferCombo_}};
    const std::array<std::string, 4> names{"Middleware", "Output", "Driver", "Buffer"};
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        const float top = NextField(ControlHeight);
        *fields[i].first = &CreateFieldLabel(*content_, top, "Options." + names[i] + ".Label");
        auto &combo = CreateDropdown(*content_,
                                     TopLeft({ContentLeft, top + ControlOffset, FieldWidth, ControlHeight}, contentHeight_),
                                     {}, "Options." + names[i] + ".Combo");
        ConfigureCombo(combo);
        AddFieldLine(*content_, combo, "Options." + names[i] + ".Line");
        *fields[i].second = &combo;
    }
    RequireDropdown(*middlewareCombo_)
        .SetItems({std::wstring(texts_.Get("Options.middlewareFmod"))});
    outputComboId_ = outputCombo_->Id();
    driverComboId_ = driverCombo_->Id();
}

void OptionsPanel::Shutdown() noexcept
{
    tooltip_.Shutdown();
    keyRows_.clear();
    keyOverlays_.clear();
    modeLabels_.clear();
    inputLayouts_.clear();
    resolutions_.clear();
    icons_.clear();
    sliders_ = {};
    sliderDrag_.reset();
    offsetDrag_.reset();
    window_ = nullptr;
    sections_ = {};
    activeSection_ = 0;
    heldSection_.reset();
    railSkinSection_.reset();
    panel_ = rail_ = title_ = close_ = headerRule_ = nullptr;
    viewport_ = content_ = nullptr;
    scrollbar_.Detach();
    footer_ = footerRule_ = statusBar_ = statusIcon_ = statusLabel_ = nullptr;
    languageLabel_ = languageCombo_ = nullptr;
    songsLabel_ = songsField_ = songsBrowse_ = songsDefault_ = nullptr;
    skinLabel_ = skinCombo_ = nullptr;
    resolutionLabel_ = resolutionCombo_ = nullptr;
    windowModeLabel_ = windowModeCombo_ = nullptr;
    notationLabel_ = notationCombo_ = nullptr;
    renderRateLabel_ = renderRateCombo_ = nullptr;
    middlewareLabel_ = outputLabel_ = driverLabel_ = bufferLabel_ = nullptr;
    middlewareCombo_ = outputCombo_ = driverCombo_ = bufferCombo_ = nullptr;
    offsetLabel_ = offsetField_ = offsetUnit_ = nullptr;
    captureRow_.reset();
    restartKeyLabel_ = restartKeyButton_ = nullptr;
    capturingRestartKey_ = false;
    editingOffset_ = replaceOffsetDraft_ = false;
    offsetDraft_.clear();
    skinNames_.clear();
    driverIndices_.clear();
    panelId_ = languageComboId_ = skinComboId_ = outputComboId_ = driverComboId_ = {};
    previousDragPoint_.reset();
    contentHeight_ = viewportHeight_ = footerHeight_ = layoutCursor_ = scrollOffset_ = 0.0F;
    railPointerInside_ = railKeyboard_ = railExpanded_ = false;
    railHoverSeconds_ = 0.0F;
    hitTestDirty_ = false;
    animationProgress_ = 0.0F;
    targetOpen_ = false;
    dragging_ = false;
    optionRevision_ = 0;
    audio_ = nullptr;
    canvas_ = nullptr;
    status_ = Status::None;
    statusDetail_.clear();
}

void OptionsPanel::Toggle()
{
    if (panel_ == nullptr)
        return;
    targetOpen_ = !targetOpen_;
    tooltip_.Hide();
    HideKeyOverlays();
    CancelKeyboardEdit();
    CollapseCombos();
    sliderDrag_.reset();
    offsetDrag_.reset();
    scrollbar_.Release();
    railKeyboard_ = railPointerInside_ = false;
    railHoverSeconds_ = 0.0F;
    if (railExpanded_)
    {
        railExpanded_ = false;
        ApplyRailWidth();
    }
    if (targetOpen_)
    {
        RefreshSkinSets();
        RefreshDisplayControls();
        RefreshIcons();
    }
    panel_->SetVisible(true);
    dragging_ = false;
    previousDragPoint_.reset();
    hitTestDirty_ = true;
}

void OptionsPanel::Update(const double deltaSeconds)
{
    if (panel_ == nullptr)
        return;
    if (optionRevision_ != options_.Revision())
        RefreshTexts();
    frameSeconds_ = deltaSeconds;
    const float direction = targetOpen_ ? 1.0F : -1.0F;
    animationProgress_ = std::clamp(
        animationProgress_ + direction * static_cast<float>(deltaSeconds / AnimationSeconds), 0.0F, 1.0F);
    ApplyAnimationPosition();
    UpdateRailExpansion(deltaSeconds);
    RefreshRail();
    if (!targetOpen_ && animationProgress_ <= 0.0F)
        panel_->SetVisible(false);
}

void OptionsPanel::Close(Visual2DInputRouter *inputRouter)
{
    CancelKeyboardEdit();
    tooltip_.Hide();
    // Closing is idempotent: another ESC during the slide must not reopen it.
    targetOpen_ = false;
    dragging_ = false;
    previousDragPoint_.reset();
    scrollbar_.Release();
    sliderDrag_.reset();
    offsetDrag_.reset();
    railKeyboard_ = railPointerInside_ = false;
    CollapseCombos();
    if (inputRouter)
        inputRouter->Reset(*canvas_);
    else
        hitTestDirty_ = true;
}

bool OptionsPanel::HandleKey(std::uint16_t virtualKey, Visual2DInputRouter &inputRouter)
{
    if (!IsVisible())
        return false;
    if (virtualKey != VK_ESCAPE)
    {
        if (!targetOpen_)
            return false;
        if (captureRow_)
        {
            CaptureKey(virtualKey);
            return true;
        }
        if (capturingRestartKey_)
        {
            CaptureRestartKey(virtualKey);
            return true;
        }
        if (editingOffset_)
        {
            EditOffset(virtualKey);
            return true;
        }
        return false;
    }
    if (VisibleKeyOverlay() != nullptr)
    {
        if (captureRow_)
            CancelKeyboardEdit();
        else
            HideKeyOverlays();
        inputRouter.Reset(*canvas_);
        return true;
    }
    Close(&inputRouter);
    return true;
}

bool OptionsPanel::ProcessInput(const InputState &input, const std::optional<Point> canvasPointer,
                                const NodeId hoveredNode, Visual2DInputRouter &inputRouter)
{
    if (hitTestDirty_)
    {
        inputRouter.InvalidateHitTest();
        hitTestDirty_ = false;
    }
    // Rail names matter while the rail is folded; the key modal has no hints.
    const bool hints = targetOpen_ && VisibleKeyOverlay() == nullptr;
    tooltip_.Update(frameSeconds_, input, hoveredNode, hints && !railExpanded_);
    if (input.WasKeyPressed(VK_ESCAPE) && HandleKey(VK_ESCAPE, inputRouter))
        return true;
    if (OwnsKeyboard())
    {
        bool consumed = false;
        for (const auto &event : input.Events())
            if (event.type == InputEventType::KeyPressed)
                consumed |= HandleKey(event.code, inputRouter);
        if (consumed)
            return true;
    }
    if (const auto *overlay = VisibleKeyOverlay())
    {
        UpdateKeyClearButtons(hoveredNode);
        return !canvasPointer || !overlay->panel->BoundsInCanvas().Contains(*canvasPointer);
    }
    const bool panelShown = IsVisible() && panel_ != nullptr;
    if (!panelShown)
        return false;

    // The rail moves between sections with Up/Down; a pointer move ends keyboard expansion.
    bool consumedKey = false;
    for (const auto &event : input.Events())
    {
        const bool pointerEvent = event.type == InputEventType::MouseMoved ||
                                  event.type == InputEventType::MouseButtonPressed;
        if (pointerEvent)
            railKeyboard_ = false;
        if (!targetOpen_ || event.type != InputEventType::KeyPressed)
            continue;
        if (event.code == VK_UP || event.code == VK_DOWN)
        {
            StepSection(event.code == VK_UP ? -1 : 1);
            consumedKey = true;
        }
    }
    railPointerInside_ = canvasPointer.has_value() && rail_->BoundsInCanvas().Contains(*canvasPointer);
    if (consumedKey)
        inputRouter.InvalidateHitTest();

    if (ProcessSliderPointer(input, canvasPointer, hoveredNode))
        return consumedKey;
    // A finished offset drag also drops the click the release produced, so no keyboard edit starts.
    if (ProcessOffsetDrag(input, canvasPointer, hoveredNode))
        return true;
    // The shared scrollbar follows its drag even when the pointer leaves the panel.
    if (const auto offset = scrollbar_.Track(canvasPointer, input.IsMouseButtonDown(MouseButton::Left)))
    {
        scrollOffset_ = *offset;
        heldSection_.reset();
        ApplyScrollOffset(inputRouter);
    }
    const bool insidePanel = canvasPointer.has_value() && panel_->BoundsInCanvas().Contains(*canvasPointer);
    if (!insidePanel && !scrollbar_.IsDragging())
    {
        dragging_ = false;
        previousDragPoint_.reset();
        return consumedKey;
    }
    if (!insidePanel)
        return consumedKey;

    if (SceneWheelDelta(input) != 0.0F && !AnyComboExpanded())
    {
        scrollOffset_ -= SceneWheelDelta(input) * 36.0F;
        heldSection_.reset();
        ApplyScrollOffset(inputRouter);
    }

    const bool middlePressed = input.WasMouseButtonPressed(MouseButton::Middle);
    const bool blankLeftPressed = hoveredNode == panelId_ && input.WasMouseButtonPressed(MouseButton::Left);
    if (middlePressed || blankLeftPressed)
    {
        dragging_ = true;
        previousDragPoint_ = canvasPointer;
    }
    if (dragging_ && previousDragPoint_.has_value())
    {
        scrollOffset_ += canvasPointer->y - previousDragPoint_->y;
        previousDragPoint_ = canvasPointer;
        heldSection_.reset();
        ApplyScrollOffset(inputRouter);
    }
    if (input.WasMouseButtonReleased(MouseButton::Middle) || input.WasMouseButtonReleased(MouseButton::Left))
    {
        dragging_ = false;
        previousDragPoint_.reset();
    }
    return consumedKey;
}

bool OptionsPanel::ProcessAction(const Action &action)
{
    if (panel_ == nullptr || !targetOpen_)
        return false;
    if (!CanProcessAction(action.source))
        return false;
    if (action.type == ActionType::Clicked)
    {
        if (action.source == close_->Id())
        {
            Close(nullptr);
            return true;
        }
        for (std::size_t i = 0; i < sections_.size(); ++i)
            if (action.source == sections_[i].railButton->Id())
            {
                ScrollToSection(i);
                return true;
            }
    }
    if (ProcessFieldAction(action))
        return true;
    if (action.type != ActionType::SelectionChanged)
        return false;
    if (action.source == languageComboId_)
    {
        const auto languages = texts_.Languages();
        if (action.selectedIndex >= languages.size())
            return false;
        auto candidate = options_.Values();
        candidate.language = languages[action.selectedIndex].language;
        if (SaveOptions(candidate))
            texts_.SetLanguage(candidate.language);
        RefreshTexts();
        return true;
    }
    if (action.source == skinComboId_)
    {
        if (action.selectedIndex >= skinNames_.size())
            return false;
        auto candidate = options_.Values();
        candidate.skinSet = skinNames_[action.selectedIndex];
        if (SaveOptions(candidate))
        {
            std::string error;
            if (!SkinSetSelection::Instance().Select(candidate.skinSet, error))
            {
                status_ = Status::SaveFailed;
                statusDetail_ = std::move(error);
            }
            RefreshIcons();
            RefreshKeyImages();
        }
        RefreshTexts();
        return true;
    }
    if (action.source == middlewareCombo_->Id())
        return true; // FMOD is the sole installed middleware.
    if (action.source == outputComboId_ && action.selectedIndex < Outputs.size())
    {
        SelectOutput(Outputs[action.selectedIndex]);
        RefreshTexts();
        return true;
    }
    if (action.source == driverComboId_ && action.selectedIndex < driverIndices_.size())
    {
        SelectDriver(driverIndices_[action.selectedIndex]);
        RefreshTexts();
        return true;
    }
    if (action.source == bufferCombo_->Id() && action.selectedIndex < DspBufferLengths.size())
    {
        SelectBuffer(action.selectedIndex);
        RefreshTexts();
        return true;
    }
    return false;
}

void OptionsPanel::RefreshSkinSets()
{
    skinNames_ = SkinSetSelection::Instance().AvailableNames();
    if (skinCombo_ == nullptr)
        return;
    RequireDropdown(*skinCombo_).SetItems(skinNames_);
    RefreshTexts();
}

void OptionsPanel::RefreshTexts()
{
    if (panel_ == nullptr)
        return;
    const OptionTextSet &text = Options(texts_);
    SetCaption(*title_, std::wstring(text.title));
    constexpr std::array SectionTextKeys{"Options.sections.General", "Options.sections.Display",
                                         "Options.sections.Audio", "Options.sections.Input"};
    for (std::size_t i = 0; i < sections_.size(); ++i)
    {
        const std::wstring name(texts_.Get(SectionTextKeys[i]));
        SetCaption(*sections_[i].headerLabel, name);
        SetCaption(*sections_[i].railLabel, name);
        tooltip_.Set(*sections_[i].railButton, {name});
    }
    tooltip_.Set(*close_, {L"", L"Esc"});
    SetCaption(*languageLabel_, std::wstring(text.language));
    auto &combo = RequireDropdown(*languageCombo_);
    const auto languages = texts_.Languages();
    for (std::size_t index = 0; index < languages.size(); ++index)
    {
        if (languages[index].language == texts_.CurrentLanguage())
        {
            combo.SetSelectedIndex(index);
            break;
        }
    }
    SetCaption(*skinLabel_, std::wstring(text.skin));
    auto &skinCombo = RequireDropdown(*skinCombo_);
    const auto selected = std::ranges::find(skinNames_, SkinSetSelection::Instance().CurrentName());
    if (selected != skinNames_.end())
        skinCombo.SetSelectedIndex(static_cast<std::size_t>(selected - skinNames_.begin()));
    RefreshAudioControls();
    RefreshSliderControls();
    RefreshDisplayControls();
    RefreshOffsetControls();
    RefreshSongsControls();
    RefreshKeyControls();
    RefreshStatus();
    RefreshRail();
    optionRevision_ = options_.Revision();
}

void OptionsPanel::RefreshAudioControls()
{
    const auto &text = Options(texts_);
    const std::array labels{std::pair{middlewareLabel_, text.middleware}, std::pair{outputLabel_, text.output},
                            std::pair{driverLabel_, text.driver}, std::pair{bufferLabel_, text.buffer}};
    for (const auto &[node, value] : labels)
        SetCaption(*node, std::wstring(value));

    auto &output = RequireDropdown(*outputCombo_);
    std::vector<std::wstring> names;
    for (auto backend : Outputs)
        names.push_back(OutputName(backend, text));
    output.SetItems(std::move(names));
    const auto requested = std::ranges::find(Outputs, audio_->RequestedOutput());
    if (requested != Outputs.end())
        output.SetSelectedIndex(static_cast<std::size_t>(requested - Outputs.begin()));

    names.clear();
    driverIndices_.clear();
    for (const auto &driver : audio_->OutputDrivers())
    {
        driverIndices_.push_back(driver.driverIndex);
        names.push_back(std::to_wstring(driver.driverIndex) + L": " + WideText(driver.name));
    }
    auto &driver = RequireDropdown(*driverCombo_);
    if (names.empty())
        names.emplace_back(text.noDrivers);
    driver.SetItems(std::move(names));
    driverCombo_->SetEnabled(!driverIndices_.empty());
    const auto selected = std::ranges::find(driverIndices_, audio_->ActiveDriverIndex());
    if (selected != driverIndices_.end())
        driver.SetSelectedIndex(static_cast<std::size_t>(selected - driverIndices_.begin()));

    // The saved buffer, which the running mixer uses only after a restart (StatusText says so).
    names.clear();
    for (const auto length : DspBufferLengths)
        names.push_back(std::to_wstring(length) + L" " + std::wstring(text.bufferSamples));
    auto &buffer = RequireDropdown(*bufferCombo_);
    buffer.SetItems(std::move(names));
    const auto savedBuffer = std::ranges::find(DspBufferLengths, options_.Values().dspBufferLength);
    if (savedBuffer != DspBufferLengths.end())
        buffer.SetSelectedIndex(static_cast<std::size_t>(savedBuffer - DspBufferLengths.begin()));
}

bool OptionsPanel::BufferAwaitsRestart() const noexcept
{
    // A backend that reports no buffer (none, or a test double) cannot be compared.
    const auto running = audio_ != nullptr ? audio_->DspBufferLength() : 0U;
    return running != 0 && running != options_.Values().dspBufferLength;
}

void OptionsPanel::SelectBuffer(const std::size_t index)
{
    auto candidate = options_.Values();
    candidate.dspBufferLength = DspBufferLengths[index];
    static_cast<void>(SaveOptions(candidate));
}

bool OptionsPanel::SaveOptions(const OptionValues &candidate)
{
    const bool saved = options_.Save(candidate, statusDetail_);
    status_ = saved ? Status::None : Status::SaveFailed;
    return saved;
}

std::string OptionsPanel::DriverName(const int index) const
{
    for (const auto &driver : audio_->OutputDrivers())
        if (driver.driverIndex == index)
            return driver.name;
    return {};
}

void OptionsPanel::RestoreAudio(const AudioOutputBackend output, const int driver, std::string &error)
{
    std::string restorationError;
    if (!audio_->SetOutputBackend(output, restorationError) ||
        (driver >= 0 && !audio_->SetOutputDriver(driver, restorationError)))
        error += " | Could not restore the previous audio selection: " + restorationError;
}

void OptionsPanel::SelectOutput(const AudioOutputBackend output)
{
    const auto previousOutput = audio_->RequestedOutput();
    const int previousDriver = audio_->ActiveDriverIndex();
    auto candidate = options_.Values();
    candidate.audioOutput = output;
    if (!audio_->SetOutputBackend(output, statusDetail_))
    {
        RestoreAudio(previousOutput, previousDriver, statusDetail_);
        status_ = Status::AudioFailed;
        return;
    }
    candidate.driverIndex = audio_->ActiveDriverIndex();
    candidate.driverName = DriverName(candidate.driverIndex);
    if (!SaveOptions(candidate))
        RestoreAudio(previousOutput, previousDriver, statusDetail_);
}

void OptionsPanel::SelectDriver(const int index)
{
    const int previousDriver = audio_->ActiveDriverIndex();
    auto candidate = options_.Values();
    candidate.audioOutput = audio_->RequestedOutput();
    candidate.driverIndex = index;
    candidate.driverName = DriverName(index);
    if (!audio_->SetOutputDriver(index, statusDetail_))
    {
        status_ = Status::AudioFailed;
        return;
    }
    if (!SaveOptions(candidate))
        RestoreAudio(audio_->RequestedOutput(), previousDriver, statusDetail_);
}

std::wstring OptionsPanel::StatusText(bool &error) const
{
    const auto &text = Options(texts_);
    std::wstring value;
    const auto &saved = options_.Values();
    // A saved name identifies the driver; without one, a saved index must still be active.
    const auto savedDriverUnavailable = [&]
    {
        const auto active = audio_->ActiveDriverIndex();
        if (!saved.driverName.empty())
            return saved.driverName != DriverName(active);
        return saved.driverIndex >= 0 && saved.driverIndex != active;
    };
    error = status_ != Status::None;
    if (status_ == Status::SaveFailed)
        value = text.saveError;
    else if (status_ == Status::AudioFailed)
        value = text.audioError;
    else if (status_ == Status::WindowFailed)
        value = text.windowError;
    else if (status_ == Status::InvalidInput)
        value = text.inputError;
    else if (audio_->RequestedOutput() != saved.audioOutput)
        value = std::wstring(text.outputFallback) + L" " + OutputName(audio_->ActiveOutput(), text);
    else if (savedDriverUnavailable())
        value = text.driverUnavailable;
    else if (BufferAwaitsRestart())
        value = text.bufferRestart;
    if (!statusDetail_.empty())
        value += L"\n" + WideText(statusDetail_);
    if (!options_.LoadWarning().empty())
    {
        if (!value.empty())
            value += L"\n";
        value += std::wstring(text.loadWarning) + L"\n" + WideText(options_.LoadWarning());
    }
    return value;
}

void OptionsPanel::RefreshStatus()
{
    bool error = false;
    const auto value = WrapText(StatusText(error), 13, StatusTextWidth);
    SetCaption(*statusLabel_, value);
    const auto colour = error ? ui_colors::theme::Danger : WarningColor;
    RequireComponent<TextVisualComponent>(*statusLabel_).SetTextColor(colour);
    SetPanelColor(*statusBar_, colour);
    TintIcon(*statusIcon_, colour);
    const auto lines = 1 + std::ranges::count(value, L'\n');
    footerHeight_ = value.empty() ? 0.0F
                                  : std::max(FooterMinimumHeight, 36.0F + StatusLineHeight * static_cast<float>(lines));
    footer_->SetVisible(!value.empty());
    LayoutFooter();
    for (auto &overlay : keyOverlays_)
        RefreshKeyOverlay(overlay);
    UpdateContentLayout();
}

void OptionsPanel::LayoutFooter()
{
    const float height = std::max(footerHeight_, FooterMinimumHeight);
    footer_->SetBounds({BodyLeft, 0, BodyWidth, height});
    footerRule_->SetBounds(TopLeft({0, 0, BodyWidth, 1}, height));
    statusBar_->SetBounds(TopLeft({24, 10, 3, height - 24}, height));
    statusIcon_->SetBounds(TopLeft({36, 16, 18, 18}, height));
    statusLabel_->SetBounds(TopLeft({62, 16, StatusTextWidth, height - 30}, height));
}

bool OptionsPanel::IsVisible() const noexcept
{
    return panel_ != nullptr && (targetOpen_ || animationProgress_ > 0.0F);
}

void OptionsPanel::ApplyAnimationPosition()
{
    const float eased = animationProgress_ * animationProgress_ * (3.0F - 2.0F * animationProgress_);
    panel_->SetBounds({-PanelWidth * (1.0F - eased), -PanelHeight * 0.5F, PanelWidth, PanelHeight});
}

void OptionsPanel::ApplyScrollOffset(Visual2DInputRouter &inputRouter)
{
    UpdateContentLayout();
    inputRouter.InvalidateHitTest();
}
