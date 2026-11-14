#pragma once
#include "PhantomCore.h"
#include <memory>
#include <utility>
#include <vector>
namespace taiko_result_ui
{
    // Immutable rectangle geometry shared with the draw packets. Result graphs
    // and the judgement ring are built on entry, resize and language changes, so
    // the node count stays constant however many inputs were recorded.
    class RectangleBatchVisual final : public PhantomCore::Visual2DComponent
    {
      public:
        explicit RectangleBatchVisual(std::vector<PhantomCore::RectanglePrimitive> rectangles)
            : rectangles_(std::make_shared<const std::vector<PhantomCore::RectanglePrimitive>>(std::move(rectangles)))
        {
        }
        void AppendDrawPackets(std::vector<PhantomCore::DrawPacket> &output) const override
        {
            if (rectangles_->empty())
                return;
            PhantomCore::DrawPacket packet;
            packet.type = PhantomCore::DrawPacketType::RectangleBatch;
            packet.rectangles = rectangles_;
            output.push_back(std::move(packet));
        }

      private:
        std::shared_ptr<const std::vector<PhantomCore::RectanglePrimitive>> rectangles_;
    };
} // namespace taiko_result_ui
