#include "Note/Submodules/TickRollInputRule.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

TickRollInputRule::TickRollInputRule(std::vector<NoteAction> acceptedActions,
                                     const RhythmTime endTime,
                                     std::vector<RhythmTime> tickTimes,
                                     const JudgementGrade maximumGrade)
    : acceptedActions_(std::move(acceptedActions)), endTime_(endTime),
      tickTimes_(std::move(tickTimes)), maximumGrade_(maximumGrade)
{
    if (acceptedActions_.empty())
    {
        throw std::invalid_argument("A tick roll requires at least one input action.");
    }
    std::ranges::sort(tickTimes_);
}

void TickRollInputRule::Reset() noexcept
{
    state_ = NoteState::Active;
    nextTickIndex_ = 0;
}

NoteState TickRollInputRule::State() const noexcept
{
    return state_;
}

bool TickRollInputRule::CanAccept(const NoteRuleContext &context, const RhythmInputEvent &input,
                                  const JudgementResult &) const noexcept
{
    if (IsTerminal(state_) || input.edge != InputEdge::Pressed)
    {
        return false;
    }
    if (input.time > FinalTime(context) || std::ranges::find(acceptedActions_, input.action) == acceptedActions_.end())
    {
        return false;
    }

    const RhythmDuration halfWindow = context.judgementProfile.HalfWindow(maximumGrade_);
    std::size_t candidate = nextTickIndex_;
    while (candidate < tickTimes_.size() && input.time > tickTimes_[candidate] + halfWindow)
    {
        ++candidate;
    }
    return candidate < tickTimes_.size() &&
           context.judgementProfile.IsWithin(maximumGrade_, tickTimes_[candidate], input.time);
}

void TickRollInputRule::ProcessInput(const NoteRuleContext &context,
                                     const RhythmInputEvent &input,
                                     const JudgementResult &judgement,
                                     NoteProcessResult &output)
{
    AppendExpiredTicks(context, input.time, output);
    if (!CanAccept(context, input, judgement))
    {
        output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                          {JudgementGrade::Unjudged, {}, 0.0}, input.time, 0,
                                          nextTickIndex_));
        return;
    }

    const std::size_t acceptedIndex = nextTickIndex_++;
    output.events.push_back(
        MakeEvent(context, NoteEventType::TickAccepted, state_, state_,
                  {JudgementGrade::Unjudged, input.time - tickTimes_[acceptedIndex], 1.0},
                  input.time, 0, acceptedIndex));
    if (AllTicksSettled())
        Complete(context, input.time, output);
}

void TickRollInputRule::Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                               NoteProcessResult &output)
{
    if (IsTerminal(state_))
    {
        return;
    }
    AppendExpiredTicks(context, update.time, output);
    // The roll keeps the lane's focus only while a tick can still be hit (user decision): once every tick is
    // settled it completes at once, even before its tail, and the next note takes the following input.
    if (AllTicksSettled())
    {
        Complete(context, update.time, output);
        return;
    }
    // A tick still waiting keeps the note open until the late edge of its window, even after the tail (user
    // decision). At that edge the note settles, matching the lane's expiry (time > ExpireTime) one step later.
    if (update.time < FinalTime(context))
    {
        return;
    }
    while (nextTickIndex_ < tickTimes_.size())
    {
        const std::size_t missedIndex = nextTickIndex_++;
        output.events.push_back(MakeEvent(context, NoteEventType::TickMissed, state_, state_,
                                          MissJudgement(context, tickTimes_[missedIndex]),
                                          tickTimes_[missedIndex], 0, missedIndex));
    }
    Complete(context, update.time, output);
}

bool TickRollInputRule::AllTicksSettled() const noexcept
{
    // A roll without ticks has nothing to settle early and still ends at its tail.
    return !tickTimes_.empty() && nextTickIndex_ == tickTimes_.size();
}

void TickRollInputRule::Complete(const NoteRuleContext &context, const RhythmTime time, NoteProcessResult &output)
{
    const NoteState before = state_;
    state_ = NoteState::Completed;
    output.events.push_back(MakeEvent(context, NoteEventType::Completed, before, state_,
                                      {JudgementGrade::Unjudged, {}, 1.0}, time, 0, nextTickIndex_));
}

RhythmTime TickRollInputRule::FinalTime(const NoteRuleContext &context) const noexcept
{
    // The last tick takes input through the late edge of its window in this note's judgement profile
    // (the judge level and any syncopation range), so the note never ends before that edge.
    if (tickTimes_.empty())
        return endTime_;
    return std::max(endTime_, tickTimes_.back() + context.judgementProfile.HalfWindow(maximumGrade_));
}

void TickRollInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                                   NoteProcessResult &output)
{
    MarkRuleMissed(context, time, state_, output, 0, nextTickIndex_);
}

RhythmTime TickRollInputRule::ExpireTime(const NoteRuleContext &context) const noexcept
{
    // Fixed for the note's lifetime so lane indexes stay valid; a roll whose ticks are all settled completes
    // earlier, when the last one is settled.
    return FinalTime(context);
}

void TickRollInputRule::AppendExpiredTicks(const NoteRuleContext &context,
                                           const RhythmTime time, NoteProcessResult &output)
{
    const RhythmDuration halfWindow = context.judgementProfile.HalfWindow(maximumGrade_);
    while (nextTickIndex_ < tickTimes_.size() && time > tickTimes_[nextTickIndex_] + halfWindow)
    {
        const std::size_t missedIndex = nextTickIndex_++;
        output.events.push_back(MakeEvent(context, NoteEventType::TickMissed, state_, state_,
                                          MissJudgement(context, tickTimes_[missedIndex]),
                                          tickTimes_[missedIndex], 0, missedIndex));
    }
}
