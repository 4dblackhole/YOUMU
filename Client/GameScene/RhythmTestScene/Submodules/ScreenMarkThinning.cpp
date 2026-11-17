#include "GameScene/RhythmTestScene/Submodules/ScreenMarkThinning.h"

#include <cmath>

ScreenMarkThinning::ScreenMarkThinning(const float minimumScreenDistance, const float screenPerLocal) noexcept
{
    if (minimumScreenDistance > 0.0F && screenPerLocal > 0.0F)
        bandWidth_ = static_cast<double>(minimumScreenDistance) / static_cast<double>(screenPerLocal);
}

bool ScreenMarkThinning::Admit(const float localOffset) noexcept
{
    if (bandWidth_ <= 0.0 || !std::isfinite(localOffset))
        return true;
    const auto band = static_cast<std::int64_t>(std::floor(static_cast<double>(localOffset) / bandWidth_));
    if (lastBand_ == band)
        return false;
    lastBand_ = band;
    return true;
}
