#pragma once
#include "Mode/Results/IPlayResult.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct PlayRecord
{
    std::int64_t id{}, playedAtMilliseconds{};
    std::filesystem::path patternPath;
    std::string mode;
    AccuracyScore accuracy;
    std::uint32_t score{};
    std::size_t totalNotes{}, processedNotes{}, maximumCombo{};
    bool completed{}, cleared{}, pauseMenuUsed{};
    JudgeLevel judgeLevel;
    PlayOptions options;
    RhythmDuration globalOffset{};
    // The mode's saved result detail (judgement counts, timing samples, HP curve and so on).
    // Query leaves it empty; PlayRecordStore::LoadDetail reads it for one record.
    std::string detail;
    static PlayRecord From(const IPlayResult &result);
    // The result screen's data again: these columns plus the mode's detail, with the chart's
    // pattern name the caller knows. Throws when the detail is missing or damaged.
    [[nodiscard]] std::unique_ptr<IPlayResult> LoadResult(std::string patternName = {}) const;
};
class PlayRecordStore final
{
  public:
    // Schema 4 keeps the score, maximum combo, HP gauge, Max strokes and the mode's result detail of
    // plays judged by the BAD-recovery rules (GAME-05). A schema 1..3 database holds records of earlier
    // rules: it is set aside on open as `<name>.v<schema>.sqlite3` beside it (`<name>.v3 (2).sqlite3`
    // and so on when taken), never read, and the store starts empty (user decision: drop them all).
    // Per-pattern offsets live in their own file and are not touched.
    static constexpr int SchemaVersion = 4;
    explicit PlayRecordStore(std::filesystem::path database) : path_(std::move(database))
    {
    }
    void Save(const PlayRecord &record) const;
    // Supported conditions: named option comparisons, AND/OR/NOT, parentheses.
    // Unknown fields, arbitrary SQL and ill-typed values are rejected.
    static void ValidateQuery(std::string_view condition);
    std::vector<PlayRecord> Query(const std::filesystem::path &pattern, std::string_view mode,
                                  std::string_view condition, std::size_t offset, std::size_t limit) const;
    // The saved result detail of record `id`; empty when the record keeps none or is gone.
    [[nodiscard]] std::string LoadDetail(std::int64_t id) const;
    // Removes record `id` for good (there is no recycle bin); false when it was already gone. Throws on a
    // database error, leaving the record where it was.
    bool Delete(std::int64_t id) const;

  private:
    std::filesystem::path path_;
};
