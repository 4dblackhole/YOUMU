#pragma once

#include "Diagnostics/RecentStatistics.h"
#include "PhantomCore.h"

#include <atomic>
#include <string>
#include <string_view>

// What the game does with the engine's diagnostics: engine records go to the
// log as they come, and a lost graphics device, a fatal error and a stalled
// main loop are written with the last two minutes of statistics. A stalled
// loop also gets a dump of where it waits.
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

    // The service that writes the dump of a stalled loop; it must outlive Run. Null (the default) writes none.
    void UseDumps(PhantomCore::CrashDumpService *dumps) noexcept { dumps_ = dumps; }
    // After this the game shows a fatal error itself, with note and the log file under the message; without
    // it the engine's plain dialog stays. Hidden runs never call it.
    void ShowFatalErrors(std::wstring note);
    // The watch mode: from now on every sample is also a log line, handed to Windows as it comes, so a run
    // that ends without a word (the process is killed) still shows its last second. The line is not forced
    // to the disk: that wait, once a second on the game loop, would be a hitch of its own. A normal run never
    // calls it and keeps its log free of a write per second.
    void WatchStatistics() noexcept { watch_ = true; }
    // The watched lines stop after this many bytes (about a day of them); what happens is still recorded.
    static constexpr std::size_t WatchLogBytes = 16ULL * 1024ULL * 1024ULL;

    void OnEngineRecord(std::string_view line) noexcept override;
    void OnGraphicsDeviceLost(const PhantomCore::GraphicsDeviceLossReport &report) noexcept override;
    [[nodiscard]] PhantomCore::FatalErrorPresentation OnFatalError(std::string_view message) noexcept override;
    void OnMainLoopStalled(const PhantomCore::MainLoopStallReport &report) noexcept override;
    void OnMainLoopResumed(double stalledSeconds) noexcept override;

  private:
    // The statistics explain one ending. A lost device is followed by the
    // fatal error it causes, which must not print them a second time.
    void WriteRecentOnce() noexcept;
    void WriteWatched(const StatisticsSample &sample) noexcept;

    RecentStatistics recent_;
    std::atomic<bool> recentWritten_{};
    PhantomCore::CrashDumpService *dumps_{};
    bool watch_{};
    std::size_t watchedBytes_{};
    bool showFatalErrors_{};
    std::wstring fatalErrorNote_;
};
