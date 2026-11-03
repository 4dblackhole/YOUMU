#pragma once
#include "PhantomCore.h"
#include <cstddef>
#include <string>
#include <string_view>

// Read-only multiline text with a clipped viewport and independent scrollbars.
class ScrollableTextComponent final : public Visual2DComponent
{
  public:
    void SetText(std::wstring text);
    [[nodiscard]] std::wstring_view Text() const noexcept
    {
        return text_;
    }
    void SetStyle(TextFont font, float fontSize, Color color);
    void SetFont(TextFont font);
    void SetClickTarget(NodeId target) noexcept
    {
        clickTarget_ = target;
    }
    // Returns true for overflowing content even at its limit, preventing the
    // enclosing song list from scrolling at the same time.
    bool Scroll(float wheelDelta, bool horizontal = false);
    [[nodiscard]] Point ScrollPosition() const noexcept
    {
        return offset_;
    }
    [[nodiscard]] Size ScrollRange() const noexcept
    {
        return range_;
    }
    void Update(double elapsedSeconds) override;
    [[nodiscard]] bool HitTest(Point localPosition) const noexcept override;
    void OnPointerEvent(const PointerEvent &event, std::vector<Action> &actions) override;

  private:
    enum class DragAxis
    {
        None,
        Horizontal,
        Vertical
    };
    void EnsureChildren();
    void UpdateLayout();
    void DragTo(Point position);
    Visual2DNode *viewport_{}, *content_{};
    Visual2DNode *verticalTrack_{}, *verticalThumb_{}, *horizontalTrack_{}, *horizontalThumb_{};
    TextVisualComponent *visual_{};
    std::wstring text_;
    std::size_t lines_{};
    Point offset_{};
    Size range_{};
    Size previousSize_{-1, -1}, previousContentSize_{-1, -1};
    Point previousOffset_{-1, -1};
    NodeId clickTarget_{};
    DragAxis dragAxis_{};
    float dragGrab_{};
    bool suppressClick_{};
};
