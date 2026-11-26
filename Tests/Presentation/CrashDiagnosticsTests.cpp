#include "Diagnostics/CrashDumpLog.h"
#include "Diagnostics/GameDiagnostics.h"
#include "Diagnostics/GameLog.h"
#include "Diagnostics/LaunchDiagnostics.h"
#include "Diagnostics/RecentStatistics.h"
#include "ScopedTestFolder.h"
#include "YOUMUAssets.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace PhantomCore;

namespace
{
    void Check(const bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }

    [[nodiscard]] std::string ReadAll(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] std::size_t Count(const std::string &text, const std::string_view part)
    {
        std::size_t count = 0;
        for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size()))
            ++count;
        return count;
    }

    [[nodiscard]] std::vector<std::filesystem::path> LogFiles(const std::filesystem::path &folder)
    {
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::directory_iterator(folder))
        {
            if (entry.path().extension() == L".log")
                files.push_back(entry.path());
        }
        std::ranges::sort(files);
        return files;
    }

    void TestLaunchOptions()
    {
        const auto plain = ParseDiagnosticsLaunchOptions(LR"("C:\Games\game.exe" --smoke-lobby)");
        const bool nothing = plain.logDirectory.empty() && !plain.removeGraphicsDevice && !plain.stallMainLoop;
        Check(nothing, "A command line without the hidden switches must ask for nothing.");

        const auto device = ParseDiagnosticsLaunchOptions(LR"(game.exe --crash-test=device --log-dir=C:\Temp\logs)");
        Check(device.removeGraphicsDevice && !device.stallMainLoop, "--crash-test=device must be read alone.");
        Check(device.logDirectory == LR"(C:\Temp\logs)", "An unquoted --log-dir ends at the end of the line.");

        const auto stall =
            ParseDiagnosticsLaunchOptions(LR"(game.exe --log-dir="C:\Temp\my logs" --crash-test=stall --smoke-lobby)");
        Check(stall.stallMainLoop && !stall.removeGraphicsDevice, "--crash-test=stall must be read alone.");
        Check(stall.logDirectory == LR"(C:\Temp\my logs)", "A quoted --log-dir keeps its spaces and loses its quotes.");

        const auto thrown = ParseDiagnosticsLaunchOptions(L"game.exe --crash-test=exception");
        const auto crashed = ParseDiagnosticsLaunchOptions(L"game.exe --crash-test=crash");
        Check(thrown.throwException && !thrown.crash, "--crash-test=exception must be read alone.");
        Check(crashed.crash && !crashed.throwException, "--crash-test=crash must be read alone.");
        const bool named = std::string_view(plain.CrashTestName()) == "none" &&
                           std::string_view(device.CrashTestName()) == "device" &&
                           std::string_view(crashed.CrashTestName()) == "crash";
        Check(named, "Each crash test has its name for the log.");

        const auto watched = ParseDiagnosticsLaunchOptions(LR"("C:\Games\game.exe" --diagnostics=watch)");
        Check(watched.watch && !watched.crash, "--diagnostics=watch must be read alone.");
        Check(!plain.watch && !device.watch, "A run without --diagnostics=watch is a normal run.");
        const bool modes = std::string_view(watched.ModeName()) == "watch" &&
                           std::string_view(plain.ModeName()) == "normal";
        Check(modes, "Each mode has its name for the log.");
        // The limits are constants, so the compiler checks them: each mode has its own, the watch mode's shorter.
        static_assert(GameDiagnostics::StallSeconds(false) == GameDiagnostics::MainLoopStallSeconds);
        static_assert(GameDiagnostics::StallSeconds(true) == GameDiagnostics::WatchedMainLoopStallSeconds);
        static_assert(GameDiagnostics::WatchedMainLoopStallSeconds < GameDiagnostics::MainLoopStallSeconds);
    }

    void TestRecentStatistics()
    {
        // Heap, like the game's: the snapshot array alone is several kilobytes.
        const auto recent = std::make_unique<RecentStatistics>();
        const auto samples = std::make_unique<std::array<StatisticsSample, RecentStatistics::Capacity>>();
        Check(recent->Snapshot(*samples) == 0, "A new history is empty.");
        for (std::uint32_t second = 0; second < RecentStatistics::Capacity + 10; ++second)
        {
            StatisticsSample sample;
            sample.second = second;
            SetScene(sample, "A Scene name that is longer than the field");
            recent->Add(sample);
        }
        Check(recent->Snapshot(*samples) == RecentStatistics::Capacity, "The history keeps its capacity and no more.");
        const bool ordered = samples->front().second == 10 && samples->back().second == RecentStatistics::Capacity + 9;
        Check(ordered, "The history returns the newest samples, oldest first.");
        const std::string_view scene = samples->front().scene;
        Check(scene.size() == sizeof(StatisticsSample::scene) - 1,
              "A long Scene name is cut to the field and terminated.");
    }

    // A normal run writes no line per sample; a watched run writes each one as it comes.
    void TestWatchedStatistics(const std::filesystem::path &folder)
    {
        std::filesystem::path file;
        {
            const game_log::Session session(folder, 10);
            file = session.File();
            const auto diagnostics = std::make_unique<GameDiagnostics>();
            PhantomCore::PerformanceStatistics performance;
            performance.framesPerSecond = 361;
            diagnostics->AddSample(performance, 1.0, "Quiet.Scene");
            diagnostics->Watch();
            performance.framesPerSecond = 722;
            diagnostics->AddSample(performance, 2.0, "Watched.Scene");
        }
        std::ifstream stream(file, std::ios::binary);
        const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        Check(text.find("Quiet.Scene") == std::string::npos, "A normal run must not write its samples.");
        const bool written = text.find("fps  722") != std::string::npos &&
                             text.find("Watched.Scene\r\n") != std::string::npos;
        Check(written, "A watched run writes each sample as one line.");
    }

    // Work outside the game loop: a normal run writes nothing about it; a watched run writes a line when a
    // piece starts and a line when it ends, with the worker's thread and how it ended.
    void TestWatchedWork(const std::filesystem::path &folder)
    {
        std::filesystem::path file;
        std::uint32_t worker = 0;
        {
            const game_log::Session session(folder, 10);
            file = session.File();
            const auto diagnostics = std::make_unique<GameDiagnostics>();
            {
                const BackgroundWork quiet("quiet work");
            }
            diagnostics->Watch();
            std::jthread([&worker] {
                worker = GetCurrentThreadId();
                BackgroundWork watched("watched work");
                watched.Cancelled();
            }).join();
            {
                const BackgroundWork done("finished work");
            }
        }
        // The observation ended with the diagnostics: nothing more is written, and nothing is called.
        {
            const BackgroundWork late("late work");
        }
        const std::string text = ReadAll(file);
        const bool quiet = text.find("quiet work") == std::string::npos && text.find("late work") == std::string::npos;
        Check(quiet, "Only a watched run writes the work outside the game loop.");
        Check(text.find(std::format("work started: watched work (thread {})\r\n", worker)) != std::string::npos,
              "A watched piece of work starts with a line that names its thread.");
        const std::size_t ended = text.find("work ended: watched work, cancelled after ");
        Check(ended != std::string::npos, "A watched piece of work ends with a line that says how.");
        Check(text.find(std::format(" ms (thread {})\r\n", worker), ended) != std::string::npos,
              "The ending line carries the time and the thread.");
        Check(text.find("work ended: finished work, succeeded after ") != std::string::npos,
              "A piece that just ends is written as succeeded.");
    }

    // Twelve old logs and a file that is not a log: opening a session that keeps ten leaves the nine newest
    // old ones, the new one, and the other file.
    void TestLogFilesKept(const std::filesystem::path &folder)
    {
        for (int index = 0; index < 12; ++index)
            std::ofstream(folder / std::format(L"youmu-20200101-0000{:02}.log", index)) << "old";
        std::ofstream(folder / L"notes.txt") << "not a log";
        {
            const game_log::Session session(folder, 10);
            Check(!session.File().empty(), "A session in a writable folder must open its file.");
        }
        const auto logs = LogFiles(folder);
        Check(logs.size() == 10, "Ten logs stay, the new one included.");
        Check(logs.front().filename() == L"youmu-20200101-000003.log", "The oldest logs are the ones removed.");
        Check(std::filesystem::exists(folder / L"notes.txt"), "Files that are not game logs are left alone.");
    }

    void TestLogText(const std::filesystem::path &folder)
    {
        std::filesystem::path file;
        {
            const game_log::Session session(folder, 10);
            file = session.File();
            wchar_t profileFolder[MAX_PATH]{};
            const DWORD profileLength = GetEnvironmentVariableW(L"USERPROFILE", profileFolder, MAX_PATH);
            Check(profileLength > 3 && profileLength < MAX_PATH, "The test needs the USERPROFILE folder.");
            const std::string profile = game_log::ToUtf8(profileFolder);
            const std::string game = game_log::ToUtf8(AssetExecutableDirectory().wstring());
            game_log::Write("songs " + profile + "\\Music\\Songs and " + game + "\\Data");
            game_log::WritePlain("plain line");
            game_log::WriteNow("last line");
            const std::string text = ReadAll(file);
            Check(text.find("songs %USERPROFILE%\\Music\\Songs and <game>\\Data\r\n") != std::string::npos,
                  "The profile and game folders must be replaced in a log line.");
            Check(text.find(profile) == std::string::npos, "The profile folder must not appear in the log.");
            const bool stamped = text.size() > 15 && text[0] == '[' && text[13] == ']';
            Check(stamped, "A line starts with the time of day in brackets.");
            Check(Count(text, "\r\n") == 3, "Each line ends once and none is lost.");
        }
        game_log::Write("after the session");
        Check(ReadAll(file).find("after the session") == std::string::npos,
              "A line after the session reaches no file.");
    }

    void TestDiagnosticsText(const std::filesystem::path &folder)
    {
        const game_log::Session session(folder, 10);
        const auto diagnostics = std::make_unique<GameDiagnostics>();
        PerformanceStatistics performance;
        performance.framesPerSecond = 360;
        performance.updatesPerSecond = 14000;
        performance.textAtlasPages = 16;
        performance.textGlyphRasterizations = 37;
        performance.textAtlasEvictions = 2;
        performance.longestLoopSeconds = 0.0021;
        diagnostics->AddSample(performance, 12.0, "Lobby");
        diagnostics->OnEngineRecord("window: 1920x1080 Windowed, display 360 Hz, render rate Display");

        GraphicsDeviceLossReport report;
        report.operation = "IDXGISwapChain4::Present";
        report.resultCode = 0x887A0005;
        report.removedReason = 0x887A0006;
        report.removedReasonName = "DXGI_ERROR_DEVICE_HUNG";
        report.renderIndex = 4242;
        report.width = 1920;
        report.height = 1080;
        diagnostics->OnGraphicsDeviceLost(report);
        Check(diagnostics->OnFatalError("IDXGISwapChain4::Present failed with HRESULT 0x887A0005.") ==
                  FatalErrorPresentation::EngineDialog,
              "The engine keeps showing its own error dialog.");
        {
            // Held open over the stall, as a worker that is still reading would be.
            const BackgroundWork held("held work");
            diagnostics->OnMainLoopStalled({10.5, 77, 123456});
        }
        diagnostics->OnMainLoopResumed(14.5);

        const std::string text = ReadAll(session.File());
        Check(text.find("engine: window: 1920x1080 Windowed") != std::string::npos, "Engine records go to the log.");
        Check(text.find("graphics device lost: IDXGISwapChain4::Present failed with 0x887A0005, reason "
                        "DXGI_ERROR_DEVICE_HUNG (0x887A0006)") != std::string::npos,
              "A lost device is written with the failed call and the reason.");
        Check(text.find("frame 4242, back buffer 1920x1080") != std::string::npos, "The loss names its frame.");
        Check(text.find("fatal error: IDXGISwapChain4::Present failed") != std::string::npos,
              "The fatal error is written.");
        // Once for the lost device (the fatal error that follows does not repeat it), once for the stall.
        Check(Count(text, "recent statistics, 1 seconds") == 2, "The statistics follow each ending once.");
        Check(text.find("fps  360 ups  14000") != std::string::npos, "A statistics line carries the sample.");
        Check(text.find("longest    2100 us") != std::string::npos, "The longest loop pass is in microseconds.");
        Check(text.find("text 16 pages   37 new  2 evicted") != std::string::npos,
              "A statistics line carries the glyph atlas counters.");
        Check(text.find("main loop stalled: no pass for 10.5 s (thread 77, 123456 passes before)") != std::string::npos,
              "A stall is written with its length, thread and passes.");
        Check(text.find("main loop resumed after 14.5 s") != std::string::npos, "The end of a stall is written.");
        // The work outside the loop at each of those moments: none at the lost device, one piece at the stall.
        Check(Count(text, "work in progress: none\r\n") == 1, "A lost device says that no work was in progress.");
        const std::size_t stalled = text.find("main loop stalled: ");
        const std::string held = std::format("work in progress: held work (thread {}, ", GetCurrentThreadId());
        Check(text.find(held, stalled) != std::string::npos, "A stall names the work in progress and its thread.");
        Check(Count(text, "work in progress:") == 2, "The work in progress is written once per ending.");

        // A crash: the dump thread tells the observer. The folder of the dump (it may hold a user name) is
        // not written, only the file name; and the statistics follow, since nothing wrote them for this ending.
        CrashDumpReport crash;
        crash.reason = CrashDumpReason::UnhandledException;
        crash.exceptionCode = 0xC0000005;
        crash.exceptionAddress = 0x7FF612345678;
        crash.threadId = 77;
        crash.dumpWritten = true;
        crash.dumpPath = LR"(C:\Users\someone\Game\Data\Crash\youmu-20261125-101010-crash.dmp)";
        CrashDumpLog dumpLog(diagnostics->Recent());
        dumpLog.OnCrashDump(crash);
        const std::string after = ReadAll(session.File());
        Check(after.find("crash: unhandled exception, code 0xC0000005 at 0x7FF612345678, thread 77, dump written: "
                         "youmu-20261125-101010-crash.dmp") != std::string::npos,
              "A crash is written with its code, address, thread and dump file name.");
        Check(after.find("someone") == std::string::npos, "The folder of the dump must not appear in the log.");
        Check(Count(after, "recent statistics, 1 seconds") == 3, "A crash is followed by the statistics.");
    }
    // Not a pass/fail check: prints what the game loop pays for the diagnostics. The sample is added once a
    // second; the Scene name is compared on every Update.
    void PrintCosts()
    {
        const auto diagnostics = std::make_unique<GameDiagnostics>();
        PerformanceStatistics performance;
        constexpr int samples = 2000;
        const auto sampleStart = std::chrono::steady_clock::now();
        for (int index = 0; index < samples; ++index)
            diagnostics->AddSample(performance, index, "YOUMU.Lobby");
        const std::chrono::duration<double, std::micro> sampleTime = std::chrono::steady_clock::now() - sampleStart;

        const std::string logged = "YOUMU.Lobby";
        // Two separate strings, taken in turn, so the comparison is done each time rather than once.
        const std::string current[2]{"YOUMU.Lobby", "YOUMU.Lobby"};
        constexpr int compares = 2'000'000;
        int same = 0;
        const auto compareStart = std::chrono::steady_clock::now();
        for (int index = 0; index < compares; ++index)
            same += std::string_view(current[index & 1]) == logged ? 1 : 0;
        const std::chrono::duration<double, std::nano> compareTime = std::chrono::steady_clock::now() - compareStart;
        // A piece of work in a run that is not watched: taking and giving back its place in the table.
        constexpr int pieces = 200'000;
        const auto workStart = std::chrono::steady_clock::now();
        for (int index = 0; index < pieces; ++index)
            const BackgroundWork work("measured work");
        const std::chrono::duration<double, std::nano> workTime = std::chrono::steady_clock::now() - workStart;
        std::cout << "Costs: one sample " << sampleTime.count() / samples
                  << " us (once a second), one Scene name check " << compareTime.count() / compares
                  << " ns (every Update, " << same << " equal), one unwatched piece of work "
                  << workTime.count() / pieces << " ns (once per piece).\n";
    }
} // namespace

int wmain(int argc, wchar_t *argv[])
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("Expected the folder for test fixtures.");
        TestLaunchOptions();
        TestRecentStatistics();
        {
            const ScopedTestFolder kept(argv[1], L"kept");
            TestLogFilesKept(kept.Path());
        }
        {
            const ScopedTestFolder text(argv[1], L"text");
            TestLogText(text.Path());
        }
        {
            const ScopedTestFolder diagnostics(argv[1], L"diagnostics");
            TestDiagnosticsText(diagnostics.Path());
        }
        {
            const ScopedTestFolder watched(argv[1], L"watched");
            TestWatchedStatistics(watched.Path());
        }
        {
            const ScopedTestFolder work(argv[1], L"work");
            TestWatchedWork(work.Path());
        }
        PrintCosts();
        std::cout << "Crash diagnostics passed: launch switches, statistics history, kept log files, hidden folder "
                     "names, lost device, fatal error, stall and crash lines, work in progress, watched statistics and work.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
