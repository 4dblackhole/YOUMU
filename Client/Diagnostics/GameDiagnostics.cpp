#include "Diagnostics/GameDiagnostics.h"

#include "Diagnostics/GameLog.h"
#include "Texts/GameIdentity.h"

#include <Psapi.h>

#include <DbgHelp.h>

#pragma comment(lib, "dbghelp.lib")

#include <cstdio>
#include <fstream>

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

    // Everything the process has in memory, so the state of the renderer at a device loss can be read later with
    // the symbols of the build. Hundreds of megabytes; written for a device loss only. Returns the bytes written.
    [[nodiscard]] std::uint64_t WriteProcessMemory(const std::filesystem::path &path) noexcept
    {
        const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return 0;
        const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithFullMemory | MiniDumpWithHandleData |
                                                     MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
        const BOOL written =
            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, nullptr, nullptr, nullptr);
        LARGE_INTEGER size{};
        const BOOL sized = GetFileSizeEx(file, &size);
        CloseHandle(file);
        return written && sized ? static_cast<std::uint64_t>(size.QuadPart) : 0;
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
    sample.textAtlasPages = Clamped(performance.textAtlasPages);
    sample.textGlyphRasterizations = Clamped(performance.textGlyphRasterizations);
    sample.textAtlasEvictions = Clamped(performance.textAtlasEvictions);
    ReadProcess(sample);
    SetScene(sample, scene);
    recent_.Add(sample);
    if (watch_)
        WriteWatched(sample);
}

void GameDiagnostics::Watch()
{
    watch_ = true;
    workLog_.emplace();
}

void GameDiagnostics::WriteWatched(const StatisticsSample &sample) noexcept
{
    if (watchedBytes_ >= WatchLogBytes)
        return;
    char line[256];
    const std::size_t length = FormatSample(sample, line, sizeof(line));
    if (length == 0)
        return;
    game_log::WriteRaw(line, length);
    watchedBytes_ += length;
    if (watchedBytes_ >= WatchLogBytes)
        game_log::WritePlain("watch: the statistics lines stop here (size limit); what happens is still recorded");
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
        if (!report.gpuCrashDumpStatus.empty())
            game_log::Write("  gpu crash dump: " + report.gpuCrashDumpStatus);
        WriteGpuCrashDump(report);
    }
    catch (...)
    {
    }
    BackgroundWorkLog::WriteRunning();
    WriteRecentOnce();
}

// The driver's dump as it came, for NVIDIA's tools, and the same dump as text, which needs no tool. Beside them the
// engine's record of what the last frames drew (-gpu-frames.txt) and the instance bytes those draws read (.bin).
void GameDiagnostics::WriteGpuCrashDump(const GraphicsDeviceLossReport &report) const
{
    const bool nothing = report.gpuCrashDump.empty() && report.frameDraws.empty();
    if (nothing || gpuDumpDirectory_.empty())
        return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    const std::wstring stem = std::format(L"youmu-{:04}{:02}{:02}-{:02}{:02}{:02}-gpu", now.wYear, now.wMonth, now.wDay,
                                          now.wHour, now.wMinute, now.wSecond);
    std::error_code ignored;
    std::filesystem::create_directories(gpuDumpDirectory_, ignored);
    WriteFrameDraws(report, gpuDumpDirectory_ / (stem + L"-frames"));
    const std::filesystem::path memoryPath = gpuDumpDirectory_ / (stem + L"-process.dmp");
    const std::uint64_t memoryBytes = WriteProcessMemory(memoryPath);
    game_log::Write(memoryBytes != 0 ? std::format("  process memory file: {} ({} MB)", memoryPath.filename().string(),
                                                   Megabytes(memoryBytes))
                                     : std::string("  process memory file could not be written"));
    if (report.gpuCrashDump.empty())
        return;
    const std::filesystem::path dumpPath = gpuDumpDirectory_ / (stem + L".nv-gpudmp");
    std::ofstream dump(dumpPath, std::ios::binary);
    dump.write(reinterpret_cast<const char *>(report.gpuCrashDump.data()),
               static_cast<std::streamsize>(report.gpuCrashDump.size()));
    game_log::Write(dump ? "  gpu crash dump file: " + dumpPath.filename().string()
                         : std::string("  gpu crash dump file could not be written"));
    if (report.gpuCrashDumpJson.empty())
        return;
    std::ofstream text(dumpPath.wstring() + L".json", std::ios::binary);
    text.write(report.gpuCrashDumpJson.data(), static_cast<std::streamsize>(report.gpuCrashDumpJson.size()));
}

void GameDiagnostics::WriteFrameDraws(const GraphicsDeviceLossReport &report, const std::filesystem::path &stem) const
{
    if (report.frameDraws.empty())
        return;
    std::ofstream text(stem.wstring() + L".txt", std::ios::binary);
    for (const std::string &line : report.frameDraws)
        text << line << "\r\n";
    std::ofstream data(stem.wstring() + L".bin", std::ios::binary);
    data.write(reinterpret_cast<const char *>(report.frameDrawData.data()),
               static_cast<std::streamsize>(report.frameDrawData.size()));
    game_log::Write(std::format("  frame draws file: {}.txt ({} lines, {} bytes of instance data)",
                                stem.filename().string(), report.frameDraws.size(), report.frameDrawData.size()));
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
    if (!showFatalErrors_)
        return FatalErrorPresentation::EngineDialog;
    try
    {
        // The engine's text, then where the record of this run is.
        const int size = static_cast<int>(message.size());
        const int length = MultiByteToWideChar(CP_UTF8, 0, message.data(), size, nullptr, 0);
        std::wstring text(static_cast<std::size_t>(std::max(length, 0)), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, message.data(), size, text.data(), length);
        const std::filesystem::path log = game_log::CurrentFile();
        if (!log.empty())
            text += L"\n\n" + fatalErrorNote_ + L"\n" + log.wstring();
        MessageBoxW(nullptr, text.c_str(), GameName, MB_OK | MB_ICONERROR);
        return FatalErrorPresentation::Handled;
    }
    catch (...)
    {
        return FatalErrorPresentation::EngineDialog;
    }
}

void GameDiagnostics::ShowFatalErrors(std::wstring note)
{
    fatalErrorNote_ = std::move(note);
    showFatalErrors_ = true;
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
    BackgroundWorkLog::WriteRunning();
    recent_.WriteToLog();
    // Every thread's call stack, while the loop is still stuck. The service's observer notes the file.
    if (dumps_)
        static_cast<void>(dumps_->WriteNow("stall"));
}

void GameDiagnostics::OnMainLoopResumed(const double stalledSeconds) noexcept
{
    char line[96];
    const int length = std::snprintf(line, sizeof(line), "main loop resumed after %.1f s", stalledSeconds);
    if (length > 0)
        game_log::WritePlain(std::string_view(line, std::min(static_cast<std::size_t>(length), sizeof(line) - 1)));
    game_log::Flush();
}
