#pragma once

#include "Note/Submodules/INote.h"
#include "Note/Submodules/NoteSoundPolicy.h"
#include <memory>

class RuleBasedNote final : public INote
{
  public:
    RuleBasedNote(NoteId id, RhythmTime timing, std::shared_ptr<const AccuracyRange> profile,
                  std::unique_ptr<INoteRule> rule,
                  std::shared_ptr<const INoteSoundPolicy> soundPolicy = {});
    ~RuleBasedNote() override;
    [[nodiscard]] const NoteAccuracy &Accuracy() const noexcept override;

    [[nodiscard]] NoteId Id() const noexcept override;
    [[nodiscard]] RhythmTime Timing() const noexcept override;
    [[nodiscard]] RhythmTime ExpireTime() const noexcept override;
    [[nodiscard]] NoteState State() const noexcept override;
    [[nodiscard]] std::optional<NoteProgress> Progress() const noexcept override;
    [[nodiscard]] std::size_t ProcessedTickCount() const noexcept override;
#if defined(_DEBUG)
    [[nodiscard]] std::wstring DebugText() const override;
#endif
    [[nodiscard]] const AccuracyRange &Profile() const noexcept override;
    [[nodiscard]] JudgementResult Preview(
        const RhythmInputEvent &input) const noexcept override;
    [[nodiscard]] bool CanAccept(const RhythmInputEvent &input) const noexcept override;
    [[nodiscard]] NoteProcessResult ProcessInput(const RhythmInputEvent &input) override;
    [[nodiscard]] NoteProcessResult Update(const NoteUpdateContext &update) override;
    [[nodiscard]] NoteProcessResult MarkMissed(RhythmTime time) override;
    void Reset() noexcept override;
    void RecordEarlyBad() noexcept override;
    [[nodiscard]] bool IsWrongPress(const RhythmInputEvent &input) const noexcept override;

  private:
    [[nodiscard]] NoteRuleContext Context() const noexcept;
    [[nodiscard]] bool JudgesTiming() const noexcept;
    void MarkTimingOutcome(NoteProcessResult &result, bool judgedTiming, bool retrying);
    void AppendAudioCues(NoteProcessResult &result) const;
    void AccumulateAccuracy(NoteProcessResult &result);

    NoteId id_{};
    RhythmTime timing_{};
    std::shared_ptr<const AccuracyRange> profile_;
    std::unique_ptr<INoteRule> rule_;
    std::shared_ptr<const INoteSoundPolicy> soundPolicy_;
    NoteAccuracy accuracy_;
    bool poorOnTimingTarget_{};
    // A late BAD or an early BAD on the timing target: the note settles as a BAD (accuracy zero).
    bool badOnTimingTarget_{};
    // The last rejection waiting for a retry of the same hit: an early BAD or a POOR.
    bool retryAfterEarlyBad_{}, retryAfterPoor_{};
};
