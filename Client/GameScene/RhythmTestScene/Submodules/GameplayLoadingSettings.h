#pragma once

#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

#include <cstdint>
#include <optional>

// The play settings plate at the right of the loading screen (osu!lazer's player loader):
// background brightness and the pattern's local offset. While the pointer rests on the plate
// (or drags its slider) the attempt waits to start. It owns a Canvas under the pause menu,
// created on first use. Display and pointer handling only; the scene applies the values.
class GameplayLoadingSettings final
{
  public:
    static constexpr std::uint32_t CanvasOrder = 9; // Below the pause menu (10).

    struct Interaction
    {
        bool holdStart{};                      // Pointer over the plate or dragging the slider.
        std::optional<int> brightness;         // Changed while dragging (percent).
        bool brightnessCommitted{};            // The drag ended: save the shown brightness.
        std::int32_t offsetDirection{};        // A -/+ button press: -1 or 1; the scene applies the step.
    };

    GameplayLoadingSettings(PhantomCore::ScreenVisual2DManager &visuals, TextCatalog &texts)
        : visuals_(visuals), texts_(texts)
    {
    }
    ~GameplayLoadingSettings()
    {
        Shutdown();
    }
    GameplayLoadingSettings(const GameplayLoadingSettings &) = delete;
    GameplayLoadingSettings &operator=(const GameplayLoadingSettings &) = delete;

    void Show(std::uint32_t width, std::uint32_t height);
    void Hide() noexcept;
    [[nodiscard]] bool Visible() const noexcept { return visible_; }
    // Values to display; text changes only when a value changes.
    void SetValues(int brightnessPercent, std::int32_t offsetMs);
    [[nodiscard]] Interaction Update(const PhantomCore::InputState &input);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    void Create(std::uint32_t width, std::uint32_t height);
    void ShowBrightness(int percent);
    [[nodiscard]] std::optional<PhantomCore::Point> Pointer(const PhantomCore::InputState &input) const;
    [[nodiscard]] int BrightnessAt(PhantomCore::Point pointer) const;

    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    PhantomCore::ScreenCanvasHandle handle_;
    PhantomCore::Visual2DNode *plate_{};
    PhantomCore::Visual2DNode *track_{};
    PhantomCore::Visual2DNode *fill_{};
    PhantomCore::Visual2DNode *knob_{};
    PhantomCore::Visual2DNode *brightnessValue_{};
    PhantomCore::Visual2DNode *minus_{};
    PhantomCore::Visual2DNode *plus_{};
    PhantomCore::Visual2DNode *offsetValue_{};
    std::uint32_t width_{}, height_{};
    int shownBrightness_{-1};
    std::optional<std::int32_t> shownOffset_;
    bool visible_{};
    bool dragging_{};
};
