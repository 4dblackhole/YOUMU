#pragma once
#include "PhantomCore.h"

// HP gauge of the redesign (UI-02), five skin parts: a constant frame, a fill
// that switches from Low to Clear at the gauge's clear threshold, a clear marker
// at that threshold and a lock shown once the attempt's HP has been disabled.
// No number is displayed; the gauge rules stay in the domain.
struct GameplayHpState
{
    double percent{};
    double clearPercent{60.0};
    // Set after the pause menu or Rewind disabled HP for this attempt.
    bool locked{};
};
class GameplayHpView final
{
  public:
    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, PhantomCore::Visual2DNode &parent);
    // gearTop is the Canvas y of the gear's upper edge; the frame sits 16 above it.
    void SetLayout(float logicalWidth, float gearTop);
    void Update(const GameplayHpState &state);
    void Shutdown() noexcept
    {
        frame_ = fill_ = clearMarker_ = lock_ = nullptr;
        state_ = {};
        lowImage_ = clearImage_ = {};
    }

  private:
    void Apply();
    PhantomCore::Visual2DNode *frame_{}, *fill_{}, *clearMarker_{}, *lock_{};
    PhantomCore::ImageHandle lowImage_{}, clearImage_{};
    GameplayHpState state_{};
};
