#include "GameplayNoteVisibility.h"

#include <cmath>
#include <limits>
#include <numeric>

void GameplayNoteVisibility::Bind(const ScrollGear &gear)
{
    for (const auto &lane : gear.Lanes())
        for (const auto &note : lane->Notes())
            models_.emplace(note->Id(), note.get());
}

void GameplayNoteVisibility::Reset() noexcept
{
    models_.clear();
    visible_.clear();
    notes_.Assign({});
    measures_.Assign({});
    revision_ = 0;
    firstDistance_ = 0;
}

const INote *GameplayNoteVisibility::Find(NoteId id) const noexcept
{
    const auto found = models_.find(id);
    return found == models_.end() ? nullptr : found->second;
}

void GameplayNoteVisibility::Rebuild(const PlaySession &session, float pixelsPerWholeNote, float travelDistance)
{
    std::vector<IntervalIndex<long double, NoteId>::Entry> notes;
    std::vector<IntervalIndex<long double, std::size_t>::Entry> lines;
    notes.reserve(models_.size());
    firstDistance_ = std::numeric_limits<long double>::infinity();
    const long double scale = session.Timeline().BaseBpm() / 240'000'000.0L * pixelsPerWholeNote;
    for (const auto &[id, model] : models_)
    {
        const auto *info = session.FindNotePresentation(id);
        const double speed = info ? info->scrollMultiplier : 1;
        const auto start = session.ScrollDistance(model->Timing(), {});
        const auto endTime =
            info && info->hasEndTime ? std::max(info->endTime, model->ExpireTime()) : model->ExpireTime();
        const auto end = session.ScrollDistance(endTime, {});
        const auto margin = travelDistance / (scale * speed);
        notes.push_back({start - margin, end + margin, id});
        firstDistance_ = std::min(firstDistance_, start - margin);
    }
    const auto &measures = session.MeasureLines();
    lines.reserve(measures.size());
    for (std::size_t i = 0; i < measures.size(); ++i)
    {
        const auto distance = session.ScrollDistance(measures[i].timing, {});
        const auto margin = travelDistance / (scale * measures[i].scrollMultiplier);
        lines.push_back({distance - margin, distance + margin, i});
        firstDistance_ = std::min(firstDistance_, distance - margin);
    }
    notes_.Assign(std::move(notes));
    measures_.Assign(std::move(lines));
    revision_ = session.PresentationRevision();
}

std::span<const INote *const> GameplayNoteVisibility::VisibleNotes(
    long double distance, RhythmTime time, RhythmDuration retainAfterExpiry)
{
    visible_.clear();
    notes_.Visit(distance, distance, [&](NoteId id)
    {
        const auto *model = models_.at(id);
        if (time <= model->ExpireTime() + retainAfterExpiry)
            visible_.push_back(model);
    });
    std::ranges::sort(visible_, [](const auto *left, const auto *right)
    {
        if (left->Timing() != right->Timing())
            return left->Timing() < right->Timing();
        return left->Id() < right->Id();
    });
    return visible_;
}

RhythmTime GameplayNoteVisibility::InitialTimelineTime(const PlaySession &session) const
{
    using Time = RhythmTime;
    const Time latest{-2'000'000};
    if (session.ScrollDistance(latest, {}) <= firstDistance_)
        return latest;
    // Invert the monotone cumulative distance once, on scene entry.
    const long double bound = std::floor(firstDistance_ / session.MinimumScrollMultiplier());
    auto low =
        static_cast<Time::rep>(std::max(bound, static_cast<long double>(std::numeric_limits<Time::rep>::lowest())));
    auto high = latest.count();
    while (low < high - 1)
    {
        const auto middle = std::midpoint(low, high);
        if (session.ScrollDistance(Time{middle}, {}) <= firstDistance_)
            low = middle;
        else
            high = middle;
    }
    return Time{low};
}
