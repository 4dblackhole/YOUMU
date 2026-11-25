#pragma once

#include <filesystem>

// User data the game writes lives in the game folder (beside the EXE) so players can find,
// back up and move it with the game: <game>/Data/...

// <game>/Data
[[nodiscard]] std::filesystem::path GameDataDirectory();

// <game>/Data/Result/plays.sqlite3. Records of the earlier per-user location
// (%LOCALAPPDATA%/YOUMU/Records) are not read: they predate scores (schema 1) and the user chose
// to start over. That folder is left as it is.
[[nodiscard]] std::filesystem::path PlayRecordDatabasePath();

// <game>/Data/LocalOffsets.txt: per-pattern local offsets (LocalOffsetStore).
[[nodiscard]] std::filesystem::path LocalOffsetFilePath();

// <game>/Data/Logs: one log file per run (game_log).
[[nodiscard]] std::filesystem::path GameLogDirectory();
