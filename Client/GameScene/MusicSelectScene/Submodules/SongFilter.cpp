#include "SongFilter.h"
#include "SongSelectionText.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

using namespace song_select;

namespace
{
    constexpr std::wstring_view Blank = L" \t\r\n";

    [[nodiscard]] std::size_t Index(const SongFilterRow row) noexcept
    {
        return static_cast<std::size_t>(row);
    }
    [[nodiscard]] std::wstring_view Trim(std::wstring_view text) noexcept
    {
        const auto first = text.find_first_not_of(Blank);
        if (first == text.npos)
            return {};
        return text.substr(first, text.find_last_not_of(Blank) - first + 1);
    }
    template <class T> bool InRange(T value, const std::optional<T> &minimum, const std::optional<T> &maximum)
    {
        const bool aboveMinimum = !minimum || value >= *minimum;
        return aboveMinimum && (!maximum || value <= *maximum);
    }
    // An unknown or non-finite value never satisfies a bound (an uncalculated difficulty, a chart without notes).
    bool InOptionalRange(const std::optional<double> &value, const std::optional<double> &minimum,
                         const std::optional<double> &maximum)
    {
        if (!value || !std::isfinite(*value))
            return false;
        return InRange(*value, minimum, maximum);
    }
    template <class T> bool Read(std::wstring_view text, std::optional<T> &result)
    {
        text = Trim(text);
        if (text.empty())
        {
            result.reset();
            return true;
        }
        std::string ascii;
        for (const auto character : text)
        {
            if (character > 127)
                return false;
            ascii.push_back(static_cast<char>(character));
        }
        T value{};
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != ascii.data() + ascii.size())
            return false;
        if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(value))
                return false;
        result = value;
        return true;
    }
    bool ReadPair(const SongFilterDraft &draft, const SongFilterRow row, std::optional<double> &minimum,
                  std::optional<double> &maximum)
    {
        const auto first = Index(row) * 2;
        return Read(draft.values[first], minimum) && Read(draft.values[first + 1], maximum);
    }
    template <class T> bool Reversed(const std::optional<T> &minimum, const std::optional<T> &maximum)
    {
        return minimum && maximum && *minimum > *maximum;
    }
    template <class T> bool HasBound(const std::optional<T> &minimum, const std::optional<T> &maximum)
    {
        return minimum || maximum;
    }
    bool IsNegative(const std::optional<double> &value)
    {
        return value && *value < 0;
    }
    bool IsOutside(const std::optional<double> &value, double minimum, double maximum)
    {
        return value && (*value < minimum || *value > maximum);
    }
    template <class T> std::wstring Text(const std::optional<T> &value)
    {
        return value ? std::format(L"{}", *value) : L"";
    }
    [[nodiscard]] bool SameTag(const std::string &candidate, const std::wstring &folded)
    {
        return FoldSearchText(DecodeDisplayText(candidate)) == folded;
    }
    [[nodiscard]] bool HasTag(const SongCatalogEntry &song, const SongCatalogPattern &pattern, const std::wstring &tag)
    {
        const auto folded = FoldSearchText(tag);
        const auto matches = [&folded](const std::string &candidate) { return SameTag(candidate, folded); };
        return std::ranges::any_of(song.music.tags, matches) || std::ranges::any_of(pattern.pattern.tags, matches);
    }
    SongFilterError ParseRanges(const SongFilterDraft &draft, SongFilter &parsed)
    {
        const auto notes = Index(SongFilterRow::Notes) * 2;
        if (!Read(draft.values[notes], parsed.minimumNotes) || !Read(draft.values[notes + 1], parsed.maximumNotes))
            return SongFilterError::Notes;
        const bool bpmRead = ReadPair(draft, SongFilterRow::Bpm, parsed.minimumBpm, parsed.maximumBpm);
        if (!bpmRead || IsNegative(parsed.minimumBpm) || IsNegative(parsed.maximumBpm))
            return SongFilterError::Bpm;
        const bool judgeRead = ReadPair(draft, SongFilterRow::JudgeLevel, parsed.minimumJudgeLevel, parsed.maximumJudgeLevel);
        if (!judgeRead || IsOutside(parsed.minimumJudgeLevel, 1.0, 100.0) || IsOutside(parsed.maximumJudgeLevel, 1.0, 100.0))
            return SongFilterError::JudgeLevel;
        if (!ReadPair(draft, SongFilterRow::Difficulty, parsed.minimumDifficulty, parsed.maximumDifficulty))
            return SongFilterError::Difficulty;
        const bool hpRead = ReadPair(draft, SongFilterRow::Hp, parsed.minimumHp, parsed.maximumHp);
        if (!hpRead || IsOutside(parsed.minimumHp, 1.0, 100.0) || IsOutside(parsed.maximumHp, 1.0, 100.0))
            return SongFilterError::Hp;
        const bool lengthRead = ReadPair(draft, SongFilterRow::Length, parsed.minimumLength, parsed.maximumLength);
        if (!lengthRead || IsNegative(parsed.minimumLength) || IsNegative(parsed.maximumLength))
            return SongFilterError::Length;
        return SongFilterError::None;
    }
    bool AnyReversed(const SongFilter &parsed)
    {
        const bool counts = Reversed(parsed.minimumNotes, parsed.maximumNotes) || Reversed(parsed.minimumBpm, parsed.maximumBpm);
        const bool levels = Reversed(parsed.minimumJudgeLevel, parsed.maximumJudgeLevel) ||
                            Reversed(parsed.minimumDifficulty, parsed.maximumDifficulty);
        const bool rest = Reversed(parsed.minimumHp, parsed.maximumHp) || Reversed(parsed.minimumLength, parsed.maximumLength);
        return counts || levels || rest;
    }
} // namespace

bool SongFilter::IsRowActive(const SongFilterRow row) const noexcept
{
    if (switchedOff[Index(row)])
        return false;
    switch (row)
    {
    case SongFilterRow::Notes:
        return HasBound(minimumNotes, maximumNotes);
    case SongFilterRow::Bpm:
        return HasBound(minimumBpm, maximumBpm);
    case SongFilterRow::JudgeLevel:
        return HasBound(minimumJudgeLevel, maximumJudgeLevel);
    case SongFilterRow::Difficulty:
        return HasBound(minimumDifficulty, maximumDifficulty);
    case SongFilterRow::Hp:
        return HasBound(minimumHp, maximumHp);
    case SongFilterRow::Length:
        return HasBound(minimumLength, maximumLength);
    case SongFilterRow::Tag:
        return !tag.empty();
    }
    return false;
}

bool SongFilter::IsActive() const noexcept
{
    if (!mode.empty())
        return true;
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
        if (IsRowActive(static_cast<SongFilterRow>(row)))
            return true;
    return false;
}

bool SongFilter::Matches(const SongCatalogEntry &song, const SongCatalogPattern &entry) const
{
    const auto &pattern = entry.pattern;
    if (!mode.empty() && mode != pattern.mode)
        return false;
    if (IsRowActive(SongFilterRow::Notes) && (!entry.noteCount || !InRange(*entry.noteCount, minimumNotes, maximumNotes)))
        return false;
    if (IsRowActive(SongFilterRow::Bpm) && !InOptionalRange(pattern.baseBpm, minimumBpm, maximumBpm))
        return false;
    if (IsRowActive(SongFilterRow::JudgeLevel) && !InRange(pattern.judgementLevel.Value(), minimumJudgeLevel, maximumJudgeLevel))
        return false;
    if (IsRowActive(SongFilterRow::Difficulty) && !InOptionalRange(entry.calculatedDifficulty, minimumDifficulty, maximumDifficulty))
        return false;
    if (IsRowActive(SongFilterRow::Hp) && !InRange(pattern.hpLevel.Value(), minimumHp, maximumHp))
        return false;
    if (IsRowActive(SongFilterRow::Length) && !InOptionalRange(entry.chartLengthSeconds, minimumLength, maximumLength))
        return false;
    return !IsRowActive(SongFilterRow::Tag) || HasTag(song, entry, tag);
}

bool SongFilterDraft::HasValue(const SongFilterRow row) const noexcept
{
    if (row == SongFilterRow::Tag)
        return !Trim(tag).empty();
    const auto first = Index(row) * 2;
    return !Trim(values[first]).empty() || !Trim(values[first + 1]).empty();
}

void SongFilterDraft::ResetEmptyRow(const SongFilterRow row) noexcept
{
    if (!HasValue(row))
        switchedOff[Index(row)] = false;
}

void SongFilterDraft::SetValue(const std::size_t index, std::wstring text)
{
    values.at(index) = std::move(text);
    ResetEmptyRow(static_cast<SongFilterRow>(index / 2));
}

void SongFilterDraft::SetTag(std::wstring text)
{
    tag = std::move(text);
    ResetEmptyRow(SongFilterRow::Tag);
}

bool SongFilterDraft::ToggleRow(const SongFilterRow row) noexcept
{
    if (!HasValue(row))
        return false;
    switchedOff[Index(row)] = !switchedOff[Index(row)];
    return true;
}

SongFilterDraft FormatSongFilter(const SongFilter &filter)
{
    SongFilterDraft draft;
    draft.mode = filter.mode;
    draft.values = {Text(filter.minimumNotes), Text(filter.maximumNotes), Text(filter.minimumBpm), Text(filter.maximumBpm),
                    Text(filter.minimumJudgeLevel), Text(filter.maximumJudgeLevel), Text(filter.minimumDifficulty),
                    Text(filter.maximumDifficulty), Text(filter.minimumHp), Text(filter.maximumHp),
                    Text(filter.minimumLength), Text(filter.maximumLength)};
    draft.tag = filter.tag;
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
        draft.switchedOff[row] = filter.switchedOff[row] && draft.HasValue(static_cast<SongFilterRow>(row));
    return draft;
}

SongFilterError ParseSongFilter(const SongFilterDraft &draft, SongFilter &result)
{
    SongFilter parsed;
    parsed.mode = draft.mode;
    if (const auto error = ParseRanges(draft, parsed); error != SongFilterError::None)
        return error;
    if (AnyReversed(parsed))
        return SongFilterError::ReversedRange;
    parsed.tag = std::wstring(Trim(draft.tag));
    for (std::size_t row = 0; row < SongFilterRowCount; ++row)
        parsed.switchedOff[row] = draft.switchedOff[row] && draft.HasValue(static_cast<SongFilterRow>(row));
    result = std::move(parsed);
    return SongFilterError::None;
}
