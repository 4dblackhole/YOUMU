#pragma once
#include "Timing/MusicalTimeline.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

inline RhythmTime Microseconds(double milliseconds)
{
    using Time = RhythmTime;
    const long double value = static_cast<long double>(milliseconds) * 1000;
    if (!std::isfinite(value) || value <= std::numeric_limits<Time::rep>::lowest() + 1.0L ||
        value >= std::numeric_limits<Time::rep>::max() - 1.0L)
        throw std::invalid_argument("Editor time must fit the rhythm clock's microsecond range.");
    return Time{static_cast<Time::rep>(std::llround(value))};
}

inline RhythmTime StepEditorTimeline(const MusicalTimeline &timeline, double milliseconds,
                                     int subdivisionsPerBeat, int direction)
{
    if (subdivisionsPerBeat <= 0)
        throw std::invalid_argument("Editor beat division must be positive.");
    const auto whole = std::max(0.0L, timeline.WholeNotesAtTime(Microseconds(milliseconds)));
    const auto division = static_cast<std::int64_t>(subdivisionsPerBeat) * 4;
    const auto gridValue = std::round(whole * division);
    if (!std::isfinite(gridValue) || gridValue >= std::numeric_limits<std::int64_t>::max())
        throw std::out_of_range("Editor grid position exceeds the supported range.");
    const auto grid = static_cast<std::int64_t>(gridValue);
    const auto origin = timeline.PositionAtWholeNotes(Rational{std::max<std::int64_t>(0, grid), division});
    const auto step = Rational{direction, division};
    const auto position = timeline.PositionToWholeNotes(origin) + step < Rational{}
                              ? MusicalPosition{} : timeline.AddWholeNotes(origin, step);
    return timeline.Compile(position);
}

inline std::int64_t MeasureNearTime(const MusicalTimeline &timeline, double milliseconds)
{
    static_cast<void>(Microseconds(milliseconds));
    // Bound the exponential search without evaluating an unnecessarily distant
    // timestamp (very low BPM can overflow the clock at the final measure).
    std::int64_t low = 0, high = 1;
    const auto at = [&](std::int64_t measure) { return timeline.MeasureSpan(measure).begin.count() / 1000.0; };
    while (at(high) <= milliseconds)
    {
        if (high == MaximumMeasureCount)
            throw std::invalid_argument("Editor time is beyond 100,000 measures.");
        low = high;
        high = std::min(high * 2, MaximumMeasureCount);
    }
    while (low + 1 < high)
    {
        const auto middle = low + (high - low) / 2;
        if (at(middle) <= milliseconds)
            low = middle;
        else
            high = middle;
    }
    return low;
}
