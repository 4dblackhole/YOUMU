#include "SongFilter.h"
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
    template<class T> bool InRange(T value, const std::optional<T> &minimum, const std::optional<T> &maximum)
    {
        return (!minimum || value >= *minimum) && (!maximum || value <= *maximum);
    }
    template<class T> bool Read(std::wstring_view text, std::optional<T> &result)
    {
        const auto first = text.find_first_not_of(L" \t\r\n");
        if (first == text.npos) { result.reset(); return true; }
        text = text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1);
        std::string ascii;
        for (const auto character : text)
        {
            if (character > 127) return false;
            ascii.push_back(static_cast<char>(character));
        }
        T value{};
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != ascii.data() + ascii.size()) return false;
        if constexpr (std::is_floating_point_v<T>) if (!std::isfinite(value)) return false;
        result = value;
        return true;
    }
    template<class T> bool Reversed(const std::optional<T> &minimum, const std::optional<T> &maximum)
    {
        return minimum && maximum && *minimum > *maximum;
    }
    template<class T> std::wstring Text(const std::optional<T> &value)
    {
        return value ? std::format(L"{}", *value) : L"";
    }
}

bool SongFilter::IsActive() const noexcept
{
    return !mode.empty() || minimumNotes || maximumNotes || minimumBpm || maximumBpm ||
           minimumJudgeLevel || maximumJudgeLevel || minimumDifficulty || maximumDifficulty;
}

bool SongFilter::Matches(const SongCatalogPattern &entry) const noexcept
{
    const auto &pattern = entry.pattern;
    if (!mode.empty() && mode != pattern.mode) return false;
    if (!InRange(entry.noteCount.value_or(pattern.notes.size()), minimumNotes, maximumNotes)) return false;
    if ((minimumBpm || maximumBpm) && (!std::isfinite(pattern.baseBpm) ||
        !InRange(pattern.baseBpm, minimumBpm, maximumBpm))) return false;
    if (!InRange(pattern.judgementLevel.Value(), minimumJudgeLevel, maximumJudgeLevel)) return false;
    if (minimumDifficulty || maximumDifficulty)
    {
        if (!entry.calculatedDifficulty || !std::isfinite(*entry.calculatedDifficulty) ||
            !InRange(*entry.calculatedDifficulty, minimumDifficulty, maximumDifficulty)) return false;
    }
    return true;
}

SongFilterDraft FormatSongFilter(const SongFilter &filter)
{
    return {filter.mode, {Text(filter.minimumNotes), Text(filter.maximumNotes),
        Text(filter.minimumBpm), Text(filter.maximumBpm), Text(filter.minimumJudgeLevel),
        Text(filter.maximumJudgeLevel), Text(filter.minimumDifficulty), Text(filter.maximumDifficulty)}};
}

SongFilterError ParseSongFilter(const SongFilterDraft &draft, SongFilter &result)
{
    SongFilter parsed;
    parsed.mode = draft.mode;
    if (!Read(draft.values[0], parsed.minimumNotes) || !Read(draft.values[1], parsed.maximumNotes))
        return SongFilterError::Notes;
    if (!Read(draft.values[2], parsed.minimumBpm) || !Read(draft.values[3], parsed.maximumBpm) ||
        (parsed.minimumBpm && *parsed.minimumBpm < 0) || (parsed.maximumBpm && *parsed.maximumBpm < 0))
        return SongFilterError::Bpm;
    if (!Read(draft.values[4], parsed.minimumJudgeLevel) || !Read(draft.values[5], parsed.maximumJudgeLevel) ||
        (parsed.minimumJudgeLevel && (*parsed.minimumJudgeLevel < 1 || *parsed.minimumJudgeLevel > 100)) ||
        (parsed.maximumJudgeLevel && (*parsed.maximumJudgeLevel < 1 || *parsed.maximumJudgeLevel > 100)))
        return SongFilterError::JudgeLevel;
    if (!Read(draft.values[6], parsed.minimumDifficulty) || !Read(draft.values[7], parsed.maximumDifficulty))
        return SongFilterError::Difficulty;
    if (Reversed(parsed.minimumNotes, parsed.maximumNotes) || Reversed(parsed.minimumBpm, parsed.maximumBpm) ||
        Reversed(parsed.minimumJudgeLevel, parsed.maximumJudgeLevel) ||
        Reversed(parsed.minimumDifficulty, parsed.maximumDifficulty)) return SongFilterError::ReversedRange;
    result = std::move(parsed);
    return SongFilterError::None;
}
