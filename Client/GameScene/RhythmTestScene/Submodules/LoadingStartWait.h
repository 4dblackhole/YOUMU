#pragma once

#include "PhantomCore.h"

// When a song's loading screen lets the prepared attempt start (user decision 2026-11-24, merging the plate hold of
// the play settings plate):
// - after 3 seconds without activity: a mouse move, button or wheel, or a key press;
// - at once after Enter;
// - never while the pointer rests on the plate (or drags its slider) or the game window is not active.
// The 3 seconds count from the loading screen's start, so a load that already took that long without activity
// starts as soon as it is ready. After a Retry from the pause menu or the restart key only the plate holds, so a
// quick restart stays quick (a Retry from the result screen opens a new play screen and waits like a first entry).
class LoadingStartWait final
{
  public:
    static constexpr double IdleSeconds = 3.0;
    struct Frame
    {
        double deltaSeconds{};
        bool windowActive{true};
        bool onPlate{}; // The pointer rests on the plate or drags its slider.
    };

    // A new loading screen: the first entry of a song waits for the idle seconds, a Retry does not.
    void Begin(bool idleWait) noexcept;
    // Something took the player away (the pause menu): the idle seconds count again from zero.
    void Interrupt() noexcept;
    // True while the start must wait.
    [[nodiscard]] bool Update(const PhantomCore::InputState &input, const Frame &frame) noexcept;

  private:
    [[nodiscard]] static bool Activity(const PhantomCore::InputState &input) noexcept;

    double idleSeconds_{};
    bool idleWait_{true};
    bool released_{};
};
