#pragma once
#include "EditorLayout.h"
#include "EditorTime.h"
#include "Timing/MusicalTimeline.h"
#include <limits>

// The realtime lane, ruler and channels share one uniform time axis (EDIT-01):
// Base BPM fixes the speed, speed effects never bend it, and compiled times
// keep BPM changes as real distances.
struct EditorTimeAxis
{
    // A normal note is the game's 128-pixel head at the lane's scale.
    static constexpr float NormalHeadDiameter = 128 * editor_layout::LaneScale;
    float judgeX{};
    long double pixelsPerMicrosecond{};
    RhythmTime now{};

    // At Base BPM and spacing 1, a sixteenth note spans 85% of a normal head.
    [[nodiscard]] static EditorTimeAxis Realtime(const MusicalTimeline &timeline, double spacing, float judgeX,
                                                 double nowMilliseconds)
    {
        const long double pixelsPerWholeNote = NormalHeadDiameter * 16.0L * .85L * spacing;
        return {judgeX, timeline.BaseBpm() / 240'000'000.0L * pixelsPerWholeNote, Microseconds(nowMilliseconds)};
    }
    [[nodiscard]] float X(RhythmTime time) const noexcept
    {
        const long double elapsed = static_cast<long double>(time.count()) - now.count();
        return judgeX + static_cast<float>(elapsed * pixelsPerMicrosecond);
    }
    [[nodiscard]] RhythmTime TimeAt(float x) const noexcept
    {
        const long double value = now.count() + (static_cast<long double>(x) - judgeX) / pixelsPerMicrosecond;
        using Rep = RhythmTime::rep;
        if (value <= static_cast<long double>(std::numeric_limits<Rep>::min()))
            return RhythmTime::min();
        if (value >= static_cast<long double>(std::numeric_limits<Rep>::max()))
            return RhythmTime::max();
        return RhythmTime{static_cast<Rep>(value)};
    }
};
