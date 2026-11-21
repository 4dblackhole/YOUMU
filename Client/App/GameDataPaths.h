#pragma once

#include <filesystem>

// User data the game writes lives in the game folder (beside the EXE) so players can find,
// back up and move it with the game: <game>/Data/...

// <game>/Data
[[nodiscard]] std::filesystem::path GameDataDirectory();

// <game>/Data/Result/plays.sqlite3. The first call adopts the records of the earlier
// per-user location (%LOCALAPPDATA%/YOUMU/Records) by copying them when the game folder
// has no database yet; see PlayRecordStore::AdoptLegacyDatabase. Thread-safe.
[[nodiscard]] std::filesystem::path PlayRecordDatabasePath();
