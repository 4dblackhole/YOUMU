#pragma once

#include "PhantomCore.h"
#include "Mode/ModeInputLayout.h"
#include <array>

// Owns the state of the existing four-key presentation. Nodes are borrowed
// from the Presenter's Canvas and are released by that Canvas.
class GameplayInputView final
{
public:
    void Initialize(PhantomCore::ScreenVisual2DManager& visuals,
        PhantomCore::Visual2DNode& inputPanel);
    void Update(const ModeInputLayout& layout, const PhantomCore::InputState& input,
        double deltaSeconds);
    void Shutdown() noexcept;

private:
    std::array<PhantomCore::Visual2DNode*, 4> keyIndicators_{};
    std::array<PhantomCore::Visual2DNode*, 4> keyGlows_{};
    std::array<PhantomCore::Visual2DNode*, 4> keyPressFlashes_{};
    std::array<double, 4> keyPressFlashRemainingSeconds_{};
    PhantomCore::ImageHandle strongKeyLightImage_{};
    PhantomCore::ImageHandle weakKeyLightImage_{};
    PhantomCore::ImageHandle keyPressFlashImage_{};
};
