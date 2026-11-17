#pragma once
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Which notation of a song or chart text the screens show (Option.ini, Display/Notation).
enum class TextNotation
{
    First,     // The first notation as written in the file.
    Romanized, // The romanized line of the block; NotationText says what stands in for an empty one.
};

// True for text written in Latin letters: every code point is below U+0280 (Basic Latin through
// Latin Extended-B), so digits, ASCII punctuation and accented letters count. Empty text does not.
[[nodiscard]] inline bool IsRomanText(const std::string_view utf8) noexcept
{
    // In UTF-8 a lead byte of 0xCA or above starts a code point at or beyond U+0280.
    const auto roman = [](const char byte) { return static_cast<unsigned char>(byte) < 0xCA; };
    return !utf8.empty() && std::ranges::all_of(utf8, roman);
}

// The text to show for one block of notations (Music Name, Artist, Pattern Maker lines) and the
// romanized line above it.
//   First:     the first notation that is not empty.
//   Romanized: the romanized line. When that line is empty, a notation that is already Roman
//              text stands in for it (the first such one), and failing that the first notation.
[[nodiscard]] inline std::string_view NotationText(const std::vector<std::string> &notations,
                                                   const std::string_view romanized,
                                                   const TextNotation notation) noexcept
{
    if (notation == TextNotation::Romanized)
    {
        if (!romanized.empty())
            return romanized;
        const auto roman = std::ranges::find_if(notations, [](const std::string &text) { return IsRomanText(text); });
        if (roman != notations.end())
            return *roman;
    }
    const auto first = std::ranges::find_if(notations, [](const std::string &text) { return !text.empty(); });
    return first == notations.end() ? std::string_view{} : std::string_view{*first};
}

// The same rule for a text with a single notation (Pattern Name).
[[nodiscard]] inline std::string_view NotationText(const std::string_view text, const std::string_view romanized,
                                                   const TextNotation notation) noexcept
{
    const bool useRomanized = notation == TextNotation::Romanized && !romanized.empty();
    return useRomanized ? romanized : text;
}
