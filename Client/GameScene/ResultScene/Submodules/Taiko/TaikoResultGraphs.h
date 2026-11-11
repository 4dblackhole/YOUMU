#pragma once
#include "PhantomCore.h"
#include "Taiko/Results/TaikoPlayResult.h"
namespace taiko_result_ui
{
    struct GraphData
    {
        std::vector<PhantomCore::RectanglePrimitive> rectangles;
        double halfRangeMilliseconds{100};
        double startSeconds{}, endSeconds{};
        std::size_t maximumBinCount{};
    };
    GraphData BuildGraphs(const TaikoPlayResult &result, PhantomCore::Rect scatter,
                          PhantomCore::Rect histogram, float minimumBarWidth = 1.0F);
    std::vector<PhantomCore::RectanglePrimitive> BuildHpCurve(const PlayResultSummary &result, PhantomCore::Rect plot);
} // namespace taiko_result_ui
