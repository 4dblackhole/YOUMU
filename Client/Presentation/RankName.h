#pragma once
#include "Judgement/AccuracyScore.h"

#include <array>
#include <cstdint>
#include <string_view>

// Rank letter for a recorded accuracy. The result screen (its rank image key is
// `Rank.<name>`) and the song-select record rows share this one table.
[[nodiscard]] constexpr std::wstring_view RankName(const AccuracyScore accuracy) noexcept
{
    struct RankThreshold
    {
        std::uint32_t minimumPercent;
        std::wstring_view name;
    };
    constexpr std::array<RankThreshold, 10> ranks{{
        {100, L"Y"},
        {99, L"X"},
        {98, L"SS"},
        {96, L"S"},
        {93, L"AA"},
        {90, L"A"},
        {85, L"B"},
        {80, L"C"},
        {75, L"D"},
        {70, L"E"},
    }};
    // Compare the recorded six-decimal units, never the truncated display text.
    for (const auto &rank : ranks)
        if (accuracy.Units() >= rank.minimumPercent * 1'000'000U)
            return rank.name;
    return L"F";
}
