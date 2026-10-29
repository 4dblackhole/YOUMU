#pragma once

#include "Note/Submodules/INoteRule.h"
#include <optional>
#include <unordered_set>
#include <vector>

class HoldInputRule final : public INoteRule
{
  public:
    [[nodiscard]] NoteAccuracyTarget AccuracyTarget() const noexcept override
    {
        return {NoteAccuracyKind::Hold, 1, tickTimes_.size()};
    }
    HoldInputRule(NoteAction requiredAction, RhythmTime endTime,
                  std::vector<RhythmTime> tickTimes,
                  JudgementGrade startMaximumGrade = JudgementGrade::Good,
                  std::optional<NoteAction> rejectedAction = std::nullopt);

    void Reset() noexcept override;
    [[nodiscard]] NoteState State() const noexcept override;
    [[nodiscard]] bool CanAccept(const NoteRuleContext &context, const RhythmInputEvent &input,
                                 const JudgementResult &judgement) const noexcept override;
    void ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                      const JudgementResult &judgement, NoteProcessResult &output) override;
    void Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                NoteProcessResult &output) override;
    void MarkMissed(const NoteRuleContext &context, RhythmTime time,
                    NoteProcessResult &output) override;
    [[nodiscard]] RhythmTime ExpireTime(const NoteRuleContext &context) const noexcept override;
    [[nodiscard]] std::size_t ProcessedTickCount() const noexcept override
    {
        return nextTickIndex_;
    }

  private:
    NoteAction requiredAction_{};
    RhythmTime endTime_{};
    std::vector<RhythmTime> tickTimes_;
    JudgementGrade startMaximumGrade_{JudgementGrade::Good};
    NoteState state_{NoteState::Pending};
    std::size_t nextTickIndex_{};
    bool started_{};
    bool held_{};
    std::optional<NoteAction> rejectedAction_;
    bool rejectedHeld_{};
    std::unordered_set<PhysicalKey> heldPhysicalKeys_;
    std::unordered_set<PhysicalKey> rejectedPhysicalKeys_;
    void ApplyHeldInput(const RhythmInputEvent &input);
    void AdvanceTicks(const NoteRuleContext &context, RhythmTime time,
                      NoteProcessResult &output);
};
