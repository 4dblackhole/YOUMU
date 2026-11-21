#include "GameFlow/SongDeletion.h"
#include "Catalog/SongLayout.h"
#include <Windows.h>
#include <shellapi.h>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace
{
    // Windows compares paths without letter case; so do these checks.
    std::wstring Key(const std::filesystem::path &path)
    {
        std::wstring key = std::filesystem::absolute(path).lexically_normal().generic_wstring();
        while (key.size() > 1 && key.back() == L'/')
            key.pop_back();
        CharLowerBuffW(key.data(), static_cast<DWORD>(key.size()));
        return key;
    }

    void RequireInside(const std::filesystem::path &path, const std::filesystem::path &songsRoot)
    {
        const auto root = Key(songsRoot), key = Key(path);
        const bool inside = key.size() > root.size() + 1 && key.starts_with(root + L"/");
        if (path.empty() || songsRoot.empty() || !inside)
            throw std::invalid_argument("The files to delete are not inside the Songs folder.");
    }

    // The chart set folder replaces its files when nothing else is in it.
    void FoldIntoChartSet(SongDeletion &deletion)
    {
        const auto folder = deletion.paths.front().parent_path();
        std::set<std::wstring> deleted;
        for (const auto &path : deletion.paths)
            deleted.insert(Key(path));
        std::error_code error;
        std::filesystem::directory_iterator entries(folder, error);
        if (error)
            return;
        for (const auto &entry : entries)
            if (!deleted.contains(Key(entry.path())))
                return;
        deletion.paths = {folder};
    }
} // namespace

SongDeletion PlanDifficultyDeletion(const SongCatalogEntry &song, const std::size_t pattern,
                                    const std::filesystem::path &songsRoot)
{
    if (pattern >= song.patterns.size())
        throw std::invalid_argument("The difficulty to delete does not exist.");
    const auto &chosen = song.patterns[pattern];
    SongDeletion deletion;
    deletion.paths.push_back(chosen.patternPath);
    if (chosen.effectPath)
    {
        // A YME another chart of this song still names stays.
        const auto effect = Key(*chosen.effectPath);
        const bool shared = std::ranges::any_of(song.patterns, [&](const SongCatalogPattern &other) {
            const bool otherChart = &other != &chosen;
            return otherChart && other.effectPath && Key(*other.effectPath) == effect;
        });
        if (!shared)
            deletion.paths.push_back(*chosen.effectPath);
    }
    for (const auto &path : deletion.paths)
        RequireInside(path, songsRoot);
    FoldIntoChartSet(deletion);
    RequireInside(deletion.paths.front(), songsRoot);
    return deletion;
}

SongDeletion PlanSongDeletion(const SongCatalogEntry &song, const std::filesystem::path &songsRoot)
{
    const auto folder = SongLayout::SongFolderOfMusic(song.metadataPath);
    RequireInside(folder, songsRoot);
    return {{folder}, true};
}

std::wstring DeletionListing(const SongDeletion &deletion, const std::filesystem::path &songsRoot)
{
    const auto root = std::filesystem::absolute(songsRoot).lexically_normal();
    std::wstring listing;
    for (const auto &path : deletion.paths)
    {
        const auto absolute = std::filesystem::absolute(path).lexically_normal();
        auto line = absolute.lexically_relative(root).generic_wstring();
        if (line.empty())
            line = absolute.generic_wstring();
        std::error_code error;
        if (std::filesystem::is_directory(path, error))
            line += L"/";
        listing += (listing.empty() ? L"" : L"\n") + line;
    }
    return listing;
}

void RecyclePaths(const std::vector<std::filesystem::path> &paths)
{
    if (paths.empty())
        return;
    // SHFileOperation takes the paths as one list ending in an empty string.
    std::wstring list;
    for (const auto &path : paths)
    {
        list += std::filesystem::absolute(path).lexically_normal().wstring();
        list.push_back(L'\0');
    }
    list.push_back(L'\0');
    SHFILEOPSTRUCTW operation{};
    operation.hwnd = GetActiveWindow();
    operation.wFunc = FO_DELETE;
    operation.pFrom = list.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_WANTNUKEWARNING;
    const int result = SHFileOperationW(&operation);
    if (operation.fAnyOperationsAborted)
        throw std::runtime_error("Deleting was cancelled; some files may be left.");
    if (result != 0)
        throw std::runtime_error("Could not move the files to the Recycle Bin (error " + std::to_string(result) + ").");
}

void OpenFolderInExplorer(const std::filesystem::path &folder)
{
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(GetActiveWindow(), L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
        throw std::runtime_error("Could not open the folder (ShellExecute error " + std::to_string(result) + ").");
}
