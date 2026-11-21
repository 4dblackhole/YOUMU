#pragma once
#include "Gauge/HpGaugeKind.h"
#include "Judgement/JudgeLevel.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

// Value snapshot for one attempt. No option mutates a source chart.
struct PlayOptions
{
    enum class ScrollUnit
    {
        Multiplier,
        Bpm
    };
    ScrollUnit scrollUnit{ScrollUnit::Multiplier};
    double scrollSpeed{1.0};
    bool ignoreScrollEffects{};
    bool metronome{};
    bool mirror{};
    bool random{};
    std::uint64_t randomSeed{};
    double randomAttemptProbability{1.0};
    // Accelerate is a multiplier (0.5..2), or with the Bpm unit a target BPM that each chart turns into a
    // multiplier against its base BPM (AccelerateMultiplier). The play session keeps only the multiplier.
    ScrollUnit accelerateUnit{ScrollUnit::Multiplier};
    double accelerate{1.0};
    static constexpr double MinimumAccelerate = 0.5, MaximumAccelerate = 2.0;
    bool preserveMusicPitch{};
    bool preserveHitSoundPitch{true};
    std::optional<JudgeLevel> judgeLevel;
    HpGaugeKind hpGauge{HpGaugeKind::Default};

    void Validate() const
    {
        const bool knownUnit = scrollUnit == ScrollUnit::Multiplier || scrollUnit == ScrollUnit::Bpm;
        const bool validScroll = knownUnit && std::isfinite(scrollSpeed) && scrollSpeed > 0;
        const bool validProbability =
            std::isfinite(randomAttemptProbability) && randomAttemptProbability >= 0 && randomAttemptProbability <= 1;
        const bool bpmAccelerate = accelerateUnit == ScrollUnit::Bpm;
        const bool knownAccelerateUnit = bpmAccelerate || accelerateUnit == ScrollUnit::Multiplier;
        const bool multiplierInRange = accelerate >= MinimumAccelerate && accelerate <= MaximumAccelerate;
        const bool accelerateInRange = bpmAccelerate ? accelerate > 0 : multiplierInRange;
        const bool validAccelerate = knownAccelerateUnit && std::isfinite(accelerate) && accelerateInRange;
        if (!validScroll || !validProbability || !validAccelerate)
            throw std::invalid_argument("Invalid play options: positive scroll speed, probability 0..1, Accelerate "
                                        "0.5..2 (or a positive target BPM) required.");
        if (FindHpGauge(hpGauge) == nullptr)
            throw std::invalid_argument("Invalid play options: unknown HP gauge.");
    }
    // The Accelerate multiplier for a chart. A target BPM is divided by the chart's base BPM and kept within
    // 0.5..2x, the range a multiplier may take.
    double AccelerateMultiplier(double baseBpm) const
    {
        Validate();
        if (accelerateUnit != ScrollUnit::Bpm)
            return accelerate;
        const double ratio = accelerate / baseBpm;
        if (!std::isfinite(ratio) || ratio <= 0)
            throw std::invalid_argument("Accelerate BPM cannot be represented for this chart.");
        return std::clamp(ratio, MinimumAccelerate, MaximumAccelerate);
    }
    double ScrollMultiplier(double baseBpm) const
    {
        Validate();
        const double value = scrollUnit == ScrollUnit::Bpm ? scrollSpeed / baseBpm : scrollSpeed;
        if (!std::isfinite(value) || value <= 0)
            throw std::invalid_argument("Scroll speed cannot be represented for this chart.");
        return value;
    }
};
