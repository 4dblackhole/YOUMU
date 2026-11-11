#pragma once
#include "PhantomCore.h"
#include <algorithm>
#include <cmath>

// Centre-origin bounds that cover the viewport at the authored centre offset.
inline PhantomCore::Rect ChartBackgroundBounds(PhantomCore::Size viewport, PhantomCore::Size image, PhantomCore::Point offset = {})
{
    if (image.width <= 0 || image.height <= 0)
        return {-viewport.width / 2, -viewport.height / 2, viewport.width, viewport.height};
    const float scale = std::max((viewport.width + 2 * std::abs(offset.x)) / image.width,
                                 (viewport.height + 2 * std::abs(offset.y)) / image.height);
    const float width = image.width * scale, height = image.height * scale;
    return {-width / 2 + offset.x, -height / 2 + offset.y, width, height};
}
