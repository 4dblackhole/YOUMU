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
    // Schema 2 keeps the score, maximum combo, HP gauge and the mode's result detail. A schema-1
    // database (accuracy-only records) is set aside on open as `<name>.v1.sqlite3` beside it
    // (`<name>.v1 (2).sqlite3` and so on when taken) and the store starts empty: the user chose
    // not to carry scoreless records over.
    static constexpr int SchemaVersion = 2;
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

  private:
    std::filesystem::path path_;
};
