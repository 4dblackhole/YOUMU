#pragma once

#include "Mode/Submodules/PlaySession.h"
#include "Utility/IntervalIndex.h"
#include <span>
#include <unordered_map>

// Read-only projection of the session. The presenter owns the Canvas nodes.
class GameplayNoteVisibility final
{
  public:
    void Bind(const ScrollGear &gear);
    void Reset() noexcept;
    [[nodiscard]] const INote *Find(NoteId id) const noexcept;
    [[nodiscard]] bool Matches(std::uint64_t revision) const noexcept { return revision_ == revision; }
    void Rebuild(const PlaySession &session, float pixelsPerWholeNote, float travelDistance);
    [[nodiscard]] std::span<const INote *const> VisibleNotes(long double distance, RhythmTime time,
                                                             RhythmDuration retainAfterExpiry);
    [[nodiscard]] RhythmTime InitialTimelineTime(const PlaySession &session) const;

    void VisitMeasureLines(long double distance, const auto &visit) const
    {
        measures_.Visit(distance, distance, visit);
    }

  private:
    std::unordered_map<NoteId, const INote *> models_;
    IntervalIndex<long double, NoteId> notes_;
    IntervalIndex<long double, std::size_t> measures_;
    std::vector<const INote *> visible_;
    std::uint64_t revision_{};
    long double firstDistance_{};
};
