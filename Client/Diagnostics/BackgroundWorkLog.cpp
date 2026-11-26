#include "Diagnostics/BackgroundWorkLog.h"

#include "Diagnostics/GameLog.h"

#include <algorithm>
#include <array>
#include <cstdio>

using namespace PhantomCore;

namespace
{
    // What snprintf wrote into room bytes: nothing when it failed or cut the text.
    [[nodiscard]] std::size_t Written(const int result, const std::size_t room) noexcept
    {
        const bool whole = result > 0 && static_cast<std::size_t>(result) < room;
        return whole ? static_cast<std::size_t>(result) : 0;
    }
} // namespace

void BackgroundWorkLog::OnBackgroundWork(const BackgroundWorkEvent &event) noexcept
{
    char line[192];
    int result = 0;
    if (event.finished)
    {
        result = std::snprintf(line, sizeof(line), "work ended: %s, %s after %.1f ms (thread %u)", event.name,
                               BackgroundWorkOutcomeName(event.outcome), event.milliseconds, event.threadId);
    }
    else
        result = std::snprintf(line, sizeof(line), "work started: %s (thread %u)", event.name, event.threadId);
    if (const std::size_t length = Written(result, sizeof(line)); length > 0)
        game_log::WritePlain(std::string_view(line, length));
}

void BackgroundWorkLog::WriteRunning() noexcept
{
    std::array<RunningBackgroundWork, BackgroundWork::MaxRunning> running{};
    const std::size_t count = BackgroundWork::Running(running);
    char line[1024];
    // Kept free behind the pieces, for the note that says how many did not fit.
    constexpr std::size_t NoteRoom = 24;
    std::size_t length =
        Written(std::snprintf(line, sizeof(line), "work in progress:%s", count == 0 ? " none" : ""), sizeof(line));
    std::size_t listed = 0;
    for (; listed < count; ++listed)
    {
        const RunningBackgroundWork &piece = running[listed];
        const std::size_t room = sizeof(line) - NoteRoom - length;
        const std::size_t added =
            Written(std::snprintf(line + length, room, "%s %s (thread %u, %.0f ms)", listed == 0 ? "" : ";",
                                  piece.name, piece.threadId, piece.milliseconds),
                    room);
        if (added == 0)
            break;
        length += added;
    }
    if (listed < count)
    {
        length += Written(std::snprintf(line + length, sizeof(line) - length, "; and %u more",
                                        static_cast<unsigned>(count - listed)),
                          sizeof(line) - length);
    }
    game_log::WritePlain(std::string_view(line, std::min(length, sizeof(line) - 1)));
}
