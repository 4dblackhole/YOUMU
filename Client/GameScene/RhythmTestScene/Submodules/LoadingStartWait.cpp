#include "LoadingStartWait.h"

#include <Windows.h>

using namespace PhantomCore;

void LoadingStartWait::Begin(const bool idleWait) noexcept
{
    idleSeconds_ = 0;
    idleWait_ = idleWait;
    released_ = false;
}

void LoadingStartWait::Interrupt() noexcept
{
    idleSeconds_ = 0;
}

bool LoadingStartWait::Update(const InputState &input, const Frame &frame) noexcept
{
    // Enter starts at once, also over the plate, and stays decided for this loading screen.
    if (input.WasKeyPressed(VK_RETURN))
        released_ = true;
    if (released_)
        return false;
    if (!idleWait_)
        return frame.onPlate;
    const bool busy = !frame.windowActive || frame.onPlate;
    if (busy || Activity(input))
    {
        idleSeconds_ = 0;
        return true;
    }
    idleSeconds_ += frame.deltaSeconds;
    return idleSeconds_ < IdleSeconds;
}

// Moving the mouse, a button or the wheel, or pressing a key (the offset keys among them). Letting go is not.
bool LoadingStartWait::Activity(const InputState &input) noexcept
{
    for (const auto &event : input.Events())
    {
        const bool release =
            event.type == InputEventType::KeyReleased || event.type == InputEventType::MouseButtonReleased;
        if (!release)
            return true;
    }
    return false;
}
