#pragma once
#include "PhantomCore.h"
#include <functional>
#include <vector>

class EditorVisual final : public PhantomCore::Visual2DComponent
{
  public:
    void Clear() noexcept
    {
        pendingRectangles_.reset();
        packets_.clear();
    }
    void AddRectangle(PhantomCore::Rect bounds, PhantomCore::Color color, float radius)
    {
        if (!pendingRectangles_)
        {
            pendingRectangles_ = std::make_shared<std::vector<PhantomCore::RectanglePrimitive>>();
            PhantomCore::DrawPacket batch;
            batch.type = PhantomCore::DrawPacketType::RectangleBatch;
            batch.rectangles = pendingRectangles_;
            packets_.push_back(std::move(batch));
        }
        pendingRectangles_->push_back({bounds, color, radius});
    }
    void AddPacket(PhantomCore::DrawPacket packet)
    {
        pendingRectangles_.reset();
        packets_.push_back(std::move(packet));
    }
    void Finish() noexcept { pendingRectangles_.reset(); }
    void AppendDrawPackets(std::vector<PhantomCore::DrawPacket> &output) const override
    {
        output.insert(output.end(), packets_.begin(), packets_.end());
    }
  private:
    std::vector<PhantomCore::DrawPacket> packets_;
    std::shared_ptr<std::vector<PhantomCore::RectanglePrimitive>> pendingRectangles_;
};
struct Control
{
    PhantomCore::Rect rect;
    std::function<void()> click;
};
