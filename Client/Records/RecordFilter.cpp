#include "RecordFilter.h"
#include "RecordCondition.h"
#include <charconv>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace
{
    std::string Ascii(std::wstring_view text)
    {
        std::string result;
        for (const auto ch : text)
        {
            if (ch > 127) throw std::invalid_argument("Record conditions and numbers use ASCII.");
            result.push_back(static_cast<char>(ch));
        }
        return result;
    }
    template<class T> std::optional<T> Number(std::wstring_view text)
    {
        const auto first = text.find_first_not_of(L" \t\r\n");
        if (first == text.npos) return {};
        const auto value = Ascii(text.substr(first, text.find_last_not_of(L" \t\r\n") - first + 1));
        T number{};
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            throw std::invalid_argument("Enter a valid number; random seed must be a uint64 integer.");
        if constexpr (std::is_floating_point_v<T>)
            if (!std::isfinite(number)) throw std::invalid_argument("Filter numbers must be finite.");
        return number;
    }
    template<class T> std::wstring Text(const std::optional<T> &value)
    { return value ? std::format(L"{}", *value) : L""; }
    bool IsRejectedBound(double value, double lower, double upper, bool positive)
    {
        if (!std::isfinite(value) || value < lower || value > upper)
            return true;
        return positive && value == 0;
    }
    void ValidateRange(const std::optional<double> &minimum, const std::optional<double> &maximum,
                       double lower, double upper, const char *error, bool positive = false)
    {
        for (const auto value : {minimum, maximum})
            if (value && IsRejectedBound(*value, lower, upper, positive))
                throw std::invalid_argument(error);
        if (minimum && maximum && *minimum > *maximum)
            throw std::invalid_argument("Minimum must not exceed maximum.");
    }
}
std::string RecordFilter::Query() const
{
    ValidateRange(minimumJudgeLevel, maximumJudgeLevel, 1, 100, "JudgeLevel must be within 1-100.");
    ValidateRange(minimumAccelerate, maximumAccelerate, .5, 2, "Playback rate must be within 0.5-2.0x.");
    ValidateRange(minimumScrollSpeed, maximumScrollSpeed, 0, HUGE_VAL, "Scroll speed must be positive.", true);
    ValidateRange(minimumRandomProbability, maximumRandomProbability, 0, 100, "Random probability must be within 0-100%.");
    std::string result;
    const auto add = [&result](const std::string &term)
    {
        if (!result.empty()) result += " AND ";
        result += '(' + term + ')';
    };
    constexpr std::array names{"Mirror", "Random", "Metronome", "IgnoreScrollEffects", "PreserveMusicPitch",
        "PreserveHitSoundPitch", "JudgeOverride", "Completed", "Cleared", "PauseMenuUsed"};
    static_assert(names.size() == RecordFilterFlagCount);
    for (std::size_t i = 0; i < flags.size(); ++i)
        if (flags[i]) add(std::string{names[i]} + (*flags[i] ? " = true" : " = false"));
    if (scrollUnit)
    {
        if (*scrollUnit != 0 && *scrollUnit != 1) throw std::invalid_argument("Invalid scroll unit.");
        add("ScrollUnit = " + std::to_string(*scrollUnit));
    }
    const auto range = [&](std::size_t row, const char *name, const std::optional<double> &minimum,
                           const std::optional<double> &maximum)
    {
        if (switchedOff[row]) return;
        if (minimum) add(std::format("{} >= {}", name, *minimum));
        if (maximum) add(std::format("{} <= {}", name, *maximum));
    };
    range(0, "JudgeLevel", minimumJudgeLevel, maximumJudgeLevel);
    range(1, "Accelerate", minimumAccelerate, maximumAccelerate);
    range(2, "ScrollSpeed", minimumScrollSpeed, maximumScrollSpeed);
    range(3, "RandomProbability", minimumRandomProbability, maximumRandomProbability);
    if (randomSeed && !switchedOff[4]) add("RandomSeed = " + std::to_string(*randomSeed));
    static_cast<void>(RecordCondition::Parse(result));
    return result;
}
bool RecordFilterDraft::HasValue(const std::size_t row) const noexcept
{
    const auto filled = [this](std::size_t index)
    { return values[index].find_first_not_of(L" \t\r\n") != std::wstring::npos; };
    const std::size_t first = row * 2;
    if (first >= values.size()) return false;
    // The seed row has one value; the range rows have a minimum and a maximum.
    return filled(first) || (first + 1 < values.size() && filled(first + 1));
}
void RecordFilterDraft::SetValue(const std::size_t index, std::wstring text)
{
    values.at(index) = std::move(text);
    if (!HasValue(RowOf(index))) switchedOff.at(RowOf(index)) = false;
}
bool RecordFilterDraft::ToggleRow(const std::size_t row) noexcept
{
    if (row >= switchedOff.size() || !HasValue(row)) return false;
    switchedOff[row] = !switchedOff[row];
    return true;
}
RecordFilterDraft FormatRecordFilter(const RecordFilter &filter)
{
    RecordFilterDraft draft;
    for (std::size_t i = 0; i < filter.flags.size(); ++i)
        draft.choices[i] = !filter.flags[i] ? 0 : *filter.flags[i] ? 1 : 2;
    draft.choices[RecordFilterFlagCount] = filter.scrollUnit ? static_cast<std::size_t>(*filter.scrollUnit + 1) : 0;
    draft.values = {Text(filter.minimumJudgeLevel), Text(filter.maximumJudgeLevel),
        Text(filter.minimumAccelerate), Text(filter.maximumAccelerate), Text(filter.minimumScrollSpeed), Text(filter.maximumScrollSpeed),
        Text(filter.minimumRandomProbability), Text(filter.maximumRandomProbability), Text(filter.randomSeed)};
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
        draft.switchedOff[row] = filter.switchedOff[row] && draft.HasValue(row);
    return draft;
}
RecordFilter ParseRecordFilter(const RecordFilterDraft &draft)
{
    RecordFilter filter;
    for (std::size_t i = 0; i < filter.flags.size(); ++i)
    {
        if (draft.choices[i] > 2) throw std::invalid_argument("Invalid filter choice.");
        if (draft.choices[i]) filter.flags[i] = draft.choices[i] == 1;
    }
    if (draft.choices[RecordFilterFlagCount] > 2) throw std::invalid_argument("Invalid scroll unit.");
    if (draft.choices[RecordFilterFlagCount]) filter.scrollUnit = static_cast<int>(draft.choices[RecordFilterFlagCount] - 1);
    filter.minimumJudgeLevel = Number<double>(draft.values[0]); filter.maximumJudgeLevel = Number<double>(draft.values[1]);
    filter.minimumAccelerate = Number<double>(draft.values[2]); filter.maximumAccelerate = Number<double>(draft.values[3]);
    filter.minimumScrollSpeed = Number<double>(draft.values[4]); filter.maximumScrollSpeed = Number<double>(draft.values[5]);
    filter.minimumRandomProbability = Number<double>(draft.values[6]); filter.maximumRandomProbability = Number<double>(draft.values[7]);
    filter.randomSeed = Number<std::uint64_t>(draft.values[8]);
    for (std::size_t row = 0; row < RecordFilterValueRowCount; ++row)
        filter.switchedOff[row] = draft.switchedOff[row] && draft.HasValue(row);
    static_cast<void>(filter.Query());
    return filter;
}
