#pragma once
#include "Catalog/SongCatalog.h"
#include <array>
#include <optional>
#include <string>

struct SongFilter
{
    std::string mode;
    std::optional<std::size_t> minimumNotes, maximumNotes;
    std::optional<double> minimumBpm, maximumBpm;
    std::optional<double> minimumJudgeLevel, maximumJudgeLevel;
    std::optional<double> minimumDifficulty, maximumDifficulty;
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] bool Matches(const SongCatalogPattern &pattern) const noexcept;
};

enum class SongFilterError { None, Notes, Bpm, JudgeLevel, Difficulty, ReversedRange };
// Minimum/maximum pairs: notes, base BPM, JudgeLevel, calculated difficulty.
struct SongFilterDraft
{
    std::string mode;
    std::array<std::wstring, 8> values;
};
[[nodiscard]] SongFilterDraft FormatSongFilter(const SongFilter &filter);
[[nodiscard]] SongFilterError ParseSongFilter(const SongFilterDraft &draft, SongFilter &result);
