#pragma once

#include "PhantomCore.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

// The scrollbar of every scrolling list (UI-02 common part): a 12px grab strip holding a flat
// 6px cobalt bar over a faint rail, with no arrows and no rounded ends. The bar darkens while the
// pointer rests on it and turns solid cobalt while held. A horizontal scrollbar is 8px with a
// 4px bar. Pressing the bar keeps the grab point; pressing the rail brings the bar's centre under
// the pointer and keeps following it. The host owns the scroll offset: it reports the range,
// follows the drag once per frame and applies the offset it gets back.
class UiScrollbar final
{
  public:
    enum class Axis : std::uint8_t
    {
        Vertical,
        Horizontal,
    };
    static constexpr float GrabWidth = 12, HorizontalGrabWidth = 8, MinimumThumb = 32;
    static constexpr PhantomCore::Color Rail = {0.122F, 0.298F, 0.6F, 0.07F};
    static constexpr PhantomCore::Color Thumb = {0.122F, 0.298F, 0.6F, 0.42F};
    static constexpr PhantomCore::Color ThumbHovered = {0.122F, 0.298F, 0.6F, 0.62F};
    static constexpr PhantomCore::Color ThumbHeld = {0.122F, 0.298F, 0.6F, 1.0F};

    UiScrollbar() = default;
    // Nodes may outlive the scrollbar (a drop-down list stays on the Canvas root); they stop calling it.
    ~UiScrollbar();
    UiScrollbar(const UiScrollbar &) = delete;
    UiScrollbar &operator=(const UiScrollbar &) = delete;

    // Creates the strip under parent. The parent's tree owns the nodes.
    void Attach(PhantomCore::Visual2DNode &parent, std::string name, Axis axis = Axis::Vertical);
    // Forgets the nodes when the host removes or rebuilds its tree.
    void Detach() noexcept;
    void SetZIndex(std::int32_t zIndex) noexcept;
    // The grab strip in the parent's (Y-up) coordinates; its thickness may be scaled.
    void SetRail(PhantomCore::Rect rail);
    // Lengths along the axis. Offset zero is the top (left) end. Nothing to scroll hides the strip.
    void SetRange(float viewport, float content, float offset);
    // Follows a drag that started on the strip. Call once per frame after the input router with the
    // pointer in Canvas coordinates; returns the offset the drag asks for.
    [[nodiscard]] std::optional<float> Track(std::optional<PhantomCore::Point> canvasPointer, bool leftButtonDown);
    // For a host without a per-frame pointer (a drop-down list): the strip follows its drag through
    // its own pointer events and reports each new offset, and hands on the wheel it receives.
    void FollowPointerEvents(std::function<void(float)> scrolled, std::function<void(float)> wheel = {});
    // Ends a drag without moving, e.g. when the host closes.
    void Release() noexcept;

    [[nodiscard]] bool IsDragging() const noexcept { return dragging_; }
    [[nodiscard]] bool IsShown() const noexcept;
    [[nodiscard]] PhantomCore::Visual2DNode *RailNode() const noexcept { return rail_; }
    [[nodiscard]] PhantomCore::Visual2DNode *ThumbNode() const noexcept { return thumb_; }

  private:
    void BeginDrag(PhantomCore::Point canvasPointer);
    void OnPointer(const PhantomCore::PointerEvent &event);
    [[nodiscard]] bool OnThumb(PhantomCore::Point canvasPointer) const;
    [[nodiscard]] float DragOffset(PhantomCore::Point canvasPointer);
    void Layout();
    void Paint(bool hovered) noexcept;
    [[nodiscard]] float Along(PhantomCore::Point canvasPointer) const;
    [[nodiscard]] float ThumbStart() const noexcept;
    [[nodiscard]] float Travel() const noexcept;

    PhantomCore::Visual2DNode *rail_{}, *track_{}, *thumb_{};
    std::function<void(float)> scrolled_, wheel_;
    // The rail's pointer handler reaches the scrollbar through this; cleared when it goes.
    std::shared_ptr<UiScrollbar *> self_;
    Axis axis_{Axis::Vertical};
    PhantomCore::Rect bounds_{};
    float viewport_{}, content_{}, offset_{}, thumbLength_{};
    // Distance from the bar's leading edge to the pointer while dragging.
    float grab_{};
    bool dragging_{}, painted_{};
    // 0 resting, 1 pointed, 2 held.
    std::uint8_t paintedState_{};
};
