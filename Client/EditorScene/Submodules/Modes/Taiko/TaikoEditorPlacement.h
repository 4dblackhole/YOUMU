#pragma once
#include "Editing/IEditorDocument.h"
#include "Taiko/Submodules/TaikoChartNotes.h"
#include <algorithm>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>

// Taiko has one lane, so a new note replaces every note whose authored span
// [head, tail] shares a musical position with its own span. Returns each
// endpoint ID once, including long-note partners outside the range.
inline std::vector<std::size_t> OverlappedNotes(const IEditorDocument &document, MusicalPosition begin,
                                                MusicalPosition end)
{
    std::set<std::size_t> ids;
    for (const auto &candidate : document.NotesInMeasures(begin.measure, end.measure + 1))
    {
        const auto selected = document.FindNote(candidate.note.sourceOrder);
        const auto &head = selected ? selected->head.note : candidate.note;
        const auto last = selected && selected->tail ? selected->tail->note.position : head.position;
        if (last < begin || end < head.position)
            continue;
        ids.insert(head.sourceOrder);
        if (selected && selected->tail)
            ids.insert(selected->tail->note.sourceOrder);
    }
    return {ids.begin(), ids.end()};
}

inline bool IsSameOrdinaryNote(const IEditorDocument &document, std::span<const std::size_t> replaced,
                               MusicalPosition position, int keyType)
{
    if (replaced.size() != 1)
        return false;
    const auto existing = document.FindNote(replaced.front());
    if (!existing || existing->head.note.actionType != 0)
        return false;
    return existing->head.note.position == position && existing->head.note.keyType == keyType;
}

// Validates the document as it will be after the replaced notes are removed.
inline void ValidatePlacement(const IEditorDocument &document, MusicalPosition position, int keyType,
                              std::optional<MusicalPosition> end = {},
                              std::optional<std::size_t> editedId = {},
                              std::span<const std::size_t> replaced = {})
{
    const auto beginTime = document.Timeline().Compile(position);
    const auto endTime = end ? document.Timeline().Compile(*end) : beginTime;
    if (end && *end <= position)
        throw std::invalid_argument("Long note end must follow its start in musical position.");

    // The range query includes both endpoints of every intersecting long note.
    // Edits keep their stable IDs; new notes follow existing equal-time notes.
    const auto edited = editedId ? document.FindNote(*editedId) : std::nullopt;
    auto notes = document.NotesInTimeRange(std::min(beginTime, endTime), std::max(beginTime, endTime));
    if (edited)
        std::erase_if(notes,
                      [&](const auto &note)
                      {
                          return note.note.sourceOrder == edited->head.note.sourceOrder ||
                                 (edited->tail && note.note.sourceOrder == edited->tail->note.sourceOrder);
                      });
    std::erase_if(notes, [replaced](const auto &note)
                  { return std::ranges::find(replaced, note.note.sourceOrder) != replaced.end(); });
    notes.push_back({{position,
                      keyType,
                      end ? 1 : 0,
                      {},
                      {},
                      {},
                      edited ? edited->head.note.sourceOrder : std::numeric_limits<std::size_t>::max() - 1},
                     beginTime,
                     1,
                     document.Timeline().IsPositionActive(position)});
    if (end)
        notes.push_back(
            {{*end,
              keyType,
              2,
              {},
              {},
              {},
              edited && edited->tail ? edited->tail->note.sourceOrder : std::numeric_limits<std::size_t>::max()},
             endTime,
             1,
             document.Timeline().IsPositionActive(*end)});
    notes = PlayableInSourceOrder(std::move(notes));
    std::optional<int> activeLongType;
    for (const auto &compiled : notes)
    {
        const auto &note = compiled.note;
        if (note.actionType == 1)
        {
            if (activeLongType)
                throw std::invalid_argument("Taiko long notes cannot overlap another long note.");
            activeLongType = note.keyType;
        }
        else if (note.actionType == 2 && activeLongType == note.keyType)
            activeLongType.reset();
        else if (note.actionType == 0 && activeLongType)
            throw std::invalid_argument("A Taiko ordinary note cannot be placed inside a long note.");
    }
}
