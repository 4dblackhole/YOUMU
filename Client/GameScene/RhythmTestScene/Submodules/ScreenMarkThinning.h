#pragma once

#include <cstdint>
#include <optional>

// Chooses which marks along one object to draw. Marks closer together on screen
// than a minimum distance overlap into one shape, so only the first mark of each
// band of that width is drawn. Bands are measured from the object's own origin,
// so the same marks stay chosen while the object scrolls. The rule depends only
// on screen distance, never on the kind of object: any scroll speed, playback
// rate, window size or mark density gets the same treatment.
class ScreenMarkThinning final
{
  public:
    // screenPerLocal: screen pixels per local unit along the marks' axis.
    ScreenMarkThinning(float minimumScreenDistance, float screenPerLocal) noexcept;

    // Offsets from the object's origin, in local units and in drawing order.
    // True when the mark starts a new band and should be drawn.
    [[nodiscard]] bool Admit(float localOffset) noexcept;

  private:
    // Local units per band; zero or less draws every mark.
    double bandWidth_{};
    std::optional<std::int64_t> lastBand_;
};
