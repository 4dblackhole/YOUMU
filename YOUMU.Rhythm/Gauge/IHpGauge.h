#pragma once
#include "Note/Submodules/NoteTypes.h"
#include <span>

struct HpHistoryPoint
{
    RhythmTime time{};
    double percent{};
};

struct HpGaugeState
{
    double percent{};
    bool applicable{};
    bool passed{};
    // HP at or above this percentage clears. Each gauge kind states its own
    // threshold so the play HUD and the result graph mark the same line.
    double clearPercent{60.0};
};

// Scene owns gauges. A play session borrows them and delivers ordered judgements.
class IHpGauge
{
  public:
    virtual ~IHpGauge() = default;
    virtual void Reset() noexcept = 0;
    virtual void Consume(const NoteProcessResult &result, bool keyPressed) = 0;
    virtual HpGaugeState State() const noexcept = 0;
    // Borrowed until the next Consume/Reset. Null means history is unavailable;
    // an empty recording is valid (the default gauge starts at zero).
    virtual std::optional<std::span<const HpHistoryPoint>> History() const noexcept { return std::nullopt; }
};
