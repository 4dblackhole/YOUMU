#pragma once
#include "Mode/Results/IPlayResultCollector.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include <unordered_map>

class TaikoResultCollector final : public IPlayResultCollector
{
  public:
    explicit TaikoResultCollector(PlayResultSummary summary, std::size_t reserveNotes);
    void RegisterNote(NoteId id, TaikoNoteType type, RhythmTime time);
    void RecordInput(const RhythmInputEvent &input) override;
    void RecordControl(PlayControlEvent event) override;
    void Consume(const NoteProcessResult &result, bool keyPressed) override;
    void RecordRetirement(const NoteAccuracy &accuracy) override;
    void SetInputMapping(std::map<PhysicalKey, NoteAction> mapping);
    void SetInitialTime(RhythmTime time) noexcept override;
    void Reset() noexcept override;
    void ResetJudgements() noexcept override;
    PlayStatistics Statistics() const noexcept override;
    std::unique_ptr<IPlayResult> TakeResult(PlayEndReason reason, RhythmTime time,
                                          std::uint32_t score,
                                          std::vector<HpGaugeState> hp = {},
                                          std::optional<std::vector<HpHistoryPoint>> hpHistory = std::nullopt) override;

  private:
    // The first BAD or POOR a normal or big note took; an early BAD is pending until something confirms it or the note
    // ends.
    enum class NoteFault : std::uint8_t
    {
        None,
        EarlyBad,
        Bad,
        Poor,
    };
    void ConsumeEvent(const NoteEvent &event, bool keyPressed);
    void FinalizeNote(const NoteAccuracy &accuracy, bool retired = false);
    void CountOnceAtSettle(const NoteAccuracy &accuracy, std::size_t index, bool retired);
    // Records a normal or big note's BAD or POOR; true when this POOR press is the one to count.
    [[nodiscard]] bool MarkNoteFault(std::size_t index, NoteFault fault);
    void Summarize();
    std::unique_ptr<TaikoPlayResult> result_;
    std::unordered_map<NoteId, std::size_t> noteIndices_;
    std::vector<bool> comboEligible_;
    // Notes missed while left at an early BAD settle as a BAD.
    std::vector<bool> settlesAsBad_;
    // The note took a POOR press (counted once, unless a BAD came first).
    std::vector<bool> poorCounted_;
    std::vector<NoteFault> noteFaults_;
    std::size_t combo_{};
    RhythmDuration timingErrorSum_{}; // Running sum of result_->timingErrors.
};
