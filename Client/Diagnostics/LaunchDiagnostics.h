#pragma once

#include <filesystem>
#include <string_view>

// Hidden command-line switches that make the game fail on purpose, so the
// records it leaves can be checked. They are for tests and for trying the
// feature by hand; nothing in the options or the settings file sets them.
struct DiagnosticsLaunchOptions
{
    // --log-dir=<folder> (quotes allowed): where the log goes instead of
    // <game>/Data/Logs, so a test never writes into a player's folder. Dump
    // files then go to <folder>/Crash instead of <game>/Data/Crash.
    std::filesystem::path logDirectory;
    // --crash-test=device: the engine removes the graphics device after a
    // few seconds' worth of frames.
    bool removeGraphicsDevice{};
    // --crash-test=stall: one Update sleeps past a shortened stall limit.
    bool stallMainLoop{};
    // --crash-test=exception: one Update throws. The engine catches it and
    // ends the run as a fatal error (log, error dialog, exit code 1).
    bool throwException{};
    // --crash-test=crash: one Update reads through a null pointer the way a
    // real bug would. Nothing catches it: the dump service writes its file
    // and Windows ends the process.
    bool crash{};

    [[nodiscard]] const char *CrashTestName() const noexcept;
};

[[nodiscard]] DiagnosticsLaunchOptions ParseDiagnosticsLaunchOptions(std::wstring_view commandLine);
