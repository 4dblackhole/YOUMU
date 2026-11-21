#pragma once

#include "PhantomCore.h"

#include <cstdint>
#include <string>

// A short notice at the top of the play and loading screens after the local offset changes
// (osu!-style "Local offset"). It owns a Canvas above the screen fade, created on first use,
// stays for a moment and then fades out. Display only; the controller owns the value.
class GameplayOffsetNotice final
{
  public:
    static constexpr std::uint32_t CanvasOrder = 13; // Above the screen fade (12).

    explicit GameplayOffsetNotice(PhantomCore::ScreenVisual2DManager &visuals) : visuals_(visuals)
    {
    }
    ~GameplayOffsetNotice()
    {
        Shutdown();
    }
    GameplayOffsetNotice(const GameplayOffsetNotice &) = delete;
    GameplayOffsetNotice &operator=(const GameplayOffsetNotice &) = delete;

    void Show(std::wstring text, std::uint32_t width, std::uint32_t height);
    void Update(double deltaSeconds);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    PhantomCore::ScreenVisual2DManager &visuals_;
    PhantomCore::ScreenCanvasHandle handle_;
    PhantomCore::Visual2DNode *plate_{};
    PhantomCore::Visual2DNode *label_{};
    double elapsed_{};
};
