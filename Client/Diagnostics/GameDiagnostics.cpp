#include "Diagnostics/GameDiagnostics.h"

#include "Diagnostics/GameLog.h"

#include <Psapi.h>

#include <cstdio>

using namespace PhantomCore;

namespace
{
    constexpr std::uint64_t BytesPerMegabyte = 1024ULL * 1024ULL;

    [[nodiscard]] std::uint32_t Megabytes(const std::uint64_t bytes) noexcept
    {
        return static_cast<std::uint32_t>(bytes / BytesPerMegabyte);
    }

    [[nodiscard]] std::uint32_t Clamped(const std::uint64_t value) noexcept
    {
        return static_cast<std::uint32_t>(std::min<std::uint64_t>(value, UINT32_MAX));
    }

    // Memory this process has committed, and the kernel handles it holds: the
    // two numbers that grow when something is not released.
    void ReadProcess(StatisticsSample &sample) noexcept
    {
        PROCESS_MEMORY_COUNTERS_EX memory{};
        memory.cb = sizeof(memory);
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory),
                                    sizeof(memory)))
            sample.processMemoryMb = Megabytes(memory.PrivateUsage);
        DWORD handles = 0;
        if (GetProcessHandleCount(GetCurrentProcess(), &handles))
            sample.handles = handles;
    }
} // namespace

void GameDiagnostics::AddSample(const PerformanceStatistics &performance, const double totalSeconds,
                                const std::string_view scene) noexcept
{
    StatisticsSample sample;
    sample.second = Clamped(static_cast<std::uint64_t>(std::max(totalSeconds, 0.0)));
    sample.framesPerSecond = Clamped(performance.framesPerSecond);
    sample.updatesPerSecond = Clamped(performance.updatesPerSecond);
    sample.drawCalls = Clamped(performance.drawCallsPerFrame);
    sample.peakDrawCalls = Clamped(performance.peakDrawCallsPerFrame);
    sample.longestLoopMicroseconds =
        Clamped(static_cast<std::uint64_t>(std::max(performance.longestLoopSeconds, 0.0) * 1'000'000.0));
    sample.graphicsMemoryMb = Megabytes(performance.graphicsMemoryUsageBytes);
    sample.graphicsBudgetMb = Megabytes(performance.graphicsMemoryBudgetBytes);
    ReadProcess(sample);
    SetScene(sample, scene);
    recent_.Add(sample);
}

void GameDiagnostics::WriteRecentOnce() noexcept
{
    if (!recentWritten_.exchange(true))
        recent_.WriteToLog();
}

void GameDiagnostics::OnEngineRecord(const std::string_view line) noexcept
{
    try
    {
        game_log::Write("engine: " + std::string(line));
    }
    catch (...)
    {
    }
}

void GameDiagnostics::OnGraphicsDeviceLost(const GraphicsDeviceLossReport &report) noexcept
{
    try
    {
        const std::string operation = report.operation.empty() ? "an earlier call" : report.operation;
        game_log::WriteNow(std::format("graphics device lost: {} failed with 0x{:08X}, reason {} (0x{:08X})", operation,
                                       report.resultCode, report.removedReasonName, report.removedReason));
        game_log::Write(std::format("  frame {}, back buffer {}x{}, {} draw calls in the last finished frame",
                                    report.renderIndex, report.width, report.height, report.lastFrameDrawCalls));
        if (report.memory.available)
        {
            game_log::Write(std::format("  graphics memory {} of {} MB, {} MB in system memory",
                                        Megabytes(report.memory.usageBytes), Megabytes(report.memory.budgetBytes),
                                        Megabytes(report.memory.sharedUsageBytes)));
        }
        if (!report.extendedDataEnabled)
            game_log::Write("  device removed data was off (Option.ini [Diagnostics] DeviceRemovedData=1 turns it on)");
        for (const std::string &line : report.extendedData)
            game_log::Write("  " + line);
    }
    catch (...)
    {
    }
    WriteRecentOnce();
}

FatalErrorPresentation GameDiagnostics::OnFatalError(const std::string_view message) noexcept
{
    try
    {
        game_log::WriteNow("fatal error: " + std::string(message));
    }
    catch (...)
    {
    }
    WriteRecentOnce();
    return FatalErrorPresentation::EngineDialog;
}

// From the engine's watch thread, while the game loop is stuck. The loop's
// own objects are not touched: the log and the statistics are built for this.
void GameDiagnostics::OnMainLoopStalled(const MainLoopStallReport &report) noexcept
{
    char line[160];
    const int length =
        std::snprintf(line, sizeof(line), "main loop stalled: no pass for %.1f s (thread %u, %llu passes before)",
                      report.stalledSeconds, report.mainThreadId, static_cast<unsigned long long>(report.loopPasses));
    // No allocation here: the stuck thread may be holding the heap.
    if (length > 0)
        game_log::WritePlain(std::string_view(line, std::min(static_cast<std::size_t>(length), sizeof(line) - 1)));
    recent_.WriteToLog();
}

void GameDiagnostics::OnMainLoopResumed(const double stalledSeconds) noexcept
{
    char line[96];
    const int length = std::snprintf(line, sizeof(line), "main loop resumed after %.1f s", stalledSeconds);
    if (length > 0)
        game_log::WritePlain(std::string_view(line, std::min(static_cast<std::size_t>(length), sizeof(line) - 1)));
    game_log::Flush();
}
