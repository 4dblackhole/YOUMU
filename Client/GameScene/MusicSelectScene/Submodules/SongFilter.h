#pragma once
#include "Catalog/SongCatalog.h"
#include <array>
#include <optional>
#include <string>

// Song filter rows in panel order. The range rows hold a minimum and a maximum; Tag holds one text value.
enum class SongFilterRow : std::size_t
{
    Notes,
    Bpm,
    JudgeLevel,
    Difficulty,
    Hp,
    Length,
    Tag,
};
inline constexpr std::size_t SongFilterRangeCount = static_cast<std::size_t>(SongFilterRow::Tag);
inline constexpr std::size_t SongFilterRowCount = SongFilterRangeCount + 1;

// Every row has a switch. A row with a value filters until its switch is turned off; the values stay so the
// switch can turn the row back on. Bounds are inclusive and every active row is ANDed for the same pattern.
struct SongFilter
{
    std::string mode;
    std::optional<std::size_t> minimumNotes, maximumNotes;
    std::optional<double> minimumBpm, maximumBpm;
    std::optional<double> minimumJudgeLevel, maximumJudgeLevel;
    std::optional<double> minimumDifficulty, maximumDifficulty;
    std::optional<double> minimumHp, maximumHp;
    std::optional<double> minimumLength, maximumLength; // Chart length in seconds.
    std::wstring tag;                                   // One tag; matches the song's or the pattern's tags.
    std::array<bool, SongFilterRowCount> switchedOff{};
    [[nodiscard]] bool IsActive() const noexcept;
    [[nodiscard]] bool IsRowActive(SongFilterRow row) const noexcept;
    [[nodiscard]] bool Matches(const SongCatalogEntry &song, const SongCatalogPattern &pattern) const;
};

enum class SongFilterError { None, Notes, Bpm, JudgeLevel, Difficulty, Hp, Length, ReversedRange };

// Panel text: minimum/maximum pairs for the range rows in SongFilterRow order, then the tag.
struct SongFilterDraft
{
    std::string mode;
    std::array<std::wstring, SongFilterRangeCount * 2> values;
    std::wstring tag;
    std::array<bool, SongFilterRowCount> switchedOff{};

    [[nodiscard]] bool HasValue(SongFilterRow row) const noexcept;
    [[nodiscard]] bool IsRowOn(SongFilterRow row) const noexcept { return HasValue(row) && !Off(row); }
    // Edits keep a switched-off row off. A row that loses its last value is reset, so its next first
    // value turns the switch on by itself.
    void SetValue(std::size_t index, std::wstring text);
    void SetTag(std::wstring text);
    // Only a row with a value can be switched.
    bool ToggleRow(SongFilterRow row) noexcept;

  private:
    [[nodiscard]] bool Off(SongFilterRow row) const noexcept { return switchedOff[static_cast<std::size_t>(row)]; }
    void ResetEmptyRow(SongFilterRow row) noexcept;
};

[[nodiscard]] SongFilterDraft FormatSongFilter(const SongFilter &filter);
[[nodiscard]] SongFilterError ParseSongFilter(const SongFilterDraft &draft, SongFilter &result);
