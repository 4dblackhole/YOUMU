#pragma once

#include "Diagnostics/RecentStatistics.h"
#include "PhantomCore.h"

// What the game writes when the dump service has written a dump: one line
// with the reason and the dump's file name, and after a crash the last two
// minutes of statistics.
//
// wWinMain owns one and gives it to the dump service, which it must outlive.
// It only reads the statistics; they belong to GameDiagnostics.
class CrashDumpLog final : public PhantomCore::ICrashDumpObserver
{
  public:
    explicit CrashDumpLog(const RecentStatistics &recent) noexcept : recent_(recent) {}

    // From the dump service's thread, possibly in a process that is about to die: nothing here allocates.
    void OnCrashDump(const PhantomCore::CrashDumpReport &report) noexcept override;

  private:
    const RecentStatistics &recent_;
};
