#pragma once

#include <filesystem>
#include <string_view>

// Hidden command-line switches that make the game fail on purpose, so the
// records it leaves can be checked. They are for tests and for trying the
// feature by hand; nothing in the options or the settings file sets them.
struct DiagnosticsLaunchOptions
{
    // --log-dir=<folder> (quotes allowed): where the log goes instead of
    // <game>/Data/Logs, so a test never writes into a player's folder.
    std::filesystem::path logDirectory;
    // --crash-test=device: the engine removes the graphics device after a
    // few seconds' worth of frames.
    bool removeGraphicsDevice{};
    // --crash-test=stall: one Update sleeps past a shortened stall limit.
    bool stallMainLoop{};
};

[[nodiscard]] DiagnosticsLaunchOptions ParseDiagnosticsLaunchOptions(std::wstring_view commandLine);
