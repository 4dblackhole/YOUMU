#include "Lane/Lane.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

Lane::Lane(const std::size_t id) noexcept
    : id_(id)
{
}

std::size_t Lane::Id() const noexcept
{
    return id_;
}

void Lane::AddNote(std::unique_ptr<INote> note)
{
    if (finalized_)
    {
        throw std::logic_error(
            "A finalized lane cannot accept additional notes.");
    }
    if (note == nullptr)
    {
        throw std::invalid_argument("A lane cannot own a null note.");
    }
    notes_.push_back(std::move(note));
}

void Lane::Finalize()
{
    std::ranges::stable_sort(
        notes_,
        [](const std::unique_ptr<INote>& left,
           const std::unique_ptr<INote>& right)
        {
            if (left->Timing() != right->Timing())
            {
                return left->Timing() < right->Timing();
            }
            return left->Id() < right->Id();
        });
    latestExpireThrough_.clear();
    latestExpireThrough_.reserve(notes_.size());
    maximumGoodHalfWindow_ = RhythmDuration::zero();
    for (const std::unique_ptr<INote>& note : notes_)
    {
        const RhythmTime expireTime = note->ExpireTime();
        maximumGoodHalfWindow_ = std::max(maximumGoodHalfWindow_,
                                         note->Profile().HalfWindow(JudgementGrade::Good));
        latestExpireThrough_.push_back(
            latestExpireThrough_.empty()
                ? expireTime
                : std::max(latestExpireThrough_.back(), expireTime));
    }
    finalized_ = true;
    Reset();
}

void Lane::Reset() noexcept
{
    for (const std::unique_ptr<INote>& note : notes_)
    {
        note->Reset();
    }
    currentIndex_ = 0;
}

NoteProcessResult Lane::ProcessInput(const RhythmInputEvent& input,
                                     const std::span<const NoteAction> heldBeforeInput)
{
    if (!finalized_)
    {
        throw std::logic_error(
            "Finalize the lane before processing input.");
    }

    NoteProcessResult result;
    ResolveFocus(input.time,
                 input.time == RhythmTime::min() ? input.time : input.time - RhythmDuration{1},
                 heldBeforeInput, result);
    INote* current = CurrentNote();
    if (current == nullptr)
    {
        return result;
    }

    const JudgementResult currentJudgement = current->Preview(input);

    // An early Bad is reported as an attempt, but it deliberately keeps
    // focus on the same note so one premature press cannot start a bad
    // judgement cascade through a dense pattern.
    if (current->Accuracy().target.kind != NoteAccuracyKind::Ticks &&
        currentJudgement.grade == JudgementGrade::Bad &&
        currentJudgement.signedError < RhythmDuration::zero())
    {
        result.events.push_back(MakeEarlyBadEvent(
            *current,
            currentJudgement,
            input.time));
        return result;
    }

    result.Append(current->ProcessInput(input));
    AdvancePastTerminalNotes();
    return result;
}

NoteProcessResult Lane::Update(
    const RhythmTime time,
    const std::span<const NoteAction> heldActions)
{
    if (!finalized_)
    {
        throw std::logic_error("Finalize the lane before updating it.");
    }

    NoteProcessResult result;
    ResolveFocus(time, time, heldActions, result);
    return result;
}

std::size_t Lane::GoodFocusIndex(const RhythmTime time) const noexcept
{
    const INote *current = CurrentNote();
    if (!current || current->Accuracy().target.kind != NoteAccuracyKind::TimingHits)
        return currentIndex_;
    const auto judgement = current->Profile().Evaluate(current->Timing(), time);
    if (judgement.grade != JudgementGrade::Bad || judgement.signedError <= RhythmDuration::zero())
        return currentIndex_;

    const auto lower = time < RhythmTime::min() + maximumGoodHalfWindow_
                           ? RhythmTime::min() : time - maximumGoodHalfWindow_;
    const auto upper = time > RhythmTime::max() - maximumGoodHalfWindow_
                           ? RhythmTime::max() : time + maximumGoodHalfWindow_;
    const auto first = notes_.begin() + static_cast<std::ptrdiff_t>(currentIndex_ + 1);
    const auto candidate = std::lower_bound(first, notes_.end(), lower,
        [](const auto &note, RhythmTime boundary) { return note->Timing() < boundary; });
    // A live long-note body keeps its existing focus policy through the tail.
    for (auto note = first; note != candidate; ++note)
        if ((*note)->Accuracy().target.kind != NoteAccuracyKind::TimingHits && time <= (*note)->ExpireTime())
            return currentIndex_;
    for (auto note = candidate; note != notes_.end() && (*note)->Timing() <= upper; ++note)
    {
        if ((*note)->Profile().IsWithin(JudgementGrade::Good, (*note)->Timing(), time))
            return static_cast<std::size_t>(note - notes_.begin());
        if ((*note)->Accuracy().target.kind != NoteAccuracyKind::TimingHits && time <= (*note)->ExpireTime())
            break;
    }
    return currentIndex_;
}

void Lane::ResolveFocus(const RhythmTime time, const RhythmTime updateTime,
                        const std::span<const NoteAction> heldActions, NoteProcessResult &result)
{
    AdvancePastTerminalNotes();
    while (INote* current = CurrentNote())
    {
        // Inputs use the pre-edge time/state so an exact-time hold tick is
        // still evaluated by the rule after the incoming key transition.
        result.Append(current->Update({updateTime, heldActions}));
        if (current->State() == NoteState::Completed ||
            current->State() == NoteState::Missed)
        {
            ++currentIndex_;
            continue;
        }
        const auto target = GoodFocusIndex(time);
        if (time > current->ExpireTime() || target > currentIndex_)
        {
            result.Append(current->MarkMissed(time));
            ++currentIndex_;
            continue;
        }
        break;
    }
}

bool Lane::Empty() const noexcept
{
    return notes_.empty();
}

std::size_t Lane::CurrentIndex() const noexcept
{
    return currentIndex_;
}

INote* Lane::CurrentNote() noexcept
{
    return currentIndex_ < notes_.size()
        ? notes_[currentIndex_].get()
        : nullptr;
}

const INote* Lane::CurrentNote() const noexcept
{
    return const_cast<Lane*>(this)->CurrentNote();
}

const std::vector<std::unique_ptr<INote>>& Lane::Notes() const noexcept
{
    return notes_;
}

void Lane::AdvancePastTerminalNotes() noexcept
{
    while (currentIndex_ < notes_.size())
    {
        const NoteState state = notes_[currentIndex_]->State();
        if (state != NoteState::Completed && state != NoteState::Missed)
        {
            break;
        }
        ++currentIndex_;
    }
}

NoteEvent Lane::MakeEarlyBadEvent(
    const INote& note,
    const JudgementResult& judgement,
    const RhythmTime time) const noexcept
{
    return NoteEvent{
        note.Id(),
        NoteEventType::InputRejected,
        note.State(),
        note.State(),
        judgement,
        time};
}
