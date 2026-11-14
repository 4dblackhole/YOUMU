#include "Gauge/BasicHpGauge.h"
#include <algorithm>
#include <cmath>
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
        state.accepted = 0;
        state.poor = state.headSettled = state.finalized = false;
    }
}

HpGaugeState BasicHpGauge::State() const noexcept
{
    // Fractional note/tick weights can place an exact 60% a few rounding
    // bits below the boundary. Keep that arithmetic noise out of clear status.
    constexpr double roundingTolerance = 1e-9;
    constexpr double clearPercent = 60.0;
    return {percent_, !notes_.empty(), !notes_.empty() && percent_ >= clearPercent - roundingTolerance,
            clearPercent};
}

double BasicHpGauge::TimingRate(const NoteStateData &state, const JudgementResult &judgement) const noexcept
{
    switch (judgement.grade)
    {
    case JudgementGrade::Max:
    case JudgementGrade::Perfect:
        return 1;
    case JudgementGrade::Great:
    {
        const auto perfect = static_cast<long double>(state.definition.perfectWindow.count());
        const auto great = static_cast<long double>(state.definition.greatWindow.count());
        if (great <= perfect)
            return 0;
        const auto error = std::abs(static_cast<long double>(judgement.signedError.count()));
        return static_cast<double>(std::clamp((great - error) / (great - perfect), 0.0L, 1.0L));
    }
    case JudgementGrade::Good:
        return 0;
    default:
        return -level_.Value();
    }
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
    if ((timing || (hold && !state.headSettled)) && keyPressed && event.type == NoteEventType::InputRejected &&
        IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good))
        state.poor = true;

    if (event.type == NoteEventType::HitAccepted)
    {
        if (timing && event.hitIndex < state.hits.size())
            state.hits[event.hitIndex] = TimingRate(state, event.judgement);
        else if (hold && !state.headSettled)
        {
            Apply(headWeight * (state.poor ? -level_.Value() : TimingRate(state, event.judgement)));
            state.headSettled = true;
        }
        else if (target.kind == NoteAccuracyKind::HitCount)
            ++state.accepted;
    }
    const bool tickResult = event.type == NoteEventType::TickAccepted || event.type == NoteEventType::TickMissed ||
                            event.type == NoteEventType::TickRejected;
    if (target.kind == NoteAccuracyKind::HitCount && event.type == NoteEventType::TickAccepted)
        ++state.accepted;
    else if (tickResult && (hold || target.kind == NoteAccuracyKind::Ticks) &&
             event.tickIndex < state.resolvedTicks.size() && !state.resolvedTicks[event.tickIndex])
    {
        state.resolvedTicks[event.tickIndex] = true;
        if (event.type == NoteEventType::TickAccepted)
            Apply((hold ? .5 : 1.0) / static_cast<double>(target.ticks));
    }
    if (event.type != NoteEventType::Completed && event.type != NoteEventType::Missed)
        return;

    // Timing pairs and count rolls settle once at logical completion. Ticks
    // already settled above retain their original order, including saturation.
    if (timing)
        Apply(state.poor           ? -level_.Value()
              : state.hits.empty() ? 0
                                   : std::accumulate(state.hits.begin(), state.hits.end(), 0.0) /
                                         static_cast<double>(state.hits.size()));
    else if (target.kind == NoteAccuracyKind::HitCount && target.hits != 0)
        Apply(std::min(1.0, static_cast<double>(state.accepted) / static_cast<double>(target.hits)));
    else if (hold && !state.headSettled)
        Apply(-level_.Value() * headWeight);
    state.finalized = true;
}
