#pragma once
#include "PhantomCore.h"

// The Penpot HP design is two solid rectangles, with no decorative labels.
class GameplayHpView final
{
  public:
    void Initialize(PhantomCore::Visual2DNode &parent);
    void SetLayout(float logicalWidth);
    void Update(double percent);
    void Shutdown() noexcept
    {
        track_ = fill_ = nullptr;
        fraction_ = 0;
    }

  private:
    PhantomCore::Visual2DNode *track_{}, *fill_{};
    float fraction_{};
};
