#pragma once

#include "PhantomCore.h"

#include <cstdint>

// Black over the whole gameplay screen while a restart darkens it (osu!-style): it grows with
// the restart key's hold and covers the fade-out of a Retry chosen in the pause menu. It owns a
// Canvas above the pause menu and the screen error line, created on first use.
class GameplayScreenFade final
{
  public:
    static constexpr std::uint32_t CanvasOrder = 12; // Pause menu 10, screen error line 11.

    explicit GameplayScreenFade(PhantomCore::ScreenVisual2DManager &visuals) : visuals_(visuals)
    {
    }
    ~GameplayScreenFade()
    {
        Shutdown();
    }
    GameplayScreenFade(const GameplayScreenFade &) = delete;
    GameplayScreenFade &operator=(const GameplayScreenFade &) = delete;

    // Opacity 0..1; 0 hides the Canvas. Safe to call every frame: the tint changes only on a new value.
    void Present(double opacity, std::uint32_t width, std::uint32_t height);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    PhantomCore::ScreenVisual2DManager &visuals_;
    PhantomCore::ScreenCanvasHandle handle_;
    PhantomCore::Visual2DNode *cover_{};
    float shown_{};
};
