#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

// Mirror, Random, Metronome, IgnoreScrollEffects, music pitch, hit pitch, JudgeOverride; then the play results
// Completed, Cleared and PauseMenuUsed.
inline constexpr std::size_t RecordFilterFlagCount = 10;
// Value rows with a switch: JudgeLevel, Accelerate, ScrollSpeed and probability ranges, then the seed.
inline constexpr std::size_t RecordFilterValueRowCount = 5;
struct RecordFilter
{
    std::array<std::optional<bool>, RecordFilterFlagCount> flags;
    std::optional<int> scrollUnit; // Empty: all; 0: x; 1: BPM.
    std::optional<int> maxStrokes; // Empty: all stroke counts; 1..4: only records played with that count.
    std::optional<double> minimumJudgeLevel, maximumJudgeLevel;
    std::optional<double> minimumAccelerate, maximumAccelerate;
    std::optional<double> minimumScrollSpeed, maximumScrollSpeed;
    std::optional<double> minimumRandomProbability, maximumRandomProbability;
    std::optional<std::uint64_t> randomSeed;
    // A switched-off value row keeps its values but does not limit the records.
    std::array<bool, RecordFilterValueRowCount> switchedOff{};
    // The restricted condition text (RecordCondition) that PlayRecordStore binds as SQL parameters.
    [[nodiscard]] std::string Query() const;
};
struct RecordFilterDraft
{
    std::array<std::size_t, RecordFilterFlagCount + 1> choices{};
    std::size_t maxStrokes{}; // 0: Any; 1..4: that stroke count.
    // Minimum/maximum pairs: JudgeLevel, Accelerate, ScrollSpeed, probability; then seed.
    std::array<std::wstring, 9> values;
    std::array<bool, RecordFilterValueRowCount> switchedOff{};

    [[nodiscard]] bool HasValue(std::size_t row) const noexcept;
    [[nodiscard]] bool IsRowOn(std::size_t row) const noexcept
    {
        return HasValue(row) && row < switchedOff.size() && !switchedOff[row];
    }
    // Same switch rule as the song filter: editing keeps a switched-off row off, and a row that loses its
    // last value is reset so its next first value turns the switch on by itself.
    void SetValue(std::size_t index, std::wstring text);
    bool ToggleRow(std::size_t row) noexcept;
    [[nodiscard]] static std::size_t RowOf(std::size_t valueIndex) noexcept { return valueIndex / 2; }
};
[[nodiscard]] RecordFilterDraft FormatRecordFilter(const RecordFilter &filter);
// Builds a candidate atomically; invalid data throws without altering the applied filter.
[[nodiscard]] RecordFilter ParseRecordFilter(const RecordFilterDraft &draft);
