#pragma once
#include "Timing/MusicalTimeline.h"
#include <algorithm>
#include <stdexcept>
struct MetronomeBeat
{
    RhythmTime time;
    bool accent{};
};
inline std::vector<MetronomeBeat> CompileQuarterBeats(const MusicalTimeline &timeline, std::int64_t lastMeasure,
                                                      RhythmTime end, std::stop_token stop = {})
{
    CheckChartLoadCancellation(stop);
    if (lastMeasure < 0 || lastMeasure > 100'000)
        throw std::invalid_argument("Metronome measure range is invalid.");
    std::vector<MetronomeBeat> beats;
    for (std::int64_t measure = 0; measure <= lastMeasure; ++measure)
    {
        CheckChartLoadCancellation(stop);
        const auto length = timeline.MeasureLength(measure);
        const auto origin = timeline.PositionToWholeNotes({measure, {}});
        for (Rational offset{}; offset < length; offset = offset + Rational{1, 4})
        {
            CheckChartLoadCancellation(stop);
            const auto position = timeline.PositionAtWholeNotes(origin + offset);
            if (!timeline.IsPositionActive(position))
                continue;
            const auto time = timeline.Compile(position);
            if (time <= end)
                beats.push_back({time, offset == Rational{}});
        }
    }
    std::size_t comparisons{};
    std::ranges::stable_sort(beats, [&stop, &comparisons](RhythmTime left, RhythmTime right)
    {
        if ((comparisons++ & 63U) == 0) CheckChartLoadCancellation(stop);
        return left < right;
    }, &MetronomeBeat::time);
    CheckChartLoadCancellation(stop);
    return beats;
}
