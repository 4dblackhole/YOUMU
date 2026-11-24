#include "Gauge/BasicHpGauge.h"
#include "Gauge/HpGaugeKind.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <stdexcept>

BasicHpGauge::BasicHpGauge(HpLevel level, std::span<const HpNoteDefinition> notes) : level_(level)
{
    for (const auto &definition : notes)
    {
        NoteStateData state;
        state.definition = definition;
        state.hits.resize(definition.target.hits);
        state.resolvedTicks.resize(definition.target.ticks);
        if (!notes_.emplace(definition.id, std::move(state)).second)
            throw std::invalid_argument("HP note IDs must be unique.");
    }
    increment_ = notes_.empty() ? 0 : 120.0 / static_cast<double>(notes_.size());
    Reset();
}

void BasicHpGauge::Reset() noexcept
{
    percent_ = 0;
    history_.clear();
    for (auto &[id, state] : notes_)
    {
        std::ranges::fill(state.hits, -level_.Value());
        std::fill(state.resolvedTicks.begin(), state.resolvedTicks.end(), false);
        state.accepted = state.acceptedTicks = 0;
        state.rejected = state.headSettled = state.finalized = false;
    }
}

HpGaugeState BasicHpGauge::State() const noexcept
{
    // Fractional note/tick weights can place an exact 60% a few rounding
    // bits below the boundary. Keep that arithmetic noise out of clear status.
    constexpr double roundingTolerance = 1e-9;
    constexpr double clearPercent = FindHpGauge(HpGaugeKind::Default)->clearPercent;
    return {percent_, !notes_.empty(), !notes_.empty() && percent_ >= clearPercent - roundingTolerance,
            clearPercent};
}

double BasicHpGauge::TimingRate(const NoteStateData &state, const JudgementResult &judgement) const noexcept
{
    // MAX to GREAT recover fully; GOOD recovers by where it lands, 100% at the GREAT edge down to 0% at BAD.
    switch (judgement.grade)
    {
    case JudgementGrade::Max:
    case JudgementGrade::Perfect:
    case JudgementGrade::Great:
        return 1;
    case JudgementGrade::Good:
    {
        const auto great = static_cast<long double>(state.definition.greatWindow.count());
        const auto good = static_cast<long double>(state.definition.goodWindow.count());
        if (good <= great)
            return 0;
        const auto error = std::abs(static_cast<long double>(judgement.signedError.count()));
        return static_cast<double>(std::clamp((good - error) / (good - great), 0.0L, 1.0L));
    }
    default:
        return -level_.Value();
    }
}

// Every body tick passed and none was taken. Retire settles a note before its ticks pass and keeps HP as it was.
bool BasicHpGauge::BodyMissed(const NoteStateData &state) noexcept
{
    const bool tookNoTick = state.definition.target.ticks != 0 && state.acceptedTicks == 0;
    return tookNoTick && std::ranges::all_of(state.resolvedTicks, std::identity{});
}

void BasicHpGauge::Apply(double weightedRate) noexcept
{
    percent_ = std::clamp(percent_ + increment_ * weightedRate, 0.0, 100.0);
}

void BasicHpGauge::Consume(const NoteProcessResult &result, bool keyPressed)
{
    for (const auto &event : result.events)
        if (auto found = notes_.find(event.noteId); found != notes_.end() && !found->second.finalized)
        {
            ConsumeEvent(found->second, event, keyPressed);
            if (history_.empty() || history_.back().percent != percent_)
                history_.push_back({event.eventTime, percent_});
        }
}

void BasicHpGauge::ConsumeEvent(NoteStateData &state, const NoteEvent &event, bool keyPressed)
{
    const auto &target = state.definition.target;
    const bool timing = target.kind == NoteAccuracyKind::TimingHits;
    const bool hold = target.kind == NoteAccuracyKind::Hold;
    const double headWeight = target.ticks == 0 ? 1.0 : .5;
    const bool judgesTiming = timing || (hold && !state.headSettled);
    // A POOR or an early BAD (a rejected press within BAD) waits for the note to settle, then decreases.
    const bool rejectedGoodInput = event.type == NoteEventType::InputRejected &&
                                   IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good);
    const bool rejectedInput = rejectedGoodInput || IsEarlyBad(event);
    if (judgesTiming && keyPressed && rejectedInput)
        state.rejected = true;
    const bool badHit = event.type == NoteEventType::HitAccepted &&
                        (event.recovery || event.judgement.grade == JudgementGrade::Bad);
    if (judgesTiming && badHit)
        state.rejected = true;

    if (event.type == NoteEventType::HitAccepted)
    {
        if (timing && event.hitIndex < state.hits.size())
            state.hits[event.hitIndex] = TimingRate(state, event.judgement);
        else if (hold && !state.headSettled)
        {
            Apply(headWeight * (state.rejected ? -level_.Value() : TimingRate(state, event.judgement)));
            state.headSettled = true;
        }
        else if (target.kind == NoteAccuracyKind::HitCount)
            ++state.accepted;
    }
    const bool tickResult = event.type == NoteEventType::TickAccepted || event.type == NoteEventType::TickMissed ||
                            event.type == NoteEventType::TickRejected;
    const bool settlesTick = tickResult && (hold || target.kind == NoteAccuracyKind::Ticks);
    if (target.kind == NoteAccuracyKind::HitCount && event.type == NoteEventType::TickAccepted)
        ++state.accepted;
    else if (settlesTick && event.tickIndex < state.resolvedTicks.size() && !state.resolvedTicks[event.tickIndex])
    {
        state.resolvedTicks[event.tickIndex] = true;
        if (event.type == NoteEventType::TickAccepted)
        {
            ++state.acceptedTicks;
            Apply((hold ? .5 : 1.0) / static_cast<double>(target.ticks));
        }
    }
    if (event.type != NoteEventType::Completed && event.type != NoteEventType::Missed)
        return;

    // Timing pairs and count rolls settle once at logical completion. Ticks
    // already settled above retain their original order, including saturation.
    if (timing)
        Apply(state.rejected       ? -level_.Value()
              : state.hits.empty() ? 0
                                   : std::accumulate(state.hits.begin(), state.hits.end(), 0.0) /
                                         static_cast<double>(state.hits.size()));
    else if (target.kind == NoteAccuracyKind::HitCount && target.hits != 0)
        Apply(std::min(1.0, static_cast<double>(state.accepted) / static_cast<double>(target.hits)));
    else if (hold && !state.headSettled)
        Apply(-level_.Value() * headWeight);
    else if (hold && BodyMissed(state))
        Apply(-level_.Value() * (1.0 - headWeight)); // A Buzz body that took no tick decreases like a MISS.
    state.finalized = true;
}
