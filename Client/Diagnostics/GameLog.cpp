#include "Diagnostics/GameLog.h"

#include "YOUMUAssets.h"

#include <atomic>
#include <cstdio>
#include <system_error>

namespace game_log
{
    namespace
    {
        constexpr std::wstring_view FilePrefix = L"youmu-";
        constexpr std::size_t LineCapacity = 2048;

        // The file is opened for appending only, so each WriteFile lands whole at
        // the end: lines from different threads never mix and no lock is needed.
        std::atomic<HANDLE> file{INVALID_HANDLE_VALUE};

        // Folder names to keep out of the log, as UTF-8. Set once by the Session
        // before any other thread exists and only read afterwards.
        struct HiddenFolder
        {
            std::string path;
            std::string_view shown;
        };
        std::array<HiddenFolder, 2> hiddenFolders;

        [[nodiscard]] std::wstring UserProfileFolder()
        {
            std::wstring folder(MAX_PATH, L'\0');
            const DWORD length =
                GetEnvironmentVariableW(L"USERPROFILE", folder.data(), static_cast<DWORD>(folder.size()));
            // Zero is "not set"; a length that does not fit is the size it would need.
            folder.resize(length < folder.size() ? length : 0);
            return folder;
        }

        // The game folder comes first: it is often inside the profile folder, and
        // the shorter name says more.
        void RememberHiddenFolders()
        {
            hiddenFolders[0] = {ToUtf8(AssetExecutableDirectory().lexically_normal().wstring()), "<game>"};
            hiddenFolders[1] = {ToUtf8(UserProfileFolder()), "%USERPROFILE%"};
        }

        [[nodiscard]] std::string HideFolders(const std::string_view line)
        {
            std::string text(line);
            for (const HiddenFolder &folder : hiddenFolders)
            {
                // A drive root or an unset variable would replace far too much.
                if (folder.path.size() < 4)
                    continue;
                for (std::size_t at = text.find(folder.path); at != std::string::npos;
                     at = text.find(folder.path, at + folder.shown.size()))
                {
                    text.replace(at, folder.path.size(), folder.shown);
                }
            }
            return text;
        }

        // "youmu-20261125-061530.log". Names carry their time, so name order is age order.
        [[nodiscard]] std::filesystem::path NewFileName()
        {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            return std::format(L"{}{:04}{:02}{:02}-{:02}{:02}{:02}.log", FilePrefix, now.wYear, now.wMonth, now.wDay,
                               now.wHour, now.wMinute, now.wSecond);
        }

        // Leaves the newest keep logs in the folder.
        void RemoveOldLogs(const std::filesystem::path &directory, const std::size_t keep)
        {
            std::vector<std::filesystem::path> logs;
            std::error_code error;
            for (std::filesystem::directory_iterator entry(directory, error), end; !error && entry != end;
                 entry.increment(error))
            {
                const std::wstring name = entry->path().filename().wstring();
                if (name.starts_with(FilePrefix) && entry->path().extension() == L".log")
                    logs.push_back(entry->path());
            }
            std::ranges::sort(logs);
            for (std::size_t index = 0; index + keep < logs.size(); ++index)
                std::filesystem::remove(logs[index], error);
        }

        // Allocates nothing: the line is built in a buffer on the stack and
        // goes out in one WriteFile.
        void WriteStamped(const std::string_view shown) noexcept
        {
            SYSTEMTIME now{};
            GetLocalTime(&now);
            char text[LineCapacity];
            // A line longer than the buffer loses its end, never its beginning.
            const int length =
                std::snprintf(text, sizeof(text) - 2, "[%02u:%02u:%02u.%03u] %.*s", now.wHour, now.wMinute, now.wSecond,
                              now.wMilliseconds, static_cast<int>(shown.size()), shown.data());
            if (length <= 0)
                return;
            std::size_t size = std::min(static_cast<std::size_t>(length), sizeof(text) - 3);
            text[size] = '\0';
            OutputDebugStringA(text);
            OutputDebugStringA("\n");
            text[size++] = '\r';
            text[size++] = '\n';
            WriteRaw(text, size);
        }

        void WriteLine(const std::string_view line, const bool toDisk) noexcept
        {
            try
            {
                WriteStamped(HideFolders(line));
                if (toDisk)
                    Flush();
            }
            catch (...)
            {
                // A line that cannot be built is lost; logging never ends the game.
            }
        }
    } // namespace

    Session::Session(const std::filesystem::path &directory, const std::uint32_t keepNewest)
    {
        try
        {
            RememberHiddenFolders();
            std::filesystem::create_directories(directory);
            // The new file is one of the logs that stay.
            RemoveOldLogs(directory, keepNewest > 0 ? keepNewest - 1 : 0);
            std::filesystem::path path = directory / NewFileName();
            const HANDLE opened = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE,
                                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (opened == INVALID_HANDLE_VALUE)
                return;
            file.store(opened);
            file_ = std::move(path);
        }
        catch (...)
        {
            // No log file. The game runs without one.
        }
    }

    Session::~Session()
    {
        const HANDLE opened = file.exchange(INVALID_HANDLE_VALUE);
        if (opened != INVALID_HANDLE_VALUE)
            CloseHandle(opened);
    }

    void Write(const std::string_view line) noexcept
    {
        WriteLine(line, false);
    }

    void WriteNow(const std::string_view line) noexcept
    {
        WriteLine(line, true);
    }

    std::string ToUtf8(const std::wstring_view text)
    {
        if (text.empty())
            return {};
        const int length =
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(std::max(length, 0)), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr,
                            nullptr);
        return result;
    }

    void WritePlain(const std::string_view line) noexcept
    {
        WriteStamped(line);
    }

    void WriteRaw(const char *text, const std::size_t length) noexcept
    {
        const HANDLE opened = file.load();
        if (opened == INVALID_HANDLE_VALUE || text == nullptr)
            return;
        DWORD written = 0;
        WriteFile(opened, text, static_cast<DWORD>(length), &written, nullptr);
    }

    void Flush() noexcept
    {
        const HANDLE opened = file.load();
        if (opened != INVALID_HANDLE_VALUE)
            FlushFileBuffers(opened);
    }
} // namespace game_log
