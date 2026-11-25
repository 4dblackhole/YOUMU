#pragma once

#include "Diagnostics/RecentStatistics.h"
#include "PhantomCore.h"

#include <atomic>
#include <string_view>

// What the game does with the engine's diagnostics: engine records go to the
// log as they come, and a lost graphics device, a fatal error and a stalled
// main loop are written with the last two minutes of statistics.
//
// wWinMain owns one and hands it to the game, which passes it to the engine
// and feeds it the one-second samples. It must outlive Run.
class GameDiagnostics final : public PhantomCore::IEngineDiagnosticSink
{
  public:
    // How long the main loop may stand still before the engine reports it.
    // Loading a song takes a fraction of this; a hang never ends.
    static constexpr double MainLoopStallSeconds = 10.0;

    // Once per performance sample, from the game loop.
    void AddSample(const PhantomCore::PerformanceStatistics &performance, double totalSeconds,
                   std::string_view scene) noexcept;
    [[nodiscard]] const RecentStatistics &Recent() const noexcept { return recent_; }

    void OnEngineRecord(std::string_view line) noexcept override;
    void OnGraphicsDeviceLost(const PhantomCore::GraphicsDeviceLossReport &report) noexcept override;
    [[nodiscard]] PhantomCore::FatalErrorPresentation OnFatalError(std::string_view message) noexcept override;
    void OnMainLoopStalled(const PhantomCore::MainLoopStallReport &report) noexcept override;
    void OnMainLoopResumed(double stalledSeconds) noexcept override;

  private:
    // The statistics explain one ending. A lost device is followed by the
    // fatal error it causes, which must not print them a second time.
    void WriteRecentOnce() noexcept;

    RecentStatistics recent_;
    std::atomic<bool> recentWritten_{};
};
