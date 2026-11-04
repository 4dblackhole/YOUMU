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
    void ValidateRange(const std::optional<double> &minimum, const std::optional<double> &maximum,
                       double lower, double upper, const char *error, bool positive = false)
    {
        for (const auto value : {minimum, maximum})
            if (value && (!std::isfinite(*value) || *value < lower || *value > upper || (positive && *value == 0)))
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
    constexpr std::array names{"Mirror", "Random", "Metronome", "IgnoreScrollEffects", "PreserveMusicPitch", "PreserveHitSoundPitch", "JudgeOverride"};
    for (std::size_t i = 0; i < flags.size(); ++i)
        if (flags[i]) add(std::string{names[i]} + (*flags[i] ? " = true" : " = false"));
    if (scrollUnit)
    {
        if (*scrollUnit != 0 && *scrollUnit != 1) throw std::invalid_argument("Invalid scroll unit.");
        add("ScrollUnit = " + std::to_string(*scrollUnit));
    }
    const auto range = [&add](const char *name, const std::optional<double> &minimum, const std::optional<double> &maximum)
    {
        if (minimum) add(std::format("{} >= {}", name, *minimum));
        if (maximum) add(std::format("{} <= {}", name, *maximum));
    };
    range("JudgeLevel", minimumJudgeLevel, maximumJudgeLevel);
    range("Accelerate", minimumAccelerate, maximumAccelerate);
    range("ScrollSpeed", minimumScrollSpeed, maximumScrollSpeed);
    range("RandomProbability", minimumRandomProbability, maximumRandomProbability);
    if (randomSeed) add("RandomSeed = " + std::to_string(*randomSeed));
    if (condition.find_first_not_of(" \t\r\n") != condition.npos)
    {
        if (result.empty()) result = condition;
        else add(condition);
    }
    static_cast<void>(RecordCondition::Parse(result));
    return result;
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
    draft.condition = std::wstring{filter.condition.begin(), filter.condition.end()};
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
    filter.condition = Ascii(draft.condition);
    static_cast<void>(filter.Query());
    return filter;
}
