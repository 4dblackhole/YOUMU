#ifdef _DEBUG
// Must precede the C runtime headers so Debug CRT allocations retain the
// source location when they originate from malloc/calloc/realloc.
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "App/YOUMUGame.h"
#include "App/GameDataPaths.h"
#include "Diagnostics/GameDiagnostics.h"
#include "Diagnostics/GameLog.h"
#include "Diagnostics/LaunchDiagnostics.h"
#include "YOUMUAssets.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Texts/GameIdentity.h"
#include "Texts/TextCatalog.h"

#include <Windows.h>

#include <cstdlib>
#include <algorithm>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

using namespace PhantomCore;

namespace
{
    void EnableCrtMemoryLeakChecks() noexcept
    {
#ifdef _DEBUG
        // Preserve the CRT's existing diagnostics and request one automatic
        // leak dump in Visual Studio's Debug Output window at process exit.
        const int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
        _CrtSetDbgFlag(
            flags | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
#endif
    }

    using VersionFunction = LONG(WINAPI *)(OSVERSIONINFOW *);

    // What the run starts on: the build, Windows, the PC. The engine adds the
    // graphics adapter and the window, the game its options.
    void LogStart(const std::string_view initialScene, const bool smokeTest, const DiagnosticsLaunchOptions &launch)
    {
#ifdef _DEBUG
        constexpr const char *configuration = "Debug";
#else
        constexpr const char *configuration = "Release";
#endif
        game_log::Write(std::format("YOUMU, built {} {} ({})", __DATE__, __TIME__, configuration));

        // GetVersionEx reports the compatibility version; the kernel's own answer is the real build.
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        const HMODULE kernel = GetModuleHandleW(L"ntdll.dll");
        const auto readVersion =
            kernel ? reinterpret_cast<VersionFunction>(GetProcAddress(kernel, "RtlGetVersion")) : nullptr;
        if (readVersion)
            readVersion(&version);
        MEMORYSTATUSEX memory{};
        memory.dwLength = sizeof(memory);
        GlobalMemoryStatusEx(&memory);
        SYSTEM_INFO system{};
        GetSystemInfo(&system);
        game_log::Write(std::format("windows {}.{}.{}, {} logical processors, {} MB memory", version.dwMajorVersion,
                                    version.dwMinorVersion, version.dwBuildNumber, system.dwNumberOfProcessors,
                                    memory.ullTotalPhys / (1024 * 1024)));

        const char *crashTest = launch.removeGraphicsDevice ? "device" : launch.stallMainLoop ? "stall" : "none";
        game_log::Write(std::format("start: scene {}, smoke test {}, crash test {}", initialScene,
                                    smokeTest ? "yes" : "no", crashTest));
    }

    void ReportStartupError(const char *error, const bool showWindow)
    {
        game_log::WriteNow(std::string("startup error: ") + error);
        // Both startup failure paths decode UTF-8 and use the shared game name.
        const int length = MultiByteToWideChar(CP_UTF8, 0, error, -1, nullptr, 0);
        std::wstring message(static_cast<std::size_t>(std::max(length, 1)), L'\0');
        if (length > 0)
            MultiByteToWideChar(CP_UTF8, 0, error, -1, message.data(), length);
        else
            message = TextCatalog{}.Get("Startup.decodeError");
        OutputDebugStringW(message.c_str());
        if (showWindow)
            MessageBoxW(nullptr, message.c_str(), GameName, MB_OK | MB_ICONERROR);
    }
}

int WINAPI wWinMain(
    _In_ HINSTANCE,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int)
{
    EnableCrtMemoryLeakChecks();

    // The log comes first, so every later failure has somewhere to go.
    const std::wstring_view commandLine = GetCommandLineW();
    const DiagnosticsLaunchOptions launch = ParseDiagnosticsLaunchOptions(commandLine);
    const game_log::Session log(launch.logDirectory.empty() ? GameLogDirectory() : launch.logDirectory,
                                game_log::KeptLogFiles);

    std::string assetError;
    if (!InitializeBuiltInAssets(assetError))
    {
        ReportStartupError(assetError.c_str(), true);
        return EXIT_FAILURE;
    }

    // The executable selects the requested Client route. Run owns the Win32,
    // D3D12, input, and audio lifetime around that object.
    const bool basicSmokeTest =
        commandLine.find(L"--smoke-test") != std::wstring_view::npos;

    const bool smokeLobby =
        commandLine.find(L"--smoke-lobby") != std::wstring_view::npos;
    const bool smokeGameplay =
        commandLine.find(L"--smoke-gameplay") != std::wstring_view::npos;
    const bool smokeEditor =
        commandLine.find(L"--smoke-editor") != std::wstring_view::npos;
    const bool rhythmDebugMode =
        commandLine.find(L"--rhythm-debug") != std::wstring_view::npos;
    const bool smokeRoute = smokeLobby || smokeGameplay || smokeEditor;
    const bool smokeTest = smokeRoute || basicSmokeTest;
    std::string initialScene(LogoSceneId);
    if (smokeLobby)
    {
        initialScene = LobbySceneId;
    }
    else if (smokeGameplay)
    {
        initialScene = RhythmTestSceneId;
    }
    else if (smokeEditor)
    {
        initialScene = EditorSceneId;
    }
    else if (rhythmDebugMode)
    {
        initialScene = RhythmTestSceneId;
    }

    LogStart(initialScene, smokeTest, launch);
    // Declared before Run and destroyed after it: the engine keeps a pointer for the whole run.
    GameDiagnostics diagnostics;

    // Specialized hidden routes exercise catalog widgets and layered Taiko
    // assets without changing the normal executable's Logo entry point.
    try
    {
        const int exitCode = Run(std::make_unique<YOUMUGame>(smokeTest, std::move(initialScene), rhythmDebugMode,
                                                             &diagnostics, launch));
        game_log::WriteNow(std::format("exit code {}", exitCode));
        return exitCode;
    }
    catch (const std::exception &error)
    {
        ReportStartupError(error.what(), !smokeTest);
        return EXIT_FAILURE;
    }
}
