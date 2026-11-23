#pragma once
#include "PhantomCore.h"
#include <algorithm>
#include <cmath>

// Logical height of the play screen; chart BG offsets are in these units.
inline constexpr float ChartBackgroundReferenceHeight = 720;

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

// The part of the chart BG that `visible` shows of a play screen, as texture coordinates: x and y are the
// top-left corner (v grows downwards) and width/height the extent. `screen` is the play viewport and
// `visible` a centre-origin rectangle inside it (+Y up), both in play units (720 high).
inline PhantomCore::Rect ChartBackgroundCrop(PhantomCore::Size screen, PhantomCore::Rect visible, PhantomCore::Size image,
                                             PhantomCore::Point offset = {})
{
    const auto bounds = ChartBackgroundBounds(screen, image, offset);
    const float top = bounds.y + bounds.height;
    return {(visible.x - bounds.x) / bounds.width, (top - (visible.y + visible.height)) / bounds.height,
            visible.width / bounds.width, visible.height / bounds.height};
}
