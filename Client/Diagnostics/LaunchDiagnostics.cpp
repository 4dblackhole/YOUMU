#include "Diagnostics/LaunchDiagnostics.h"

namespace
{
    constexpr std::wstring_view LogDirectorySwitch = L"--log-dir=";

    // The text after the switch: up to the closing quote when it starts with
    // one, otherwise up to the next space.
    [[nodiscard]] std::wstring_view SwitchValue(const std::wstring_view commandLine, const std::wstring_view name)
    {
        const std::size_t at = commandLine.find(name);
        if (at == std::wstring_view::npos)
            return {};
        std::wstring_view value = commandLine.substr(at + name.size());
        const bool quoted = !value.empty() && value.front() == L'"';
        if (quoted)
            value.remove_prefix(1);
        return value.substr(0, value.find(quoted ? L'"' : L' '));
    }
} // namespace

DiagnosticsLaunchOptions ParseDiagnosticsLaunchOptions(const std::wstring_view commandLine)
{
    DiagnosticsLaunchOptions options;
    options.logDirectory = SwitchValue(commandLine, LogDirectorySwitch);
    options.removeGraphicsDevice = commandLine.find(L"--crash-test=device") != std::wstring_view::npos;
    options.hangGraphicsDevice = commandLine.find(L"--crash-test=gpu-hang") != std::wstring_view::npos;
    options.stallMainLoop = commandLine.find(L"--crash-test=stall") != std::wstring_view::npos;
    options.throwException = commandLine.find(L"--crash-test=exception") != std::wstring_view::npos;
    options.crash = commandLine.find(L"--crash-test=crash") != std::wstring_view::npos;
    options.watch = commandLine.find(L"--diagnostics=watch") != std::wstring_view::npos;
    return options;
}

const char *DiagnosticsLaunchOptions::CrashTestName() const noexcept
{
    if (removeGraphicsDevice)
        return "device";
    if (hangGraphicsDevice)
        return "gpu-hang";
    if (stallMainLoop)
        return "stall";
    if (throwException)
        return "exception";
    return crash ? "crash" : "none";
}
