#pragma once

#include <filesystem>
#include <string_view>

// Command-line switches for the records the game leaves. The crash tests make
// it fail on purpose, so the records can be checked: they are for tests and
// for trying the feature by hand. Nothing in the options or the settings file
// sets any of these.
struct DiagnosticsLaunchOptions
{
    // --log-dir=<folder> (quotes allowed): where the log goes instead of
    // <game>/Data/Logs, so a test never writes into a player's folder. Dump
    // files then go to <folder>/Crash instead of <game>/Data/Crash.
    std::filesystem::path logDirectory;
    // --crash-test=device: the engine removes the graphics device after a
    // few seconds' worth of frames.
    bool removeGraphicsDevice{};
    // --crash-test=gpu-hang: the engine draws one frame the GPU never finishes.
    // Windows resets the graphics driver about two seconds later: the screen
    // goes dark for a moment and other programs may lose their graphics
    // devices. It is the only way to see a real GPU crash dump on purpose,
    // and nothing but a person typing the switch runs it.
    bool hangGraphicsDevice{};
    // --crash-test=stall: one Update sleeps past a shortened stall limit.
    bool stallMainLoop{};
    // --crash-test=exception: one Update throws. The engine catches it and
    // ends the run as a fatal error (log, error dialog, exit code 1).
    bool throwException{};
    // --crash-test=crash: one Update reads through a null pointer the way a
    // real bug would. Nothing catches it: the dump service writes its file
    // and Windows ends the process.
    bool crash{};
    // --diagnostics=watch: the watch mode, for this run only and without a
    // change to Option.ini. Device removed data is on and every second of
    // statistics goes to the log as it comes. It is the one switch here that
    // players use: the launcher file next to the game passes it.
    bool watch{};

    [[nodiscard]] const char *CrashTestName() const noexcept;
    // "watch" or "normal", for the log.
    [[nodiscard]] const char *ModeName() const noexcept { return watch ? "watch" : "normal"; }
};

[[nodiscard]] DiagnosticsLaunchOptions ParseDiagnosticsLaunchOptions(std::wstring_view commandLine);
