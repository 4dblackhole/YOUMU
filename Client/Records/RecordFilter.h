#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

// Mirror, Random, Metronome, IgnoreScrollEffects, music pitch, hit pitch, JudgeOverride.
inline constexpr std::size_t RecordFilterFlagCount = 7;
struct RecordFilter
{
    std::array<std::optional<bool>, RecordFilterFlagCount> flags;
    std::optional<int> scrollUnit; // Empty: all; 0: x; 1: BPM.
    std::optional<double> minimumJudgeLevel, maximumJudgeLevel;
    std::optional<double> minimumAccelerate, maximumAccelerate;
    std::optional<double> minimumScrollSpeed, maximumScrollSpeed;
    std::optional<double> minimumRandomProbability, maximumRandomProbability;
    std::optional<std::uint64_t> randomSeed;
    std::string condition; // Existing direct condition syntax, including OR/NOT.
    [[nodiscard]] std::string Query() const;
};
struct RecordFilterDraft
{
    std::array<std::size_t, RecordFilterFlagCount + 1> choices{};
    // Minimum/maximum pairs: JudgeLevel, Accelerate, ScrollSpeed, probability; then seed.
    std::array<std::wstring, 9> values;
    std::wstring condition;
};
[[nodiscard]] RecordFilterDraft FormatRecordFilter(const RecordFilter &filter);
// Builds a candidate atomically; invalid data throws without altering the applied filter.
[[nodiscard]] RecordFilter ParseRecordFilter(const RecordFilterDraft &draft);
