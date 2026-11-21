#pragma once

#include "PhantomCore.h"

#include <Windows.h>

// Alt+wheel belongs to the game-wide volume control (VolumeOverlay, osu! style), on every screen. Screens read
// the wheel through SceneWheelDelta so the same turn never also scrolls a list or zooms behind the volume.
[[nodiscard]] inline bool IsAltDown(const PhantomCore::InputState &input) noexcept
{
    return input.IsKeyDown(VK_LMENU) || input.IsKeyDown(VK_RMENU);
}

[[nodiscard]] inline float SceneWheelDelta(const PhantomCore::InputState &input) noexcept
{
    return IsAltDown(input) ? 0.0F : input.MouseWheelDelta();
}
