#include "UiScrollbar.h"

#include <algorithm>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    void Fill(Visual2DNode &node, const Color colour)
    {
        auto *sprite = node.GetComponent<SpriteVisualComponent>();
        if (sprite == nullptr)
            throw std::logic_error("A scrollbar part is missing its sprite.");
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        sprite->SetStyle(style);
    }
} // namespace

UiScrollbar::~UiScrollbar()
{
    if (self_)
        *self_ = nullptr;
}

void UiScrollbar::Attach(Visual2DNode &parent, std::string name, const Axis axis)
{
    axis_ = axis;
    if (self_)
        *self_ = nullptr;
    self_ = std::make_shared<UiScrollbar *>(this);
    rail_ = &parent.CreateChild(std::move(name));
    rail_->AddComponent<RectangleCollider2DComponent>();
    rail_->AddComponent<PointerReceiverComponent>(
        [self = self_](Visual2DNode &, const PointerEvent &event, std::vector<Action> &) {
            if (*self != nullptr)
                (*self)->OnPointer(event);
        });
    track_ = &CreateSprite(*rail_, {}, {}, "Rail");
    Fill(*track_, Rail);
    thumb_ = &CreateSprite(*rail_, {}, {}, "Thumb");
    thumb_->SetZIndex(1);
    painted_ = dragging_ = false;
    Paint(false);
    rail_->SetVisible(false);
}

void UiScrollbar::FollowPointerEvents(std::function<void(float)> scrolled, std::function<void(float)> wheel)
{
    scrolled_ = std::move(scrolled);
    wheel_ = std::move(wheel);
}

void UiScrollbar::Detach() noexcept
{
    if (self_)
        *self_ = nullptr;
    rail_ = track_ = thumb_ = nullptr;
    dragging_ = painted_ = false;
}

void UiScrollbar::SetZIndex(const std::int32_t zIndex) noexcept
{
    if (rail_ != nullptr)
        rail_->SetZIndex(zIndex);
}

void UiScrollbar::SetRail(const Rect rail)
{
    bounds_ = rail;
    Layout();
}

void UiScrollbar::SetRange(const float viewport, const float content, const float offset)
{
    viewport_ = std::max(viewport, 0.0F);
    content_ = std::max(content, 0.0F);
    offset_ = std::clamp(offset, 0.0F, std::max(content_ - viewport_, 0.0F));
    Layout();
}

bool UiScrollbar::IsShown() const noexcept
{
    return rail_ != nullptr && rail_->IsVisible();
}

float UiScrollbar::Travel() const noexcept
{
    const float length = axis_ == Axis::Vertical ? bounds_.height : bounds_.width;
    return std::max(length - thumbLength_, 0.0F);
}

float UiScrollbar::ThumbStart() const noexcept
{
    const float maximum = content_ - viewport_;
    return maximum > 0.0F ? Travel() * offset_ / maximum : 0.0F;
}

void UiScrollbar::Layout()
{
    if (rail_ == nullptr)
        return;
    const bool vertical = axis_ == Axis::Vertical;
    const float length = vertical ? bounds_.height : bounds_.width;
    const float thickness = vertical ? bounds_.width : bounds_.height;
    // Nothing to scroll: the rail and bar disappear instead of showing a full-length bar.
    const bool shown = content_ > viewport_ && length > 0.0F;
    rail_->SetVisible(shown);
    if (!shown)
    {
        dragging_ = false;
        return;
    }
    rail_->SetBounds(bounds_);
    // The visible bar is half the grab strip, centred in it; the minimum length scales with it.
    const float bar = thickness * 0.5F, inset = thickness * 0.25F;
    const float minimum = std::min(MinimumThumb * thickness / (vertical ? GrabWidth : HorizontalGrabWidth), length);
    thumbLength_ = std::clamp(length * viewport_ / content_, minimum, length);
    const float start = ThumbStart();
    if (vertical)
    {
        // Bounds are Y-up: offset zero puts the bar at the top of the rail.
        track_->SetBounds({inset, 0.0F, bar, length});
        thumb_->SetBounds({inset, length - start - thumbLength_, bar, thumbLength_});
    }
    else
    {
        track_->SetBounds({0.0F, inset, length, bar});
        thumb_->SetBounds({start, inset, thumbLength_, bar});
    }
}

float UiScrollbar::Along(const Point canvasPointer) const
{
    // Measured in the rail's own units, so a scaled host maps the pointer the same way.
    const Rect rail = rail_->BoundsInCanvas();
    if (axis_ == Axis::Vertical)
        return rail.height > 0.0F ? (rail.y + rail.height - canvasPointer.y) * bounds_.height / rail.height : 0.0F;
    return rail.width > 0.0F ? (canvasPointer.x - rail.x) * bounds_.width / rail.width : 0.0F;
}

bool UiScrollbar::OnThumb(const Point canvasPointer) const
{
    if (!rail_->BoundsInCanvas().Contains(canvasPointer))
        return false;
    const float along = Along(canvasPointer), start = ThumbStart();
    return along >= start && along <= start + thumbLength_;
}

void UiScrollbar::BeginDrag(const Point canvasPointer)
{
    if (!IsShown())
        return;
    // The bar keeps its grab point; a press on the rail centres the bar under the pointer.
    grab_ = OnThumb(canvasPointer) ? Along(canvasPointer) - ThumbStart() : thumbLength_ * 0.5F;
    dragging_ = true;
}

float UiScrollbar::DragOffset(const Point canvasPointer)
{
    const float travel = Travel(), maximum = content_ - viewport_;
    const float start = std::clamp(Along(canvasPointer) - grab_, 0.0F, travel);
    offset_ = travel > 0.0F ? maximum * start / travel : 0.0F;
    Layout();
    return offset_;
}

void UiScrollbar::OnPointer(const PointerEvent &event)
{
    if (rail_ == nullptr)
        return;
    if (event.type == PointerEventType::Press && event.button == PointerButton::Left)
        BeginDrag(event.canvasPosition);
    if (!scrolled_)
        return;
    // Event-following hosts: the router keeps sending moves to the pressed strip until release.
    if (event.type == PointerEventType::Release)
        dragging_ = false;
    if (event.type == PointerEventType::Wheel && wheel_)
        wheel_(event.wheelDelta);
    const bool moved = event.type == PointerEventType::Press || event.type == PointerEventType::Move;
    if (dragging_ && moved)
        scrolled_(DragOffset(event.canvasPosition));
    Paint(event.type != PointerEventType::Leave && OnThumb(event.canvasPosition));
}

std::optional<float> UiScrollbar::Track(const std::optional<Point> canvasPointer, const bool leftButtonDown)
{
    if (!IsShown())
    {
        dragging_ = false;
        return std::nullopt;
    }
    if (!leftButtonDown)
        dragging_ = false;
    Paint(canvasPointer.has_value() && OnThumb(*canvasPointer));
    if (!dragging_ || !canvasPointer.has_value())
        return std::nullopt;
    return DragOffset(*canvasPointer);
}

void UiScrollbar::Release() noexcept
{
    dragging_ = false;
    Paint(false);
}

void UiScrollbar::Paint(const bool hovered) noexcept
{
    if (thumb_ == nullptr)
        return;
    const auto state = static_cast<std::uint8_t>(dragging_ ? 2 : hovered ? 1 : 0);
    if (painted_ && paintedState_ == state)
        return;
    const Color colour = state == 2 ? ThumbHeld : state == 1 ? ThumbHovered : Thumb;
    if (auto *sprite = thumb_->GetComponent<SpriteVisualComponent>())
    {
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        sprite->SetStyle(style);
    }
    painted_ = true;
    paintedState_ = state;
}
