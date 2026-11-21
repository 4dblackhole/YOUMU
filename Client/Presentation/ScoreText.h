#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// SCORE as every screen shows it: digits grouped by thousands (97,531,400).
[[nodiscard]] inline std::wstring ScoreText(std::uint64_t score)
{
    auto digits = std::to_wstring(score);
    for (auto position = static_cast<std::ptrdiff_t>(digits.size()) - 3; position > 0; position -= 3)
        digits.insert(static_cast<std::size_t>(position), 1, L',');
    return digits;
}
