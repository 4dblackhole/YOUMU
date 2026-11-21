#pragma once

#include "Mode/Submodules/PlaySession.h"
#include "PhantomCore.h"

#include <optional>

// The judgement circle's Highlight emphasis (UI-02): an amber glow behind the circle, the same circle
// image in pale gold over it and one gold ring outside it. Glow and ring breathe on every beat and
// the whole emphasis fades in and out over 250 ms of song time. Nodes live under the lane root and
// the judgement circle, so the lane's rotation and the gear transform carry them.
class GameplayHighlightView final
{
  public:
    static constexpr RhythmDuration FadeDuration{250'000};

    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, PhantomCore::Visual2DNode &laneRoot,
                    PhantomCore::Visual2DNode &judgementCircle, PhantomCore::ImageHandle circleImage);
    // Lane-local size; the glow is clipped to the lane so it never spills over the input panel.
    void SetLaneLayout(float laneWidth, float laneLength, float judgementLocalY);
    void Update(const PlaySession &session, RhythmTime time);
    // Restarts from the state at the next update without fading (Retry, seeks).
    void Reset() noexcept;
    void Shutdown() noexcept;
    [[nodiscard]] float Strength() const noexcept
    {
        return strength_;
    }

  private:
    void Present(float pulse);

    PhantomCore::Visual2DNode *clip_{}, *glow_{}, *gold_{}, *ring_{};
    PhantomCore::Size glowSize_{}, ringSize_{}, circleSize_{};
    float judgementLocalY_{}, laneWidth_{}, strength_{};
    std::optional<RhythmTime> lastTime_;
};
