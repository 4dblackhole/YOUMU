#pragma once
#include "Gauge/HpGaugeKind.h"
#include "Judgement/JudgeLevel.h"
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
    double accelerate{1.0};
    bool preserveMusicPitch{true};
    bool preserveHitSoundPitch{true};
    std::optional<JudgeLevel> judgeLevel;
    HpGaugeKind hpGauge{HpGaugeKind::Default};

    void Validate() const
    {
        const bool knownUnit = scrollUnit == ScrollUnit::Multiplier || scrollUnit == ScrollUnit::Bpm;
        const bool validScroll = knownUnit && std::isfinite(scrollSpeed) && scrollSpeed > 0;
        const bool validProbability =
            std::isfinite(randomAttemptProbability) && randomAttemptProbability >= 0 && randomAttemptProbability <= 1;
        const bool validAccelerate = std::isfinite(accelerate) && accelerate >= 0.5 && accelerate <= 2.0;
        if (!validScroll || !validProbability || !validAccelerate)
            throw std::invalid_argument(
                "Invalid play options: positive scroll speed, probability 0..1, Accelerate 0.5..2 required.");
        if (FindHpGauge(hpGauge) == nullptr)
            throw std::invalid_argument("Invalid play options: unknown HP gauge.");
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
