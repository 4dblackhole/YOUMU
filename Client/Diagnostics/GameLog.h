#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

// The game's log file: what the run started with, what changed, and why it
// ended. It is for finding out after the fact what happened on a player's PC,
// so it stays small (a normal run writes a few dozen lines) and it stays on
// that PC.
//
// Like the built-in assets there is one per process: a Session in wWinMain
// owns the file, and any code may write a line. Before a Session exists, and
// when the file could not be opened, a line only reaches the debugger.
namespace game_log
{
    inline constexpr std::uint32_t KeptLogFiles = 10;

    // Opens <directory>/youmu-YYYYMMDD-HHMMSS.log for this run and deletes the
    // oldest logs so that keepNewest remain, this one included. It never
    // throws: a folder that cannot be written leaves the game without a file.
    class Session final
    {
      public:
        Session(const std::filesystem::path &directory, std::uint32_t keepNewest);
        ~Session();
        Session(const Session &) = delete;
        Session &operator=(const Session &) = delete;

        // Empty when the file could not be opened.
        [[nodiscard]] const std::filesystem::path &File() const noexcept { return file_; }

      private:
        std::filesystem::path file_;
    };

    // One line of UTF-8 text without a line break, from any thread. The time
    // of day is put in front. The player's profile folder and the game folder
    // are replaced by %USERPROFILE% and <game>, so a log that is passed on
    // does not carry the Windows user name in its paths.
    void Write(std::string_view line) noexcept;
    // The same, and the file is on disk before the call returns: for the
    // lines that explain an ending.
    void WriteNow(std::string_view line) noexcept;

    // For a handler that must not allocate (a crash, a stalled loop). WritePlain
    // puts the time in front like Write but replaces no folder names, so the
    // line must not hold a path. WriteRaw sends the bytes as they are, in one
    // piece; the caller ends its own lines with "\r\n".
    void WritePlain(std::string_view line) noexcept;
    void WriteRaw(const char *text, std::size_t length) noexcept;
    void Flush() noexcept;

    // The file of this run; empty without a Session or when it could not be opened. Not for handlers: it copies.
    [[nodiscard]] std::filesystem::path CurrentFile();

    // Log lines are UTF-8; names and paths in the game are wide.
    [[nodiscard]] std::string ToUtf8(std::wstring_view text);
} // namespace game_log
