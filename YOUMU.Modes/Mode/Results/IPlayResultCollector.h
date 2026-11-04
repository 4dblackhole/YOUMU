#pragma once
#include "Mode/Results/IPlayResult.h"
#include <array>
#include <memory>

struct PlayStatistics
{
    std::size_t combo{};
    std::size_t maximumCombo{};
    // Same ordering as JudgementGrade: MAX, PERFECT, GREAT, GOOD, BAD, MISS.
    std::array<std::size_t, 6> judgements{};
    std::size_t okCount{};
    std::size_t poorCount{};
};

class IPlayResultCollector
{
  public:
    virtual ~IPlayResultCollector() = default;
    virtual void RecordInput(const RhythmInputEvent &input) = 0;
    virtual void RecordControl(PlayControlEvent event) = 0;
    virtual void Consume(const NoteProcessResult &result, bool keyPressed) = 0;
    virtual void RecordRetirement(const NoteAccuracy &accuracy) = 0;
    virtual void SetInitialTime(RhythmTime time) noexcept = 0;
    virtual void Reset() noexcept = 0;
    // Rewind replaces judgement data but retains the complete replay trace.
    virtual void ResetJudgements() noexcept = 0;
    virtual PlayStatistics Statistics() const noexcept = 0;
    virtual std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, RhythmTime time,
                                                  std::vector<HpGaugeState> hp = {}) = 0;
};
