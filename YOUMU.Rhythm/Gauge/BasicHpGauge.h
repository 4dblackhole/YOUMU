#pragma once
#include "Gauge/HpLevel.h"
#include "Gauge/IHpGauge.h"
#include <map>
#include <span>

struct HpNoteDefinition
{
    NoteId id{};
    NoteAccuracyTarget target;
    RhythmDuration perfectWindow{}, greatWindow{};
};

class BasicHpGauge final : public IHpGauge
{
  public:
    BasicHpGauge(HpLevel level, std::span<const HpNoteDefinition> notes);
    void Reset() noexcept override;
    void Consume(const NoteProcessResult &result, bool keyPressed) override;
    HpGaugeState State() const noexcept override;
    std::optional<std::span<const HpHistoryPoint>> History() const noexcept override { return std::span{history_}; }

  private:
    struct NoteStateData
    {
        HpNoteDefinition definition;
        std::vector<double> hits;
        std::vector<bool> resolvedTicks;
        std::size_t accepted{};
        bool poor{}, headSettled{}, finalized{};
    };
    double TimingRate(const NoteStateData &, const JudgementResult &) const noexcept;
    void ConsumeEvent(NoteStateData &, const NoteEvent &, bool keyPressed);
    void Apply(double weightedRate) noexcept;
    std::map<NoteId, NoteStateData> notes_;
    HpLevel level_;
    double increment_{}, percent_{};
    std::vector<HpHistoryPoint> history_;
};
