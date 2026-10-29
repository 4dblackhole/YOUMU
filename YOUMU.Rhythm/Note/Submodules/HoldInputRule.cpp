#include "Note/Submodules/HoldInputRule.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

HoldInputRule::HoldInputRule(const NoteAction requiredAction, const RhythmTime endTime,
                             std::vector<RhythmTime> tickTimes,
                             const JudgementGrade startMaximumGrade,
                             const std::optional<NoteAction> rejectedAction)
    : requiredAction_(requiredAction), endTime_(endTime), tickTimes_(std::move(tickTimes)),
      startMaximumGrade_(startMaximumGrade), rejectedAction_(rejectedAction)
{
    if (rejectedAction_ == requiredAction_)
        throw std::invalid_argument("A hold cannot reject its required action.");
    std::ranges::sort(tickTimes_);
}

void HoldInputRule::Reset() noexcept
{
    state_ = NoteState::Pending;
    nextTickIndex_ = 0;
    started_ = false;
    held_ = false;
    rejectedHeld_ = false;
    heldPhysicalKeys_.clear();
    rejectedPhysicalKeys_.clear();
}

NoteState HoldInputRule::State() const noexcept
{
    return state_;
}

bool HoldInputRule::CanAccept(const NoteRuleContext &, const RhythmInputEvent &input,
                              const JudgementResult &judgement) const noexcept
{
    if (IsTerminal(state_) || input.time > endTime_)
    {
        return false;
    }
    if (!started_)
    {
        return input.action == requiredAction_ && input.edge == InputEdge::Pressed &&
               IsAtLeastAsAccurateAs(judgement.grade, startMaximumGrade_);
    }
    return input.action == requiredAction_ || rejectedAction_ == input.action;
}

void HoldInputRule::ApplyHeldInput(const RhythmInputEvent &input)
{
    const bool rejected = rejectedAction_ == input.action;
    if (!rejected && input.action != requiredAction_)
        return;
    auto &keys = rejected ? rejectedPhysicalKeys_ : heldPhysicalKeys_;
    if (input.edge == InputEdge::Pressed)
        keys.insert(input.physicalKey);
    else
        keys.erase(input.physicalKey);
    (rejected ? rejectedHeld_ : held_) = input.actionHeldAfter.value_or(!keys.empty());
}

void HoldInputRule::ProcessInput(const NoteRuleContext &context, const RhythmInputEvent &input,
                                 const JudgementResult &judgement, NoteProcessResult &output)
{
    if (!IsTerminal(state_))
    {
        // Earlier ticks use the previous colors. Exact-time ticks use the
        // state after this edge, for both the required and rejected action.
        AdvanceTicks(context, input.time - RhythmDuration{1}, output);
    }
    const bool accepted = CanAccept(context, input, judgement);
    // Both colors affect future ticks, including a wrong color held before the head.
    ApplyHeldInput(input);
    if (!accepted)
    {
        output.events.push_back(MakeEvent(context, NoteEventType::InputRejected, state_, state_,
                                          judgement, input.time));
        return;
    }

    const NoteState before = state_;
    if (input.edge == InputEdge::Pressed)
    {
        state_ = held_ && !rejectedHeld_ ? NoteState::Holding : NoteState::Active;
        if (!started_)
        {
            started_ = true;
            output.events.push_back(MakeEvent(context, NoteEventType::HitAccepted, before,
                                              state_, judgement, input.time));
            output.events.push_back(MakeEvent(context, NoteEventType::HoldStarted, before,
                                              state_, judgement, input.time));
        }
        AdvanceTicks(context, input.time, output);
        return;
    }

    state_ = held_ && !rejectedHeld_ ? NoteState::Holding : NoteState::Active;
    output.events.push_back(
        MakeEvent(context, NoteEventType::HoldReleased, before, state_, judgement, input.time));
    AdvanceTicks(context, input.time, output);
}

void HoldInputRule::Update(const NoteRuleContext &context, const NoteUpdateContext &update,
                           NoteProcessResult &output)
{
    if (IsTerminal(state_))
    {
        return;
    }
    rejectedHeld_ = rejectedAction_ && update.IsHeld(*rejectedAction_);
    if (started_)
    {
        held_ = update.IsHeld(requiredAction_);
        state_ = held_ && !rejectedHeld_ ? NoteState::Holding : NoteState::Active;
    }

    AdvanceTicks(context, update.time, output);

    if (update.time < endTime_)
    {
        return;
    }
    const NoteState before = state_;
    state_ = started_ ? NoteState::Completed : NoteState::Missed;
    output.events.push_back(MakeEvent(
        context,
        state_ == NoteState::Completed ? NoteEventType::Completed : NoteEventType::Missed,
        before, state_,
        state_ == NoteState::Completed ? JudgementResult{JudgementGrade::Unjudged, {}, 1.0}
                                       : MissJudgement(context, update.time),
        endTime_));
}

void HoldInputRule::MarkMissed(const NoteRuleContext &context, const RhythmTime time,
                               NoteProcessResult &output)
{
    if (IsTerminal(state_))
    {
        return;
    }
    const NoteState before = state_;
    state_ = NoteState::Missed;
    output.events.push_back(MakeEvent(context, NoteEventType::Missed, before, state_,
                                      MissJudgement(context, time), time));
}

void HoldInputRule::AdvanceTicks(const NoteRuleContext &context, const RhythmTime time,
                                 NoteProcessResult &output)
{
    while (nextTickIndex_ < tickTimes_.size() && tickTimes_[nextTickIndex_] <= time)
    {
        const std::size_t index = nextTickIndex_++;
        const bool rejected = started_ && rejectedHeld_;
        const bool success = started_ && held_ && !rejected;
        output.events.push_back(
            MakeEvent(context, rejected ? NoteEventType::TickRejected
                                       : success ? NoteEventType::TickAccepted : NoteEventType::TickMissed,
                      state_, state_, {JudgementGrade::Unjudged, {}, success ? 1.0 : 0.0},
                      tickTimes_[index], 0, index));
    }
}

RhythmTime HoldInputRule::ExpireTime(const NoteRuleContext &) const noexcept
{
    return endTime_;
}
