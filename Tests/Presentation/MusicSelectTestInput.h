#pragma once
#include "PhantomCore.h"
struct InputStateTestAccess
{
    static void Key(InputState &input, std::uint16_t key, bool down)
    {
        input.SetKey(key, down, 1);
    }
    static void Begin(InputState &input)
    {
        input.BeginUpdate();
    }
    static void Mouse(InputState &input, Point canvasPoint, float wheel)
    {
        input.SetMousePosition(static_cast<std::int32_t>(canvasPoint.x + 640),
                               static_cast<std::int32_t>(360 - canvasPoint.y), true);
        input.AddMouseWheel(wheel, 1);
    }
};
