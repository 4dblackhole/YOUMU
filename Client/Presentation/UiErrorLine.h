#pragma once

#include "PhantomCore.h"

#include <cstdint>
#include <string>

// One diagnostic on the screen error line: an English kind caption, the file location with its
// line (empty when the cause names no file) and the cause as reported.
struct ScreenErrorText
{
    std::wstring kind;
    std::wstring location;
    std::wstring cause;

    [[nodiscard]] bool Empty() const noexcept
    {
        return kind.empty() && location.empty() && cause.empty();
    }
    [[nodiscard]] bool operator==(const ScreenErrorText &) const = default;
};

// The screen error line shared by play and loading (UI-02): a white strip at the top centre with
// a red mark and alert icon on its left, then the kind, the file location in a fixed-width chip
// and the cause on one line. It owns a Canvas above the pause menu, so the cause stays readable
// while the menu is open. A line longer than the strip's maximum width is cut at the right edge.
class UiErrorLine final
{
  public:
    static constexpr std::uint32_t CanvasOrder = 11; // The pause menu's Canvas is 10.
    static constexpr float Top = 20.0F, Height = 37.0F, MaximumWidth = 720.0F, ScreenMargin = 32.0F;

    explicit UiErrorLine(PhantomCore::ScreenVisual2DManager &visuals) : visuals_(visuals)
    {
    }
    ~UiErrorLine()
    {
        Shutdown();
    }
    UiErrorLine(const UiErrorLine &) = delete;
    UiErrorLine &operator=(const UiErrorLine &) = delete;

    // Shows the text, or hides the line for an empty one. Safe to call every frame.
    void Present(const ScreenErrorText &text, std::uint32_t width, std::uint32_t height);
    void Hide() noexcept;
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;
    [[nodiscard]] bool IsShown() const noexcept
    {
        return shown_;
    }
    [[nodiscard]] const ScreenErrorText &Text() const noexcept
    {
        return text_;
    }
    // The line's Canvas, or null before the first diagnostic.
    [[nodiscard]] PhantomCore::Visual2DCanvas *Canvas() noexcept
    {
        return handle_.Get();
    }

  private:
    void Build();
    void Layout();

    PhantomCore::ScreenVisual2DManager &visuals_;
    PhantomCore::ScreenCanvasHandle handle_;
    PhantomCore::Visual2DNode *plate_{}, *mark_{}, *icon_{}, *kind_{}, *chip_{}, *location_{}, *cause_{};
    ScreenErrorText text_;
    bool shown_{};
};
