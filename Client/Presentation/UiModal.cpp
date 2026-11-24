#include "Presentation/UiModal.h"
#include "App/AssetPaths.h"
#include "Presentation/NativeInputFonts.h"
#include "Presentation/UiFonts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;
using namespace ui_modal;

namespace
{
    constexpr float TitleTop = 28, TitleHeight = 33, CloseSize = 32, Cut = 14, ButtonPadding = 16, PrimaryPadding = 26;
    constexpr float BandHeight = 4, SparkleWidth = 22;
    constexpr Color Transparent{1, 1, 1, 0};
    constexpr auto Surface = ui_colors::Rgb(0xF0F8FF, .98F), Scrim = ui_colors::Rgb(0x142F5E, .42F);
    constexpr auto FooterRule = ui_colors::Rgb(0xD3E2F8), Sparkle = ui_colors::Rgb(0x5B8FE6);
    constexpr auto PrimaryHover = ui_colors::Rgb(0x2A5DB4), PrimaryPressed = ui_colors::theme::BrandDeep;
    constexpr auto SecondaryFill = ui_colors::Rgb(0xFFFFFF, .72F), HoverFill = ui_colors::Rgb(0xFFFFFF, .8F);
    constexpr auto PressedFill = ui_colors::Rgb(0xE6EEFC), Danger = ui_colors::theme::Danger;
    constexpr auto FieldFill = ui_colors::Rgb(0xFFFFFF, .74F), FieldDisabled = ui_colors::Rgb(0xFFFFFF, .4F);
    constexpr auto SwitchOff = ui_colors::Rgb(0x7F97B0);
    // Field text sizes; a native edit over a field uses the same size.
    constexpr float FieldTextSize = 14, FieldNumberSize = 15;
    // 16:9 is the reference; a narrower canvas scales the modal like the song select board.
    constexpr float ReferenceWidth = 1280;

    template <class T> T &Require(Visual2DNode &node)
    {
        auto *component = node.GetComponent<T>();
        if (component == nullptr)
            throw std::logic_error("A modal widget is missing a required component.");
        return *component;
    }
    void Paint(Visual2DNode &node, const Color normal, const Color hovered, const Color pressed, const Color disabled)
    {
        VisualStyle style{};
        style.normal = normal;
        style.hovered = hovered;
        style.pressed = pressed;
        style.disabled = disabled;
        Require<SpriteVisualComponent>(node).SetStyle(style);
    }
    void Paint(Visual2DNode &node, const Color color)
    {
        Paint(node, color, color, color, color);
    }
    void StyleText(TextVisualComponent &text, const float size, const std::uint16_t weight, const Color color,
                   const TextAlignment alignment = TextAlignment::Leading)
    {
        text.SetFont(ui_fonts::Text(text.Text()));
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(color);
        text.SetHorizontalAlignment(alignment);
    }
    // One slanted end of the primary button: a large square turned so one edge runs along the slant, clipped by
    // the end's box. The left end fills below its diagonal, the right end above it.
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
    Visual2DNode &Fill(Visual2DNode &parent, std::string name, const Color color)
    {
        auto &node = CreatePanel(parent, {}, std::move(name));
        Paint(node, color);
        return node;
    }
} // namespace

namespace ui_modal
{
    void Place(Visual2DNode &node, const Rect topLeftBounds, const float parentHeight)
    {
        node.SetBounds(TopLeft(topLeftBounds, parentHeight));
    }

    Visual2DNode &CreateText(Visual2DNode &parent, std::string name, const float size, const std::uint16_t weight,
                             const Color color)
    {
        auto &label = CreateLabel(parent, {}, L"", std::move(name));
        StyleText(Require<TextVisualComponent>(label), size, weight, color);
        return label;
    }

    void SetText(Visual2DNode &node, const std::wstring_view text)
    {
        auto &component = Require<TextVisualComponent>(node);
        component.SetFont(ui_fonts::Text(text));
        component.SetText(std::wstring(text));
    }

    float EstimateWidth(const std::wstring_view text, const float size) noexcept
    {
        float width = 0;
        for (const wchar_t character : text)
            width += character >= 0x1100 ? size : size * (character == L' ' ? .3F : .55F);
        return width;
    }

    Visual2DNode &CreateField(Visual2DNode &parent, std::string name, const bool number)
    {
        auto &field = CreateButton(parent, {}, L"", std::move(name));
        auto &text = Require<TextVisualComponent>(field);
        if (number)
        {
            auto face = ui_fonts::Number(500, false);
            text.SetFont(std::move(face.font));
            text.SetFontWeight(face.weight);
            text.SetFontSize(FieldNumberSize);
            text.SetTextColor(Ink);
            text.SetHorizontalAlignment(TextAlignment::Leading);
        }
        else
            StyleText(text, FieldTextSize, 400, Ink);
        static_cast<void>(Fill(field, "Line", FieldLine));
        StyleField(field, false, true);
        return field;
    }

    void SetFieldText(Visual2DNode &field, const std::wstring_view text, const bool number)
    {
        if (!number)
        {
            SetText(field, text);
            return;
        }
        Require<TextVisualComponent>(field).SetText(std::wstring(text));
    }

    void PlaceField(Visual2DNode &field, const Rect topLeftBounds, const float parentHeight)
    {
        Place(field, topLeftBounds, parentHeight);
        field.SetClipRect({0, 0, topLeftBounds.width, topLeftBounds.height});
        Require<TextVisualComponent>(field).SetContentBounds(
            {12, 0, std::max(topLeftBounds.width - 24, 1.0F), topLeftBounds.height});
        for (const auto &child : field.Children())
            if (child->Name() == "Line")
                child->SetBounds({0, 0, topLeftBounds.width, 2});
    }

    void StyleField(Visual2DNode &field, const bool focused, const bool enabled)
    {
        Paint(field, enabled ? FieldFill : FieldDisabled, enabled ? ui_colors::White : FieldDisabled,
              PressedFill, FieldDisabled);
        field.SetEnabled(enabled);
        for (const auto &child : field.Children())
            if (child->Name() == "Line")
                Paint(*child, focused ? Brand : FieldLine);
    }

    Visual2DNode &CreateSwitch(Visual2DNode &parent, std::string name)
    {
        auto &node = CreateButton(parent, {}, L"", std::move(name));
        Require<SpriteVisualComponent>(node).SetCornerRadius(2);
        auto &knob = Fill(node, "Knob", ui_colors::White);
        Require<SpriteVisualComponent>(knob).SetCornerRadius(2);
        SetSwitch(node, false, true);
        return node;
    }

    void SetSwitch(Visual2DNode &node, const bool on, const bool enabled)
    {
        const auto track = on ? Brand : SwitchOff;
        const Color dimmed{track.red, track.green, track.blue, .4F};
        Paint(node, enabled ? track : dimmed);
        node.SetEnabled(enabled);
        for (const auto &child : node.Children())
            if (child->Name() == "Knob")
            {
                child->SetBounds({on ? 19.0F : 3.0F, 3, 18, 18});
                Paint(*child, enabled ? ui_colors::White : ui_colors::Rgb(0xFFFFFF, .7F));
            }
    }

    Visual2DNode &CreateTab(Visual2DNode &parent, std::string name, const std::wstring_view text, const float size)
    {
        auto &node = CreateButton(parent, {}, std::wstring(text), std::move(name));
        Paint(node, Transparent);
        StyleText(Require<TextVisualComponent>(node), size, 800, Caption, TextAlignment::Center);
        static_cast<void>(Fill(node, "Underline", Brand));
        SetTabSelected(node, false);
        return node;
    }

    void SetTabSelected(Visual2DNode &node, const bool selected)
    {
        Require<TextVisualComponent>(node).SetTextColor(selected ? BrandText : Caption);
        const auto size = node.NodeSize();
        for (const auto &child : node.Children())
            if (child->Name() == "Underline")
            {
                child->SetVisible(selected);
                child->SetBounds({0, 0, size.width, 3});
            }
    }

    Visual2DNode &CreateRule(Visual2DNode &parent, std::string name)
    {
        return Fill(parent, std::move(name), Rule);
    }

    Visual2DNode &CreateIcon(Visual2DNode &parent, std::string name, ScreenVisual2DManager *visuals,
                             const wchar_t *key, const Color tint)
    {
        auto &icon = CreateSprite(parent, {}, visuals ? visuals->RegisterImage(Common(key)) : ImageHandle{},
                                  std::move(name));
        // A style would replace the sprite's image, so the icon is only tinted. Without an image the sprite
        // would draw a tinted square, so it stays transparent.
        if (visuals)
            Require<SpriteVisualComponent>(icon).SetTint(tint);
        else
            Paint(icon, Transparent);
        return icon;
    }

    void PlaceTextInput(ITextInput &input, const Visual2DNode &field, const Visual2DCanvas &canvas,
                        const std::uint32_t windowHeight, const float scale, const bool number)
    {
        const auto logical = canvas.LogicalSize();
        if (windowHeight == 0 || logical.height <= 0)
            return;
        const float pixels = static_cast<float>(windowHeight) / logical.height;
        const auto bounds = field.BoundsInCanvas();
        const float size = number ? FieldNumberSize : FieldTextSize;
        const auto font = static_cast<std::uint32_t>(std::max(1.0F, std::round(size * pixels * scale)));
        const float editHeight = std::min(static_cast<float>(font) + 4, bounds.height * pixels);
        const float inset = 12 * scale;
        input.SetBounds({static_cast<std::int32_t>(std::round((bounds.x + inset + logical.width / 2) * pixels)),
                         static_cast<std::int32_t>(
                             std::round((logical.height / 2 - bounds.y - bounds.height / 2) * pixels - editHeight / 2)),
                         static_cast<std::uint32_t>(std::max(1.0F, std::round((bounds.width - inset * 2) * pixels))),
                         static_cast<std::uint32_t>(std::max(1.0F, std::round(editHeight)))});
        input.SetFont(number ? native_input_fonts::Number() : native_input_fonts::Text, font);
    }
} // namespace ui_modal

void UiModal::Create(Visual2DCanvas &canvas, Setup setup)
{
    canvas_ = &canvas;
    width_ = setup.width;
    const auto &prefix = setup.prefix;
    overlay_ = &CreatePanel(canvas.Root(), {}, prefix + ".Overlay");
    overlay_->SetZIndex(1000);
    Paint(*overlay_, Scrim);
    // The scrim takes every pointer so nothing behind the modal reacts.
    overlay_->AddComponent<RectangleCollider2DComponent>();
    overlay_->AddComponent<PointerReceiverComponent>();
    panel_ = &Fill(*overlay_, prefix + ".Panel", Surface);
    band_ = &Fill(*panel_, "Band", Brand);
    sparkle_ = &CreateText(*panel_, "Sparkle", 18, 900, Sparkle);
    SetText(*sparkle_, L"✦");
    title_ = &CreateText(*panel_, prefix + ".Title", 26, 900, Ink);
    if (setup.close)
    {
        close_ = &CreateButton(*panel_, {}, L"", prefix + ".Close");
        Paint(*close_, Transparent, HoverFill, PressedFill, Transparent);
        auto &icon = CreateIcon(*close_, "Icon", setup.visuals, L"Icons.x", BrandText);
        icon.SetBounds({7, 7, 18, 18});
    }
    body_ = &panel_->CreateChild(prefix + ".Body");
    status_ = &panel_->CreateChild(prefix + ".Status");
    statusBar_ = &Fill(*status_, "Bar", Danger);
    statusIcon_ = &CreateIcon(*status_, "Icon", setup.visuals, L"Icons.alert", Danger);
    statusText_ = &CreateText(*status_, prefix + ".StatusText", 13, 600, Danger);
    status_->SetVisible(false);
    rule_ = &Fill(*panel_, "FooterRule", FooterRule);

    reset_ = &CreateButton(*panel_, {}, L"", prefix + ".Reset");
    Paint(*reset_, Transparent, ui_colors::Rgb(0x1F4C99, .08F), ui_colors::Rgb(0x1F4C99, .16F), Transparent);
    resetIcon_ = &CreateIcon(*reset_, "Icon", setup.visuals, L"Icons.retry", BrandText);
    resetText_ = &CreateText(*reset_, "Text", 14, 800, BrandText);
    if (setup.cancel)
    {
        cancel_ = &CreateButton(*panel_, {}, L"", prefix + ".Cancel");
        Paint(*cancel_, SecondaryFill, ui_colors::White, PressedFill, SecondaryFill);
        StyleText(Require<TextVisualComponent>(*cancel_), 14, 800, BrandText, TextAlignment::Center);
        static_cast<void>(Fill(*cancel_, "Line", FieldLine));
    }
    // The primary action is a cobalt parallelogram: a middle fill and two clipped slanted ends that follow the
    // button's own pointer state.
    primary_ = &CreateButton(*panel_, {}, L"", prefix + ".Apply");
    Paint(*primary_, Transparent);
    auto &middle = Fill(*primary_, "Fill", Brand);
    for (std::size_t end = 0; end < 2; ++end)
    {
        primaryCaps_[end] = &CreatePanel(*primary_, {}, end == 0 ? "LeftEnd" : "RightEnd");
        Paint(*primaryCaps_[end], Transparent);
        primaryCapFills_[end] = &Fill(*primaryCaps_[end], "Fill", Brand);
    }
    primaryText_ = &CreateText(*primary_, "Text", 14, 800, ui_colors::White);
    Require<TextVisualComponent>(*primaryText_).SetHorizontalAlignment(TextAlignment::Center);
    auto paintPrimary = [this, middleFill = &middle](const Color color) {
        Paint(*middleFill, color);
        for (auto *capFill : primaryCapFills_)
            Paint(*capFill, color);
    };
    Require<PointerReceiverComponent>(*primary_)
        .SetHandler([paintPrimary](Visual2DNode &, const PointerEvent &event, std::vector<Action> &) {
            switch (event.type)
            {
            case PointerEventType::Enter:
            case PointerEventType::Release:
                paintPrimary(PrimaryHover);
                break;
            case PointerEventType::Leave:
                paintPrimary(Brand);
                break;
            case PointerEventType::Press:
                paintPrimary(PrimaryPressed);
                break;
            default:
                break;
            }
        });
    overlay_->SetVisible(false);
}

void UiModal::Forget() noexcept
{
    canvas_ = nullptr;
    overlay_ = panel_ = band_ = sparkle_ = title_ = close_ = body_ = nullptr;
    status_ = statusBar_ = statusIcon_ = statusText_ = rule_ = nullptr;
    reset_ = resetIcon_ = resetText_ = cancel_ = primary_ = primaryText_ = nullptr;
    primaryCaps_ = primaryCapFills_ = {};
}

void UiModal::SetTexts(const std::wstring_view title, const std::wstring_view reset, const std::wstring_view cancel,
                       const std::wstring_view primary)
{
    if (!panel_)
        return;
    SetText(*title_, title);
    SetText(*resetText_, reset);
    if (cancel_)
        SetText(*cancel_, cancel);
    SetText(*primaryText_, primary);
}

void UiModal::SetStatus(const std::wstring_view text)
{
    if (!status_)
        return;
    statusVisible_ = !text.empty();
    SetText(*statusText_, text);
    status_->SetVisible(statusVisible_);
}

Visual2DNode &UiModal::Body() const
{
    if (!body_)
        throw std::logic_error("The modal has not been created.");
    return *body_;
}

void UiModal::Layout(const float bodyHeight)
{
    if (!canvas_ || !panel_)
        return;
    bodyHeight_ = bodyHeight;
    const auto logical = canvas_->LogicalSize();
    scale_ = std::clamp(logical.width / ReferenceWidth, 0.1F, 1.0F);
    const float statusSpace = statusVisible_ ? StatusGap + StatusHeight : 0.0F;
    const float height = BodyTop + bodyHeight + statusSpace + BodyBottomSpace;
    overlay_->SetBounds({-logical.width / 2, -logical.height / 2, logical.width, logical.height});
    panel_->SetBounds({(logical.width - width_ * scale_) / 2, (logical.height - height * scale_) / 2, width_, height});
    panel_->Transform().SetScale(scale_, scale_, 1.0F);
    Place(*band_, {0, 0, width_, BandHeight}, height);
    Place(*sparkle_, {Padding, TitleTop + 2, SparkleWidth, 24}, height);
    Place(*title_, {Padding + SparkleWidth, TitleTop, width_ - Padding * 2 - SparkleWidth - CloseSize, TitleHeight}, height);
    if (close_)
        Place(*close_, {width_ - Padding - CloseSize, TitleTop, CloseSize, CloseSize}, height);
    Place(*body_, {Padding, BodyTop, BodyWidth(), bodyHeight}, height);
    const float statusTop = BodyTop + bodyHeight + StatusGap;
    Place(*status_, {Padding, statusTop, BodyWidth(), StatusHeight}, height);
    statusBar_->SetBounds({0, 0, 3, StatusHeight});
    statusIcon_->SetBounds({12, (StatusHeight - 18) / 2, 18, 18});
    statusText_->SetBounds({38, 0, BodyWidth() - 38, StatusHeight});
    LayoutFooter(height);
}

void UiModal::LayoutFooter(const float panelHeight)
{
    const float ruleTop = panelHeight - BodyBottomSpace + FooterGap;
    Place(*rule_, {Padding, ruleTop, BodyWidth(), 1}, panelHeight);
    const float top = ruleTop + 1 + FooterPadding;
    const float resetWidth = 17 + 18 + 8 + EstimateWidth(Require<TextVisualComponent>(*resetText_).Text(), 14) + 17;
    Place(*reset_, {Padding, top, resetWidth, ButtonHeight}, panelHeight);
    resetIcon_->SetBounds({17, (ButtonHeight - 18) / 2, 18, 18});
    resetText_->SetBounds({43, 0, resetWidth - 43, ButtonHeight});
    float right = width_ - Padding;
    LayoutPrimary(right, top);
    right -= primary_->NodeSize().width + 8;
    if (cancel_)
    {
        const float width = EstimateWidth(Require<TextVisualComponent>(*cancel_).Text(), 14) + ButtonPadding * 2;
        Place(*cancel_, {right - width, top, width, ButtonHeight}, panelHeight);
        for (const auto &child : cancel_->Children())
            child->SetBounds({0, 0, width, 2});
    }
}

void UiModal::LayoutPrimary(const float right, const float top)
{
    const float panelHeight = panel_->NodeSize().height;
    const float width = EstimateWidth(Require<TextVisualComponent>(*primaryText_).Text(), 14) + PrimaryPadding * 2;
    Place(*primary_, {right - width, top, width, ButtonHeight}, panelHeight);
    for (const auto &child : primary_->Children())
        if (child->Name() == "Fill")
            child->SetBounds({Cut, 0, width - Cut * 2, ButtonHeight});
    primaryCaps_[0]->SetBounds({0, 0, Cut, ButtonHeight});
    primaryCaps_[1]->SetBounds({width - Cut, 0, Cut, ButtonHeight});
    for (std::size_t end = 0; end < 2; ++end)
        PlaceCap(*primaryCaps_[end], *primaryCapFills_[end], ButtonHeight, end == 0);
    primaryText_->SetBounds({0, 0, width, ButtonHeight});
}

void UiModal::SetVisible(const bool visible) noexcept
{
    if (overlay_)
        overlay_->SetVisible(visible);
}

bool UiModal::Owns(const NodeId source) const noexcept
{
    if (!canvas_ || !IsVisible())
        return false;
    for (const auto *node = canvas_->FindNode(source); node; node = node->Parent())
    {
        if (!node->IsVisible() || !node->IsEnabled())
            return false;
        if (node == panel_)
            return true;
    }
    return false;
}
