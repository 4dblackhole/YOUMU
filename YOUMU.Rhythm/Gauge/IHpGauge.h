#pragma once
#include "Note/Submodules/NoteTypes.h"

struct HpGaugeState
{
    double percent{};
    bool applicable{};
    bool passed{};
};

// Scene owns gauges. A play session borrows them and delivers ordered judgements.
class IHpGauge
{
  public:
    virtual ~IHpGauge() = default;
    virtual void Reset() noexcept = 0;
    virtual void Consume(const NoteProcessResult &result, bool keyPressed) = 0;
    virtual HpGaugeState State() const noexcept = 0;
};
