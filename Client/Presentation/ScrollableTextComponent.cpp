#include "ScrollableTextComponent.h"
#include "UiColors.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace PhantomCore;

namespace
{
    Visual2DNode &Bar(Visual2DNode &parent, const char *name, Color color)
    {
        auto &node = parent.CreateChild(name);
        auto &sprite = node.AddComponent<SpriteVisualComponent>();
        VisualStyle style;
        style.normal = style.hovered = style.pressed = style.disabled = color;
        sprite.SetStyle(style);
        sprite.SetCornerRadius(3);
        return node;
    }
} // namespace

void ScrollableTextComponent::EnsureChildren()
{
    if (visual_)
        return;
    viewport_ = &Owner().CreateChild("TextViewport");
    content_ = &viewport_->CreateChild("TextContent");
    visual_ = &content_->AddComponent<TextVisualComponent>(text_);
    verticalTrack_ = &Bar(Owner(), "TextScroll.VerticalTrack", ui_colors::sky::PaleBorder);
    verticalThumb_ = &Bar(Owner(), "TextScroll.VerticalThumb", ui_colors::sky::Accent);
    horizontalTrack_ = &Bar(Owner(), "TextScroll.HorizontalTrack", ui_colors::sky::PaleBorder);
    horizontalThumb_ = &Bar(Owner(), "TextScroll.HorizontalThumb", ui_colors::sky::Accent);
}

void ScrollableTextComponent::SetText(std::wstring text)
{
    EnsureChildren();
    if (text_ == text)
        return;
    text_ = std::move(text);
    lines_ = text_.empty() ? 0 : 1 + static_cast<std::size_t>(std::count(text_.begin(), text_.end(), L'\n'));
    offset_ = {};
    visual_->SetText(text_);
    UpdateLayout();
}

void ScrollableTextComponent::SetStyle(TextFont font, float fontSize, Color color)
{
    EnsureChildren();
    visual_->SetFont(std::move(font));
    visual_->SetFontSize(fontSize);
    visual_->SetTextColor(color);
    UpdateLayout();
}

void ScrollableTextComponent::SetFont(TextFont font)
{
    EnsureChildren();
    visual_->SetFont(std::move(font));
    UpdateLayout();
}

void ScrollableTextComponent::Update(double)
{
    EnsureChildren();
    if (!Owner().IsPressed())
        dragAxis_ = DragAxis::None;
    UpdateLayout();
}

void ScrollableTextComponent::UpdateLayout()
{
    const auto size = Owner().NodeSize();
    // Measurement is published by the renderer. Use a line-height estimate
    // only until the complete Unicode layout has been measured.
    Size measured{0, static_cast<float>(lines_) * visual_->FontSize() * 1.5F};
    if (const auto value = visual_->MeasuredSize();
        value && std::isfinite(value->width) && std::isfinite(value->height) && value->width >= 0 && value->height >= 0)
        measured = *value;
    if (size.width == previousSize_.width && size.height == previousSize_.height &&
        measured.width == previousContentSize_.width && measured.height == previousContentSize_.height &&
        offset_.x == previousOffset_.x && offset_.y == previousOffset_.y)
        return;
    Owner().SetClipRect({0, 0, size.width, size.height});
    const float bar = std::min(6.0F, std::min(size.width, size.height) * .25F);
    bool vertical = measured.height > size.height;
    bool horizontal = measured.width > size.width - (vertical ? bar : 0);
    vertical = vertical || measured.height > size.height - (horizontal ? bar : 0);
    horizontal = horizontal || measured.width > size.width - (vertical ? bar : 0);
    const float width = std::max(0.0F, size.width - (vertical ? bar : 0));
    const float height = std::max(0.0F, size.height - (horizontal ? bar : 0));
    range_ = {std::max(measured.width - width, 0.0F), std::max(measured.height - height, 0.0F)};
    offset_.x = std::clamp(offset_.x, 0.0F, range_.width);
    offset_.y = std::clamp(offset_.y, 0.0F, range_.height);
    viewport_->SetBounds({0, horizontal ? bar : 0, width, height});
    viewport_->SetClipRect({0, 0, width, height});
    content_->SetBounds(
        {-offset_.x, height - measured.height + offset_.y, std::max(width, measured.width), measured.height});

    verticalTrack_->SetVisible(vertical && bar > 0);
    verticalThumb_->SetVisible(vertical && bar > 0);
    horizontalTrack_->SetVisible(horizontal && bar > 0);
    horizontalThumb_->SetVisible(horizontal && bar > 0);
    verticalTrack_->SetBounds({width, horizontal ? bar : 0, bar, height});
    horizontalTrack_->SetBounds({0, 0, width, bar});
    const float thumbHeight =
        std::clamp(height * height / std::max(measured.height, 1.0F), std::min(10.0F, height), height);
    const float thumbWidth = std::clamp(width * width / std::max(measured.width, 1.0F), std::min(10.0F, width), width);
    const float y = horizontal ? bar : 0;
    verticalThumb_->SetBounds(
        {width, y + height - thumbHeight - (range_.height > 0 ? offset_.y / range_.height * (height - thumbHeight) : 0),
         bar, thumbHeight});
    horizontalThumb_->SetBounds(
        {range_.width > 0 ? offset_.x / range_.width * (width - thumbWidth) : 0, 0, thumbWidth, bar});
    previousSize_ = size;
    previousContentSize_ = measured;
    previousOffset_ = offset_;
}

bool ScrollableTextComponent::Scroll(float wheelDelta, bool horizontal)
{
    EnsureChildren();
    UpdateLayout();
    if (range_.width <= 0 && range_.height <= 0)
        return false;
    if (!std::isfinite(wheelDelta))
        return true;
    const float movement = -wheelDelta * visual_->FontSize() * 3;
    if (horizontal || range_.height <= 0)
        offset_.x += movement;
    else
        offset_.y += movement;
    UpdateLayout();
    return true;
}

bool ScrollableTextComponent::HitTest(Point position) const noexcept
{
    const auto size = Owner().NodeSize();
    return Rect{0, 0, size.width, size.height}.Contains(position);
}

void ScrollableTextComponent::DragTo(Point position)
{
    if (dragAxis_ == DragAxis::Vertical)
    {
        const auto track = verticalTrack_->Bounds();
        const auto thumb = verticalThumb_->Bounds();
        const float travel = track.height - thumb.height;
        if (travel > 0)
            offset_.y = (track.y + track.height - (position.y - dragGrab_ + thumb.height)) / travel * range_.height;
    }
    else if (dragAxis_ == DragAxis::Horizontal)
    {
        const auto track = horizontalTrack_->Bounds();
        const auto thumb = horizontalThumb_->Bounds();
        const float travel = track.width - thumb.width;
        if (travel > 0)
            offset_.x = (position.x - dragGrab_ - track.x) / travel * range_.width;
    }
    UpdateLayout();
}

void ScrollableTextComponent::OnPointerEvent(const PointerEvent &event, std::vector<Action> &actions)
{
    EnsureChildren();
    if (event.type == PointerEventType::Press)
    {
        UpdateLayout();
        suppressClick_ = false;
        dragAxis_ = DragAxis::None;
        if (verticalTrack_->IsVisible() && verticalTrack_->Bounds().Contains(event.localPosition))
        {
            dragAxis_ = DragAxis::Vertical;
            const auto thumb = verticalThumb_->Bounds();
            dragGrab_ = thumb.Contains(event.localPosition) ? event.localPosition.y - thumb.y : thumb.height / 2;
        }
        else if (horizontalTrack_->IsVisible() && horizontalTrack_->Bounds().Contains(event.localPosition))
        {
            dragAxis_ = DragAxis::Horizontal;
            const auto thumb = horizontalThumb_->Bounds();
            dragGrab_ = thumb.Contains(event.localPosition) ? event.localPosition.x - thumb.x : thumb.width / 2;
        }
        if (dragAxis_ != DragAxis::None)
        {
            suppressClick_ = true;
            DragTo(event.localPosition);
        }
    }
    else if (event.type == PointerEventType::Move && dragAxis_ != DragAxis::None && Owner().IsPressed())
        DragTo(event.localPosition);
    else if (event.type == PointerEventType::Release)
        dragAxis_ = DragAxis::None;
    else if (event.type == PointerEventType::Click && !suppressClick_ && clickTarget_ != 0)
        actions.push_back({ActionType::Clicked, clickTarget_, 0, 0, event.timestampTicks});
}
