#include "Diagnostics/CrashDumpLog.h"

#include "Diagnostics/GameLog.h"

#include <cstdio>

using namespace PhantomCore;

namespace
{
    [[nodiscard]] const char *Describe(const CrashDumpReason reason) noexcept
    {
        switch (reason)
        {
        case CrashDumpReason::UnhandledException:
            return "crash: unhandled exception";
        case CrashDumpReason::Terminate:
            return "crash: std::terminate";
        case CrashDumpReason::PureVirtualCall:
            return "crash: pure virtual call";
        case CrashDumpReason::InvalidParameter:
            return "crash: the C runtime rejected an argument";
        case CrashDumpReason::Requested:
            return "dump requested";
        }
        return "dump";
    }

    // The dump's file name without its folder, as plain characters: the folder may hold the user's name, and
    // nothing here may allocate.
    void CopyFileName(const wchar_t *path, char *name, const std::size_t capacity) noexcept
    {
        const wchar_t *start = path;
        for (const wchar_t *at = path; at != nullptr && *at != 0; ++at)
        {
            if (*at == L'\\' || *at == L'/')
                start = at + 1;
        }
        std::size_t length = 0;
        for (; start != nullptr && start[length] != 0 && length + 1 < capacity; ++length)
            name[length] = start[length] < 128 ? static_cast<char>(start[length]) : '?';
        name[length] = '\0';
    }
} // namespace

void CrashDumpLog::OnCrashDump(const CrashDumpReport &report) noexcept
{
    char name[96];
    CopyFileName(report.dumpPath, name, sizeof(name));
    char line[256];
    const int length = std::snprintf(line, sizeof(line), "%s, code 0x%08X at 0x%llX, thread %u, dump %s%s",
                                     Describe(report.reason), report.exceptionCode,
                                     static_cast<unsigned long long>(report.exceptionAddress), report.threadId,
                                     report.dumpWritten ? "written: " : "not written", report.dumpWritten ? name : "");
    if (length > 0)
        game_log::WritePlain(std::string_view(line, std::min(static_cast<std::size_t>(length), sizeof(line) - 1)));
    // A stall has already written its statistics; a crash has not.
    if (report.reason != CrashDumpReason::Requested)
        recent_.WriteToLog();
    game_log::Flush();
}
