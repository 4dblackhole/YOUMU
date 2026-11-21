#pragma once
#include "Catalog/SongCatalog.h"
#include <cstddef>
#include <filesystem>
#include <vector>

// What one delete from the song select right-click menu sends to the Recycle Bin. Every path sits
// inside the Songs folder; a plan that would reach outside it is refused.
struct SongDeletion
{
    std::vector<std::filesystem::path> paths;
    bool wholeSong{};
};

// A difficulty: its YMP, the YME when no other chart of the song uses it, and the chart set folder
// instead when nothing else would be left in it. Throws std::invalid_argument when a path is outside
// songsRoot.
[[nodiscard]] SongDeletion PlanDifficultyDeletion(const SongCatalogEntry &song, std::size_t pattern,
                                                  const std::filesystem::path &songsRoot);
// The whole song folder (<song>/Music, Chart and BG). Throws std::invalid_argument when the folder is
// not one folder inside songsRoot.
[[nodiscard]] SongDeletion PlanSongDeletion(const SongCatalogEntry &song, const std::filesystem::path &songsRoot);
// The paths shown in the confirmation, relative to songsRoot with '/' separators.
[[nodiscard]] std::wstring DeletionListing(const SongDeletion &deletion, const std::filesystem::path &songsRoot);

// Sends the paths to the Windows Recycle Bin. Windows asks first when an item cannot be recycled and
// would be deleted for good. Throws std::runtime_error when the operation fails or is cancelled.
void RecyclePaths(const std::vector<std::filesystem::path> &paths);
// Opens the folder in Windows Explorer. Throws std::runtime_error when Explorer cannot open it.
void OpenFolderInExplorer(const std::filesystem::path &folder);
