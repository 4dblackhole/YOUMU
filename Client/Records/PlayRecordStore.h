#pragma once
#include "Mode/Results/IPlayResult.h"
#include <filesystem>
#include <string>
#include <vector>

struct PlayRecord
{
    std::int64_t id{}, playedAtMilliseconds{};
    std::filesystem::path patternPath;
    std::string mode;
    AccuracyScore accuracy;
    std::size_t totalNotes{}, processedNotes{};
    bool completed{}, cleared{}, pauseMenuUsed{};
    JudgeLevel judgeLevel;
    PlayOptions options;
    RhythmDuration globalOffset{};
    static PlayRecord From(const IPlayResult &result);
};
class PlayRecordStore final
{
  public:
    // Where records lived before they moved into the game folder:
    // %LOCALAPPDATA%/YOUMU/Records/plays.sqlite3. Empty when LOCALAPPDATA is unavailable.
    static std::filesystem::path LegacyPath();
    // Prepares the game-folder database once. When `database` does not exist yet and `legacy`
    // does, copies the legacy database (and a rollback journal beside it) to `database`; the
    // legacy files are never changed. Returns the path to open: `database`, or `legacy` when
    // the copy failed so the old records stay reachable.
    static std::filesystem::path AdoptLegacyDatabase(const std::filesystem::path &database,
                                                     const std::filesystem::path &legacy);
    explicit PlayRecordStore(std::filesystem::path database) : path_(std::move(database))
    {
    }
    void Save(const PlayRecord &record) const;
    // Supported conditions: named option comparisons, AND/OR/NOT, parentheses.
    // Unknown fields, arbitrary SQL and ill-typed values are rejected.
    static void ValidateQuery(std::string_view condition);
    std::vector<PlayRecord> Query(const std::filesystem::path &pattern, std::string_view mode,
                                  std::string_view condition, std::size_t offset, std::size_t limit) const;

  private:
    std::filesystem::path path_;
};
