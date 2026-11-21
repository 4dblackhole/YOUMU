#pragma once
#include "Editing/IEditorDocument.h"
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// Find/Replace note patterns. A pattern is drawn on a lane of grid cells at one Beat division:
// short notes on cells, and a window [begin, end] (both edges included) that also covers the
// blanks before and after them. A match is a chart window of the same length that holds exactly
// those notes and nothing else: no other note, no long note, on either edge or inside.
struct NotePattern
{
    int division{4};          // Beat divider: cells per quarter note.
    int cells{16};            // Lane length in cells.
    int begin{0}, end{4};     // Window edges in cells, begin < end.
    std::map<int, int> notes; // Cell -> persisted key of a short note.

    [[nodiscard]] Rational CellLength() const { return Rational{1, static_cast<Rational::Representation>(division) * 4}; }
    [[nodiscard]] bool Usable() const noexcept;
};

struct PatternMatch
{
    Rational begin;                  // Window start in whole notes from the chart's start.
    std::vector<std::size_t> notes;  // Source IDs of the matched notes.
};

using PatternScope = std::optional<std::pair<MusicalPosition, MusicalPosition>>;

// Matches from the start, never sharing a note with an earlier match. A scope keeps whole windows inside it.
[[nodiscard]] std::vector<PatternMatch> FindNotePattern(const IEditorDocument &, const NotePattern &, const PatternScope &);

// The document's pattern with every match removed and `replacement` placed at each match's window
// start. A replacement note replaces the notes it lands on (a long note whole), as click placement does.
[[nodiscard]] PatternDocument ReplaceNotePattern(const IEditorDocument &, const NotePattern &replacement,
                                                 std::span<const PatternMatch> matches);
