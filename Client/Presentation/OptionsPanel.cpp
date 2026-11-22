#include "OptionsPanel.h"
#include "OptionsPanel/Submodules/OptionsAudio.h"
#include "OptionsPanel/Submodules/OptionsDisplay.h"
#include "OptionsPanel/Submodules/OptionsGeneral.h"
#include "OptionsPanel/Submodules/OptionsInput.h"
#include "OptionsPanel/Submodules/OptionsPanelStyle.h"
#include "Presentation/SceneWheel.h"
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

OptionsPanel::OptionsPanel(TextCatalog &texts, OptionSettings &options, ScreenVisual2DManager *visuals)
    // The categories see the panel only as OptionsContext, a private base converted here inside the class.
    : categories_{std::make_unique<OptionsGeneral>(static_cast<OptionsContext &>(*this)),
                  std::make_unique<OptionsDisplay>(static_cast<OptionsContext &>(*this)),
                  std::make_unique<OptionsAudio>(static_cast<OptionsContext &>(*this)),
                  std::make_unique<OptionsInput>(static_cast<OptionsContext &>(*this))},
      texts_(texts), options_(options), visuals_(visuals)
{
}

OptionsPanel::~OptionsPanel() = default;

void OptionsPanel::Initialize(Visual2DCanvas &canvas, AudioSystem &audio, IWindowControl *window)
{
    canvas_ = &canvas;
    audio_ = &audio;
    window_ = window;
    for (const auto &category : categories_)
        category->Load();
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

void OptionsPanel::SkinChanged()
{
    RefreshIcons();
    for (const auto &category : categories_)
        category->OnSkinChanged();
}

void OptionsPanel::Shutdown() noexcept
{
    tooltip_.Shutdown();
    for (const auto &category : categories_)
        category->Shutdown();
    icons_.clear();
    window_ = nullptr;
    sections_ = {};
    activeSection_ = 0;
    heldSection_.reset();
    railSkinSection_.reset();
    panel_ = rail_ = title_ = close_ = headerRule_ = nullptr;
    viewport_ = content_ = nullptr;
    scrollbar_.Detach();
    footer_ = footerRule_ = statusBar_ = statusIcon_ = statusLabel_ = nullptr;
    panelId_ = {};
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
    status_ = OptionsStatus::None;
    statusDetail_.clear();
}

void OptionsPanel::Toggle()
{
    if (panel_ == nullptr)
        return;
    targetOpen_ = !targetOpen_;
    tooltip_.Hide();
    // Every drag stops and the key modal closes, whichever way the panel goes.
    for (const auto &category : categories_)
        category->OnToggle();
    CancelKeyboardEdit();
    CollapseCombos();
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
        for (const auto &category : categories_)
            category->OnOpened();
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
    for (const auto &category : categories_)
        category->OnClosed();
    railKeyboard_ = railPointerInside_ = false;
    CollapseCombos();
    if (inputRouter)
        inputRouter->Reset(*canvas_);
    else
        hitTestDirty_ = true;
}

bool OptionsPanel::OwnsKeyboard() const noexcept
{
    return std::ranges::any_of(categories_, [](const auto &category) { return category->OwnsKeyboard(); });
}

void OptionsPanel::CancelKeyboardEdit()
{
    for (const auto &category : categories_)
        category->CancelKeyboardEdit();
}

OptionsCategory *OptionsPanel::ModalCategory() const noexcept
{
    const auto shown = std::ranges::find_if(categories_, [](const auto &category) {
        return category->VisibleModal() != nullptr;
    });
    return shown == categories_.end() ? nullptr : shown->get();
}

bool OptionsPanel::HandleKey(std::uint16_t virtualKey, Visual2DInputRouter &inputRouter)
{
    if (!IsVisible())
        return false;
    if (virtualKey != VK_ESCAPE)
    {
        if (!targetOpen_)
            return false;
        // One keyboard edit at a time: the category that holds it takes the key.
        const auto editing = std::ranges::find_if(categories_, [](const auto &category) {
            return category->OwnsKeyboard();
        });
        if (editing == categories_.end())
            return false;
        (*editing)->HandleKey(virtualKey);
        return true;
    }
    if (auto *modal = ModalCategory())
    {
        modal->EscapeModal();
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
    auto *modal = ModalCategory();
    const bool hints = targetOpen_ && modal == nullptr;
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
    modal = ModalCategory();
    if (modal != nullptr)
    {
        modal->UpdateModal(hoveredNode);
        return !canvasPointer || !modal->VisibleModal()->BoundsInCanvas().Contains(*canvasPointer);
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
    return ProcessPointer(input, canvasPointer, hoveredNode, inputRouter, consumedKey);
}

bool OptionsPanel::ProcessPointer(const InputState &input, const std::optional<Point> canvasPointer,
                                  const NodeId hoveredNode, Visual2DInputRouter &inputRouter, const bool consumedKey)
{
    // A slider drag holds the frame; a finished offset or position drag also drops the release's click.
    for (const auto &category : categories_)
    {
        const auto pointer = category->ProcessPointer(input, canvasPointer, hoveredNode);
        if (pointer == OptionsPointer::Sliding)
            return consumedKey;
        if (pointer == OptionsPointer::Consumed)
            return true;
    }
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
    return std::ranges::any_of(categories_, [&action](const auto &category) { return category->ProcessAction(action); });
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
    for (const auto &category : categories_)
        category->Refresh();
    RefreshStatus();
    RefreshRail();
    optionRevision_ = options_.Revision();
}

bool OptionsPanel::Save(const OptionValues &candidate)
{
    const bool saved = options_.Save(candidate, statusDetail_);
    status_ = saved ? OptionsStatus::None : OptionsStatus::SaveFailed;
    return saved;
}

std::wstring OptionsPanel::StatusText(bool &error) const
{
    const auto &text = Options(texts_);
    std::wstring value;
    error = status_ != OptionsStatus::None;
    if (status_ == OptionsStatus::SaveFailed)
        value = text.saveError;
    else if (status_ == OptionsStatus::AudioFailed)
        value = text.audioError;
    else if (status_ == OptionsStatus::WindowFailed)
        value = text.windowError;
    else if (status_ == OptionsStatus::InvalidInput)
        value = text.inputError;
    else
    {
        // Without a failure the first category notice shows (Audio's fallback output, driver or restart).
        for (const auto &category : categories_)
        {
            value = category->Notice();
            if (!value.empty())
                break;
        }
    }
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
    for (const auto &category : categories_)
        category->OnStatusChanged();
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
