#pragma once

#include "App/AudioVolumes.h"
#include "App/OptionSettings.h"
#include "PhantomCore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

// Which volume an Alt+wheel turn changes.
enum class VolumeTarget : std::size_t
{
    Master,
    Music,
    Effects
};

// Alt+wheel volume step: 5% per wheel notch, kept within 0..100%. Fractional (touchpad) turns round to notches.
[[nodiscard]] inline AudioVolumes StepVolume(AudioVolumes volumes, const VolumeTarget target, const float notches)
{
    constexpr int StepPercent = 5;
    int &percent = target == VolumeTarget::Music     ? volumes.music
                   : target == VolumeTarget::Effects ? volumes.effects
                                                     : volumes.master;
    const auto change = static_cast<int>(std::lround(notches)) * StepPercent;
    percent = std::clamp(percent + change, 0, AudioVolumes::Maximum);
    return volumes;
}

// The osu!-style volume control: Alt+wheel on any screen (gameplay included) changes a volume and shows three
// meters (Master, Music, Effects) at the bottom-right corner. With the pointer on a meter that volume changes,
// otherwise the master. Each change is saved through OptionSettings at once; the title player and song preview
// follow its revision, and gameplay and the editor re-apply the saved volumes while they run.
// It sits on its own Canvas above every screen. Screens ignore the wheel while Alt is held (SceneWheel.h).
class VolumeOverlay final
{
  public:
    static constexpr double ShowSeconds = 1.2;

    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, OptionSettings &options,
                    PhantomCore::IWindowControl *window);
    void Shutdown() noexcept;
    // Runs after the screens each frame.
    void Update(const PhantomCore::InputState &input, double deltaSeconds);
    [[nodiscard]] bool IsVisible() const noexcept { return visibleSeconds_ > 0; }

  private:
    struct Meter
    {
        PhantomCore::Visual2DNode *column{}, *label{}, *track{}, *fill{}, *value{};
    };
    [[nodiscard]] std::optional<PhantomCore::Point> CanvasPointer(const PhantomCore::InputState &input) const;
    [[nodiscard]] VolumeTarget TargetAt(std::optional<PhantomCore::Point> pointer) const;
    void Refresh(VolumeTarget target);

    PhantomCore::ScreenCanvasHandle canvas_;
    OptionSettings *options_{};
    PhantomCore::IWindowControl *window_{};
    PhantomCore::Visual2DNode *panel_{};
    std::array<Meter, 3> meters_{};
    double visibleSeconds_{};
};
