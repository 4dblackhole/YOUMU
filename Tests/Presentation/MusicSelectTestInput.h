#pragma once
#include "PhantomCore.h"
namespace PhantomCore
{
struct InputStateTestAccess
{
    static void Key(PhantomCore::InputState &input, std::uint16_t key, bool down)
    {
        input.SetKey(key, down, 1);
    }
    static void Begin(PhantomCore::InputState &input)
    {
        input.BeginUpdate();
    }
    static void Mouse(PhantomCore::InputState &input, PhantomCore::Point canvasPoint, float wheel)
    {
        input.SetMousePosition(static_cast<std::int32_t>(canvasPoint.x + 640),
                               static_cast<std::int32_t>(360 - canvasPoint.y), true);
        input.AddMouseWheel(wheel, 1);
    }
};
} // namespace PhantomCore
