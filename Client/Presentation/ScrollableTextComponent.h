#pragma once
#include "PhantomCore.h"
#include <cstddef>
#include <string>
#include <string_view>

// Read-only multiline text with a clipped viewport and independent scrollbars.
class ScrollableTextComponent final : public PhantomCore::Visual2DComponent
{
  public:
    void SetText(std::wstring text);
    [[nodiscard]] std::wstring_view Text() const noexcept
    {
        return text_;
    }
    void SetStyle(PhantomCore::TextFont font, float fontSize, PhantomCore::Color color);
    void SetFont(PhantomCore::TextFont font);
    void SetClickTarget(PhantomCore::NodeId target) noexcept
    {
        clickTarget_ = target;
    }
    // Returns true for overflowing content even at its limit, preventing the
    // enclosing song list from scrolling at the same time.
    bool Scroll(float wheelDelta, bool horizontal = false);
    [[nodiscard]] PhantomCore::Point ScrollPosition() const noexcept
    {
        return offset_;
    }
    [[nodiscard]] PhantomCore::Size ScrollRange() const noexcept
    {
        return range_;
    }
    void Update(double elapsedSeconds) override;
    [[nodiscard]] bool HitTest(PhantomCore::Point localPosition) const noexcept override;
    void OnPointerEvent(const PhantomCore::PointerEvent &event, std::vector<PhantomCore::Action> &actions) override;

  private:
    enum class DragAxis
    {
        None,
        Horizontal,
        Vertical
    };
    void EnsureChildren();
    void UpdateLayout();
    void DragTo(PhantomCore::Point position);
    PhantomCore::Visual2DNode *viewport_{}, *content_{};
    PhantomCore::Visual2DNode *verticalTrack_{}, *verticalThumb_{}, *horizontalTrack_{}, *horizontalThumb_{};
    PhantomCore::TextVisualComponent *visual_{};
    std::wstring text_;
    std::size_t lines_{};
    PhantomCore::Point offset_{};
    PhantomCore::Size range_{};
    PhantomCore::Size previousSize_{-1, -1}, previousContentSize_{-1, -1};
    PhantomCore::Point previousOffset_{-1, -1};
    PhantomCore::NodeId clickTarget_{};
    DragAxis dragAxis_{};
    float dragGrab_{};
    bool suppressClick_{};
};
