#include "Taiko/Results/TaikoResultCollector.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace
{
[[nodiscard]] bool CountsAcceptedTicks(TaikoNoteType type) noexcept
{
    return type == TaikoNoteType::TickRoll || type == TaikoNoteType::BigTickRoll || type == TaikoNoteType::Buzz;
}
// A long note whose timing head settled as a BAD (late BAD, early BAD or POOR), or that has body ticks and took
// none of them, breaks the combo. Only a Buzz has both a timing head and body ticks; rolls have neither.
[[nodiscard]] bool LongNoteFailed(const NoteAccuracy &accuracy) noexcept
{
    if (accuracy.rejectedTiming)
        return true;
    return accuracy.target.ticks != 0 && accuracy.acceptedTicks == 0;
}
[[nodiscard]] bool HeadAlreadyCountedBad(const NoteAccuracy &accuracy, bool retired) noexcept
{
    return !retired && accuracy.rejectedTiming && accuracy.acceptedHits != 0;
}
} // namespace

TaikoResultGroup ResultGroup(TaikoNoteType type) noexcept
{
    const auto *definition = FindTaikoNote(static_cast<int>(type));
    if (definition && definition->longNote)
        return TaikoResultGroup::Roll;
    return type == TaikoNoteType::Don || type == TaikoNoteType::Kat ? TaikoResultGroup::Normal
                                                                    : TaikoResultGroup::Big;
}
TaikoResultCollector::TaikoResultCollector(PlayResultSummary summary, std::size_t reserveNotes)
    : result_(std::make_unique<TaikoPlayResult>())
{
    result_->summary = std::move(summary);
    result_->notes.reserve(reserveNotes);
    result_->timingErrors.reserve(reserveNotes);
    result_->summary.inputs.reserve(reserveNotes);
    noteIndices_.reserve(reserveNotes);
    comboEligible_.reserve(reserveNotes);
    settlesAsBad_.reserve(reserveNotes);
    poorCounted_.reserve(reserveNotes);
}
void TaikoResultCollector::RegisterNote(NoteId id, TaikoNoteType type, RhythmTime time)
{
    if (!result_ || !noteIndices_.emplace(id, result_->notes.size()).second)
        throw std::logic_error("Result note IDs must be unique.");
    result_->notes.push_back({id, type, time});
    comboEligible_.push_back(true);
    settlesAsBad_.push_back(false);
    poorCounted_.push_back(false);
}
void TaikoResultCollector::RecordInput(const RhythmInputEvent &input)
{
    if (result_)
        result_->summary.inputs.push_back(input);
}
void TaikoResultCollector::RecordControl(PlayControlEvent event)
{
    if (!result_)
        return;
    event.inputIndex = result_->summary.inputs.size();
    result_->summary.controls.push_back(event);
    result_->summary.pauseMenuUsed |= event.pauseMenuShown;
}
void TaikoResultCollector::SetInitialTime(RhythmTime time) noexcept
{
    if (result_)
        result_->summary.initialTime = time;
}
void TaikoResultCollector::SetInputMapping(std::map<PhysicalKey, NoteAction> mapping)
{
    result_->summary.inputMapping = std::move(mapping);
}
void TaikoResultCollector::ConsumeEvent(const NoteEvent &event, bool keyPressed)
{
    const auto found = noteIndices_.find(event.noteId);
    if (found == noteIndices_.end() || result_->notes[found->second].processed)
        return;
    const auto grade = static_cast<std::size_t>(event.judgement.grade);
    if (event.type == NoteEventType::Completed || event.type == NoteEventType::Missed)
        result_->notes[found->second].finalState = event.stateAfter;
    if (event.type == NoteEventType::Missed && event.leftAtEarlyBad)
        settlesAsBad_[found->second] = true;
    if (event.type == NoteEventType::HitAccepted && grade < 5)
    {
        // A BAD recovery shows its own grade but counts as a BAD; the combo settles when the note finishes.
        // A hit on a note that already counted its POOR adds no count (user decision: POOR is a kind of BAD).
        const bool bad = event.recovery || event.judgement.grade == JudgementGrade::Bad;
        if (bad)
            comboEligible_[found->second] = false;
        if (poorCounted_[found->second])
            comboEligible_[found->second] = false;
        else
            ++result_->judgements[bad ? static_cast<std::size_t>(JudgementGrade::Bad) : grade];
        result_->timingErrors.push_back({event.noteId, event.hitIndex, event.eventTime, event.judgement.signedError,
                                         event.judgement.grade,
                                         AccuracyScore::FromRate(event.judgement.scoreRate)});
        timingErrorSum_ += event.judgement.signedError;
    }
    else if (keyPressed && IsPoor(event))
    {
        // A POOR counts now, once per note however many wrong presses follow; the note's combo settles when
        // it finishes (user decisions).
        if (!poorCounted_[found->second])
            ++result_->poorCount;
        poorCounted_[found->second] = true;
        comboEligible_[found->second] = false;
    }
    else if (keyPressed && IsEarlyBad(event))
        comboEligible_[found->second] = false; // An early BAD counts once the note settles.
    else if (event.type == NoteEventType::TickAccepted && CountsAcceptedTicks(result_->notes[found->second].type))
        ++result_->acceptedTicks;
    else if (event.type == NoteEventType::TickMissed || event.type == NoteEventType::TickRejected)
        ++result_->missedTicks; // A wrong colour held over a tick only leaves that tick unprocessed.
    // HoldStarted and Completed carry head judgements too; ignoring them
    // here prevents a single Buzz press from producing duplicate samples.
}
void TaikoResultCollector::RecordRetirement(const NoteAccuracy &accuracy)
{
    if (result_)
        FinalizeNote(accuracy, true);
}
void TaikoResultCollector::FinalizeNote(const NoteAccuracy &accuracy, bool retired)
{
    const auto found = noteIndices_.find(accuracy.noteId);
    if (found == noteIndices_.end() || !accuracy.finalized)
        return;
    auto &note = result_->notes[found->second];
    if (note.processed)
        return;
    note.processed = true;
    note.accuracy = retired ? AccuracyScore{} : accuracy.Score();
    if (retired)
        note.finalState = NoteState::Missed;
    note.target = accuracy.target;
    note.acceptedHits = accuracy.acceptedHits;
    note.acceptedTicks = accuracy.acceptedTicks;
    const bool longNote = ResultGroup(note.type) == TaikoResultGroup::Roll;
    // A long note that took any hit or tick counts as OK and keeps the combo; only one left
    // completely unprocessed is a MISS (user decision). Retire still turns it into a MISS.
    // A Buzz also needs a clean head and at least one body tick (user decision).
    const bool longNoteProgressed = !retired && accuracy.HasProgress() && !LongNoteFailed(accuracy);
    const bool shortNoteHit = note.finalState == NoteState::Completed && comboEligible_[found->second];
    const bool success = longNote ? longNoteProgressed : shortNoteHit;
    note.comboCounted = success;
    if (success)
    {
        ++combo_;
        result_->summary.maximumCombo = std::max(result_->summary.maximumCombo, combo_);
    }
    else
        combo_ = 0;
    if (longNote && success)
        ++result_->okCount;
    else if (note.finalState == NoteState::Missed && settlesAsBad_[found->second])
        ++result_->judgements[static_cast<std::size_t>(JudgementGrade::Bad)];
    else if (longNote && HeadAlreadyCountedBad(accuracy, retired))
        return; // A Buzz head BAD or recovery already counted as its BAD.
    else if (poorCounted_[found->second])
        return; // A note that took a POOR and was not hit after it ends as that POOR, not a MISS (user decision).
    else if (note.finalState == NoteState::Missed || longNote)
        ++result_->judgements[static_cast<std::size_t>(JudgementGrade::Miss)];
}
void TaikoResultCollector::Consume(const NoteProcessResult &result, bool keyPressed)
{
    if (!result_)
        return;
    std::unordered_map<NoteId, const NoteAccuracy *> completions;
    if (result.finalizedAccuracies.size() > 1)
    {
        completions.reserve(result.finalizedAccuracies.size());
        for (const auto &accuracy : result.finalizedAccuracies)
            completions.emplace(accuracy.noteId, &accuracy);
    }
    for (const auto &event : result.events)
    {
        ConsumeEvent(event, keyPressed);
        // Complete in event order: one update can finish earlier notes before
        // rejecting a later input. A late POOR must remain a combo break.
        if (event.type == NoteEventType::Completed || event.type == NoteEventType::Missed)
        {
            if (result.finalizedAccuracies.size() == 1 &&
                result.finalizedAccuracies.front().noteId == event.noteId)
                FinalizeNote(result.finalizedAccuracies.front());
            else if (const auto accuracy = completions.find(event.noteId); accuracy != completions.end())
                FinalizeNote(*accuracy->second);
        }
    }
    for (const auto &accuracy : result.finalizedAccuracies)
        FinalizeNote(accuracy);
}
void TaikoResultCollector::Reset() noexcept
{
    ResetJudgements();
    if (!result_)
        return;
    result_->summary.inputs.clear();
    result_->summary.controls.clear();
    result_->summary.pauseMenuUsed = false;
}
void TaikoResultCollector::ResetJudgements() noexcept
{
    if (!result_)
        return;
    for (auto &note : result_->notes)
    {
        note.processed = false;
        note.finalState = NoteState::Pending;
        note.accuracy = {};
        note.target = {};
        note.acceptedHits = note.acceptedTicks = 0;
        note.comboCounted = false;
    }
    result_->timingErrors.clear();
    timingErrorSum_ = {};
    result_->judgements = {};
    result_->okCount = result_->poorCount = result_->acceptedTicks = result_->missedTicks = 0;
    combo_ = result_->summary.maximumCombo = 0;
    std::fill(comboEligible_.begin(), comboEligible_.end(), true);
    std::fill(settlesAsBad_.begin(), settlesAsBad_.end(), false);
    std::fill(poorCounted_.begin(), poorCounted_.end(), false);
}
PlayStatistics TaikoResultCollector::Statistics() const noexcept
{
    return result_ ? PlayStatistics{combo_,
                                    result_->summary.maximumCombo,
                                    result_->judgements,
                                    result_->okCount,
                                    result_->poorCount,
                                    timingErrorSum_,
                                    result_->timingErrors.size()}
                   : PlayStatistics{};
}
void TaikoResultCollector::Summarize()
{
    auto &summary = result_->summary;
    summary.totalNotes = result_->notes.size();
    std::uint64_t totalSum{};
    std::array<std::uint64_t, 3> groupSums{};
    for (auto &group : result_->groups)
        group.succeeded = 0;
    for (const auto &note : result_->notes)
    {
        const auto index = static_cast<std::size_t>(ResultGroup(note.type));
        auto &group = result_->groups[index];
        ++group.total;
        if (note.comboCounted)
            ++group.succeeded;
        if (note.processed)
        {
            ++summary.processedNotes;
            ++group.processed;
            groupSums[index] += note.accuracy.Units();
            totalSum += note.accuracy.Units();
        }
    }
    summary.unprocessedNotes = summary.totalNotes - summary.processedNotes;
    summary.accuracy = AccuracyScore::Average(totalSum, summary.totalNotes);
    for (std::size_t i = 0; i < groupSums.size(); ++i)
        result_->groups[i].accuracy = AccuracyScore::Average(groupSums[i], result_->groups[i].total);

    // Population variance of real graded hits, using Welford accumulation.
    // Automatic misses and unfinished notes have no invented timing error.
    double mean{}, m2{};
    std::size_t count{};
    for (const auto &sample : result_->timingErrors)
    {
        const double value = static_cast<double>(sample.error.count()) / 1000.0;
        const double delta = value - mean;
        mean += delta / static_cast<double>(++count);
        m2 += delta * (value - mean);
    }
    if (count)
    {
        result_->meanErrorMilliseconds = mean;
        result_->varianceMillisecondsSquared = std::max(0.0, m2 / static_cast<double>(count));
        result_->standardDeviationMilliseconds = std::sqrt(*result_->varianceMillisecondsSquared);
    }
}
std::unique_ptr<IPlayResult> TaikoResultCollector::TakeResult(PlayEndReason reason, RhythmTime time,
                                                           std::uint32_t score,
                                                           std::vector<HpGaugeState> hp,
                                                           std::optional<std::vector<HpHistoryPoint>> hpHistory)
{
    if (!result_)
        throw std::logic_error("The play result was already transferred.");
    result_->summary.endReason = reason;
    result_->summary.endTime = time;
    result_->summary.score = score;
    result_->summary.hpGauges = std::move(hp);
    result_->summary.hpHistory = std::move(hpHistory);
    Summarize();
    return std::move(result_);
}
