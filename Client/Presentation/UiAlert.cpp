#include "UiAlert.h"
#include "App/SkinSetSelection.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include "Presentation/UiTextWrap.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <Windows.h>

using namespace PhantomCore;

namespace
{
    // Mock-up geometry (popups.css .modal-alert), in 1280x720 logical units from the panel's top-left.
    constexpr float PaddingTop = 30, PaddingSide = 28, PaddingBottom = 20, Gap = 12, Band = 4;
    constexpr float IconSize = 28, Indent = 40, HeadHeight = 28;
    constexpr float TextLeft = PaddingSide + Indent, BodyWidth = UiAlert::Width - TextLeft - PaddingSide;
    constexpr float TitleSize = 22, MessageSize = 15, MessageLine = 22;
    constexpr float DetailSize = 12, DetailLine = 15, DetailPaddingX = 12, DetailPaddingY = 8, DetailMaximum = 120;
    constexpr float DetailTextWidth = BodyWidth - DetailPaddingX * 2, DetailStripe = 3, DetailWheelStep = DetailLine * 2;
    constexpr float FooterGap = 4, FooterPadding = 16, ButtonHeight = 40, ButtonGap = 8, Cut = 14;
    constexpr float ButtonTextSize = 14, PrimaryPadding = 26, ButtonPadding = 16, ButtonLine = 2;
    // The scrim covers any Canvas; the panel is placed in its local space around the Canvas centre.
    constexpr float ScrimExtent = 20000, ScrimOrigin = -ScrimExtent * .5F;
    // Above every layer of its Canvas: song select panels (1000), drop-down lists (260) and tooltips (300).
    constexpr int ScrimZ = 2000;

    constexpr auto ScrimColour = ui_colors::Rgb(0x142F5E, .42F), PanelColour = ui_colors::Rgb(0xF0F8FF, .98F);
    constexpr auto DetailColour = ui_colors::Rgb(0x1F4C99, .06F), Transparent = ui_colors::Rgb(0xFFFFFF, 0);
    constexpr auto PrimaryColour = ui_colors::theme::Brand, PrimaryHover = ui_colors::Rgb(0x2A5DB4);
    constexpr auto PrimaryPressed = ui_colors::theme::BrandDeep;
    constexpr auto PlainColour = ui_colors::Rgb(0xFFFFFF, .72F), PlainHover = ui_colors::Rgb(0xFFFFFF);
    constexpr auto PlainPressed = ui_colors::Rgb(0xE6EEFC), PlainLine = ui_colors::Rgb(0xB5CFF5);
    constexpr auto DangerLine = ui_colors::Rgb(0xF1C5CF);
    // A gold icon on the light panel would fall under 3:1 contrast, so the warning icon is darker.
    constexpr auto WarningIcon = ui_colors::Rgb(0xB7790B);

    [[nodiscard]] Color BandColour(const AlertKind kind) noexcept
    {
        switch (kind)
        {
        case AlertKind::Warning: return ui_colors::theme::Sun;
        case AlertKind::Info: return ui_colors::theme::Brand;
        default: return ui_colors::theme::Danger;
        }
    }
    [[nodiscard]] Color IconColour(const AlertKind kind) noexcept
    {
        return kind == AlertKind::Warning ? WarningIcon : BandColour(kind);
    }
    [[nodiscard]] constexpr Rect TopLeft(const Rect bounds, const float parentHeight) noexcept
    {
        return {bounds.x, parentHeight - bounds.y - bounds.height, bounds.width, bounds.height};
    }
    template <typename Component> Component &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<Component>();
        if (component == nullptr)
            throw std::logic_error("An alert node is missing a required component.");
        return *component;
    }
    void Paint(Visual2DNode &node, const Color colour)
    {
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        Require<SpriteVisualComponent>(node).SetStyle(style);
    }
    Visual2DNode &Fill(Visual2DNode &parent, std::string name, const Color colour)
    {
        auto &node = CreateSprite(parent, {}, {}, std::move(name));
        Paint(node, colour);
        return node;
    }
    Visual2DNode &Text(Visual2DNode &parent, std::string name)
    {
        return CreateLabel(parent, {}, L"", std::move(name));
    }
    void StyleText(Visual2DNode &node, std::wstring value, const TextFont &font, const float size,
                   const std::uint16_t weight, const Color colour, const TextAlignment alignment)
    {
        auto &text = Require<TextVisualComponent>(node);
        if (text.Text() != value)
            text.SetText(std::move(value));
        text.SetFont(font);
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(colour);
        text.SetHorizontalAlignment(alignment);
    }
    [[nodiscard]] float LineCount(const std::wstring_view text) noexcept
    {
        return static_cast<float>(1 + std::ranges::count(text, L'\n'));
    }
    [[nodiscard]] float ButtonWidth(const AlertButton &button, const float textWidth) noexcept
    {
        return textWidth + 2 * (button.primary ? PrimaryPadding : ButtonPadding);
    }
    // One slanted end of the primary button: a large square turned so that one edge runs along the
    // slant, clipped by the end's box. The left end fills below its diagonal, the right one above.
    void PlaceCap(Visual2DNode &end, Visual2DNode &square, const float height, const bool leftEnd)
    {
        end.SetClipRect({0, 0, Cut, height});
        const float length = std::hypot(Cut, height), side = length * 2;
        const float direction = leftEnd ? 1.0F : -1.0F;
        const Point normal{direction * height / length, -direction * Cut / length};
        square.SetPivot({.5F, .5F});
        square.SetSize({side, side});
        square.SetPosition({Cut * .5F + normal.x * side * .5F, height * .5F + normal.y * side * .5F});
        square.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, std::atan2(height, Cut));
    }
    void CopyToClipboard(const std::wstring &text)
    {
        if (text.empty() || !OpenClipboard(nullptr))
            return;
        static_cast<void>(EmptyClipboard());
        const auto bytes = (text.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes))
        {
            if (void *target = GlobalLock(memory))
            {
                std::memcpy(target, text.c_str(), bytes);
                GlobalUnlock(memory);
                // The clipboard owns the memory once SetClipboardData succeeds.
                if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr)
                    GlobalFree(memory);
            }
            else
                GlobalFree(memory);
        }
        CloseClipboard();
    }
} // namespace

void UiAlert::Initialize(Visual2DCanvas &canvas, ScreenVisual2DManager *visuals)
{
    canvas_ = &canvas;
    visuals_ = visuals;
    scrim_ = &CreatePanel(canvas.Root(), {ScrimOrigin, ScrimOrigin, ScrimExtent, ScrimExtent}, "Alert");
    scrim_->SetZIndex(ScrimZ);
    Paint(*scrim_, ScrimColour);
    // A press outside the panel lands here and goes no further.
    scrim_->AddComponent<RectangleCollider2DComponent>();
    scrim_->AddComponent<PointerReceiverComponent>();

    panel_ = &CreatePanel(*scrim_, {}, "Alert.Panel");
    Paint(*panel_, PanelColour);
    band_ = &Fill(*panel_, "Alert.Band", BandColour(AlertKind::Error));
    icon_ = &CreateSprite(*panel_, {}, {}, "Alert.Icon");
    title_ = &Text(*panel_, "Alert.Title");
    message_ = &Text(*panel_, "Alert.Message");
    detailBox_ = &Fill(*panel_, "Alert.Detail", DetailColour);
    detailLine_ = &Fill(*detailBox_, "Stripe", BandColour(AlertKind::Error));
    detailView_ = &CreatePanel(*detailBox_, {}, "View");
    Paint(*detailView_, Transparent);
    detailView_->AddComponent<RectangleCollider2DComponent>();
    detailView_->AddComponent<PointerReceiverComponent>(
        [this](Visual2DNode &, const PointerEvent &event, std::vector<Action> &) {
            if (event.type == PointerEventType::Wheel)
                ScrollDetail(event.wheelDelta);
        });
    detail_ = &Text(*detailView_, "Text");
    rule_ = &Fill(*panel_, "Alert.Rule", ui_colors::theme::Rule);
    scrim_->SetVisible(false);
}

void UiAlert::Shutdown() noexcept
{
    // The nodes belong to the Canvas, which its host releases.
    canvas_ = nullptr;
    visuals_ = nullptr;
    scrim_ = panel_ = band_ = icon_ = title_ = message_ = nullptr;
    detailBox_ = detailLine_ = detailView_ = detail_ = rule_ = nullptr;
    buttons_.clear();
    chosen_.reset();
    hovered_.reset();
    pressed_.reset();
}

void UiAlert::EnsureButtons(const std::size_t count)
{
    while (buttons_.size() < count)
    {
        const std::size_t index = buttons_.size();
        Button button;
        button.node = &CreateButton(*panel_, {}, L"", "Alert.Button" + std::to_string(index));
        Paint(*button.node, Transparent);
        // The engine button already receives pointer events; its handler drives the alert's states.
        Require<PointerReceiverComponent>(*button.node)
            .SetHandler([this, index](Visual2DNode &, const PointerEvent &event, std::vector<Action> &) {
                Pointer(index, event);
            });
        button.fill = &Fill(*button.node, "Fill", PlainColour);
        button.line = &Fill(*button.node, "Line", PlainLine);
        for (std::size_t end = 0; end < 2; ++end)
        {
            button.caps[end] = &CreatePanel(*button.node, {}, end == 0 ? "LeftEnd" : "RightEnd");
            Paint(*button.caps[end], Transparent);
            button.capFills[end] = &Fill(*button.caps[end], "Fill", PrimaryColour);
        }
        button.label = &Text(*button.node, "Label");
        buttons_.push_back(button);
    }
}

void UiAlert::Show(AlertContent content)
{
    if (canvas_ == nullptr)
        throw std::logic_error("The alert must be initialized before it is shown.");
    if (content.buttons.empty())
        throw std::invalid_argument("An alert needs at least one button.");
    content_ = std::move(content);
    chosen_.reset();
    hovered_.reset();
    pressed_.reset();
    detailScroll_ = 0;
    keysReady_ = false;

    const auto colour = BandColour(content_.kind);
    Paint(*band_, colour);
    Paint(*detailLine_, colour);
    // The skin's white line icon, tinted; without the skin it stays out rather than drawing a square.
    const auto path = visuals_ != nullptr
                          ? SkinSetSelection::Instance().Image("Common", content_.kind == AlertKind::Info ? "Icons.info"
                                                                                                          : "Icons.alert")
                          : std::filesystem::path{};
    Paint(*icon_, path.empty() ? Transparent : IconColour(content_.kind));
    if (!path.empty())
        Require<SpriteVisualComponent>(*icon_).SetImage(visuals_->RegisterImage(path));

    StyleText(*title_, content_.title, ui_fonts::Text(content_.title), TitleSize, 900, ui_colors::theme::Ink,
              TextAlignment::Leading);
    const auto message = ui_text::WrapText(content_.message, MessageSize, BodyWidth);
    StyleText(*message_, message, ui_fonts::Text(message), MessageSize, 500, ui_colors::theme::InkSoft,
              TextAlignment::Leading);
    StyleText(*detail_, ui_text::WrapText(content_.detail, DetailSize, DetailTextWidth),
              TextFont{TextFontSource::System, L"Consolas"}, DetailSize, 500, ui_colors::theme::Ink,
              TextAlignment::Leading);
    message_->SetVisible(!content_.message.empty());
    detailBox_->SetVisible(!content_.detail.empty());

    EnsureButtons(content_.buttons.size());
    for (std::size_t i = 0; i < buttons_.size(); ++i)
    {
        const bool used = i < content_.buttons.size();
        buttons_[i].node->SetVisible(used);
        if (!used)
            continue;
        const auto &button = content_.buttons[i];
        const auto ink = button.primary ? ui_colors::White
                         : button.left  ? ui_colors::theme::Danger
                                        : ui_colors::theme::BrandText;
        StyleText(*buttons_[i].label, button.label, ui_fonts::Text(button.label), ButtonTextSize, 800, ink,
                  TextAlignment::Center);
        PaintButton(i);
    }
    scrim_->SetVisible(true);
    Layout();
}

void UiAlert::Hide() noexcept
{
    if (scrim_ != nullptr)
        scrim_->SetVisible(false);
    hovered_.reset();
    pressed_.reset();
}

bool UiAlert::IsOpen() const noexcept
{
    return scrim_ != nullptr && scrim_->IsVisible();
}

Visual2DNode *UiAlert::ButtonNode(const std::size_t index) const noexcept
{
    return index < buttons_.size() && index < content_.buttons.size() ? buttons_[index].node : nullptr;
}

float UiAlert::TextHeight(Visual2DNode &node, const float estimate) const
{
    const auto measured = Require<TextVisualComponent>(node).MeasuredSize();
    return measured ? measured->height : estimate;
}

float UiAlert::TextWidth(Visual2DNode &node, const float estimate) const
{
    const auto measured = Require<TextVisualComponent>(node).MeasuredSize();
    return measured ? measured->width : estimate;
}

std::vector<float> UiAlert::Measurements() const
{
    std::vector<float> sizes;
    const auto &message = Require<TextVisualComponent>(*message_).Text();
    const auto &detail = Require<TextVisualComponent>(*detail_).Text();
    sizes.push_back(TextHeight(*message_, LineCount(message) * MessageLine));
    sizes.push_back(TextHeight(*detail_, LineCount(detail) * DetailLine));
    for (std::size_t i = 0; i < content_.buttons.size(); ++i)
        sizes.push_back(TextWidth(*buttons_[i].label, ui_text::EstimateWidth(content_.buttons[i].label, ButtonTextSize)));
    return sizes;
}

// Top to bottom: head (icon and title), message, detail, a rule, then the buttons. The panel's
// height follows its content and the panel stays centred on the Canvas.
void UiAlert::Layout()
{
    laidOut_ = Measurements();
    const float messageHeight = laidOut_[0], detailHeight = laidOut_[1];
    float y = PaddingTop + HeadHeight;
    float messageTop = 0, detailTop = 0, viewHeight = 0;
    if (!content_.message.empty())
    {
        messageTop = y + Gap;
        y = messageTop + messageHeight;
    }
    if (!content_.detail.empty())
    {
        detailTop = y + Gap;
        viewHeight = std::min(detailHeight, DetailMaximum - DetailPaddingY * 2);
        y = detailTop + viewHeight + DetailPaddingY * 2;
    }
    const float ruleTop = y + Gap + FooterGap, buttonTop = ruleTop + 1 + FooterPadding;
    const float height = buttonTop + ButtonHeight + PaddingBottom;

    panel_->SetBounds({-Width * .5F - ScrimOrigin, -height * .5F - ScrimOrigin, Width, height});
    band_->SetBounds(TopLeft({0, 0, Width, Band}, height));
    icon_->SetBounds(TopLeft({PaddingSide, PaddingTop, IconSize, IconSize}, height));
    title_->SetBounds(TopLeft({TextLeft, PaddingTop, BodyWidth, HeadHeight}, height));
    message_->SetBounds(TopLeft({TextLeft, messageTop, BodyWidth, messageHeight}, height));

    const float boxHeight = viewHeight + DetailPaddingY * 2;
    detailBox_->SetBounds(TopLeft({TextLeft, detailTop, BodyWidth, boxHeight}, height));
    detailLine_->SetBounds({0, 0, DetailStripe, boxHeight});
    detailView_->SetBounds({DetailPaddingX, DetailPaddingY, DetailTextWidth, viewHeight});
    detailView_->SetClipRect({0, 0, DetailTextWidth, viewHeight});
    detailOverflow_ = std::max(0.0F, detailHeight - viewHeight);
    detailScroll_ = std::clamp(detailScroll_, 0.0F, detailOverflow_);
    detail_->SetBounds({0, viewHeight - detailHeight + detailScroll_, DetailTextWidth, detailHeight});
    rule_->SetBounds(TopLeft({PaddingSide, ruleTop, Width - PaddingSide * 2, 1}, height));

    // Buttons keep their order: left ones from the left edge, the rest ending at the right edge.
    float left = PaddingSide, right = Width - PaddingSide;
    for (std::size_t i = content_.buttons.size(); i-- > 0;)
    {
        if (content_.buttons[i].left)
            continue;
        const float width = ButtonWidth(content_.buttons[i], laidOut_[2 + i]);
        right -= width;
        buttons_[i].node->SetBounds(TopLeft({right, buttonTop, width, ButtonHeight}, height));
        right -= ButtonGap;
    }
    for (std::size_t i = 0; i < content_.buttons.size(); ++i)
    {
        if (!content_.buttons[i].left)
            continue;
        const float width = ButtonWidth(content_.buttons[i], laidOut_[2 + i]);
        buttons_[i].node->SetBounds(TopLeft({left, buttonTop, width, ButtonHeight}, height));
        left += width + ButtonGap;
    }
    for (std::size_t i = 0; i < content_.buttons.size(); ++i)
    {
        auto &button = buttons_[i];
        const float width = button.node->NodeSize().width;
        const bool primary = content_.buttons[i].primary;
        button.fill->SetBounds(primary ? Rect{Cut, 0, width - Cut * 2, ButtonHeight} : Rect{0, 0, width, ButtonHeight});
        button.line->SetBounds({0, 0, width, ButtonLine});
        button.line->SetVisible(!primary);
        button.caps[0]->SetBounds({0, 0, Cut, ButtonHeight});
        button.caps[1]->SetBounds({width - Cut, 0, Cut, ButtonHeight});
        for (std::size_t end = 0; end < 2; ++end)
        {
            button.caps[end]->SetVisible(primary);
            PlaceCap(*button.caps[end], *button.capFills[end], ButtonHeight, end == 0);
        }
        button.label->SetBounds({0, 0, width, ButtonHeight});
    }
}

void UiAlert::PaintButton(const std::size_t index)
{
    const auto &content = content_.buttons[index];
    auto &button = buttons_[index];
    const bool pressed = pressed_ == index, hovered = hovered_ == index;
    if (content.primary)
    {
        const auto colour = pressed ? PrimaryPressed : hovered ? PrimaryHover : PrimaryColour;
        Paint(*button.fill, colour);
        for (auto *cap : button.capFills)
            Paint(*cap, colour);
        return;
    }
    const auto lineColour = content.left ? ui_colors::theme::Danger : ui_colors::theme::Brand;
    const auto hoverColour = content.left ? ui_colors::theme::DangerSurface : PlainHover;
    Paint(*button.fill, pressed ? PlainPressed : hovered ? hoverColour : PlainColour);
    Paint(*button.line, hovered || pressed ? lineColour : (content.left ? DangerLine : PlainLine));
}

void UiAlert::Pointer(const std::size_t index, const PointerEvent &event)
{
    if (index >= content_.buttons.size() || !IsOpen())
        return;
    switch (event.type)
    {
    case PointerEventType::Enter: hovered_ = index; break;
    case PointerEventType::Leave: hovered_.reset(); pressed_.reset(); break;
    case PointerEventType::Press: pressed_ = index; break;
    case PointerEventType::Release: pressed_.reset(); break;
    case PointerEventType::Click: Choose(index); return;
    default: return;
    }
    PaintButton(index);
}

void UiAlert::ScrollDetail(const float wheel)
{
    if (detailOverflow_ <= 0)
        return;
    detailScroll_ = std::clamp(detailScroll_ - wheel * DetailWheelStep, 0.0F, detailOverflow_);
    Layout();
}

void UiAlert::Choose(const std::size_t index) noexcept
{
    chosen_ = index;
    Hide();
}

std::optional<std::size_t> UiAlert::Update(const InputState &input)
{
    if (chosen_)
        return std::exchange(chosen_, std::nullopt);
    if (!IsOpen())
        return {};
    // Measured text replaces the estimates once the renderer has laid it out.
    if (Measurements() != laidOut_)
        Layout();
    // The key that led to the alert must not also answer it.
    if (!keysReady_)
    {
        keysReady_ = true;
        return {};
    }
    const bool control = input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (control && input.WasKeyPressed('C'))
        CopyToClipboard(content_.detail.empty() ? content_.message : content_.detail);
    const auto &buttons = content_.buttons;
    if (input.WasKeyPressed(VK_RETURN))
    {
        const auto primary = std::ranges::find_if(buttons, &AlertButton::primary);
        if (primary != buttons.end())
            Choose(static_cast<std::size_t>(primary - buttons.begin()));
    }
    else if (input.WasKeyPressed(VK_ESCAPE))
    {
        const auto cancel = std::ranges::find_if(buttons, &AlertButton::cancel);
        if (cancel != buttons.end())
            Choose(static_cast<std::size_t>(cancel - buttons.begin()));
        else if (buttons.size() == 1)
            Choose(0);
    }
    return std::exchange(chosen_, std::nullopt);
}
