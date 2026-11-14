#pragma once
#include "Judgement/AccuracyScore.h"

#include <format>
#include <string>
#include <string_view>

// Displayed accuracy keeps two decimals and discards the rest, so 99.999999%
// reads 99.99% and 100.00% appears only for an exact 100%. Ranks, records and
// scoring keep using the stored 0.000001% units.
inline constexpr std::wstring_view EmptyAccuracyText = L"--.--%";

inline std::wstring AccuracyText(const AccuracyScore accuracy)
{
    const auto hundredths = accuracy.Units() / 10'000U;
    return std::format(L"{}.{:02}%", hundredths / 100U, hundredths % 100U);
}
