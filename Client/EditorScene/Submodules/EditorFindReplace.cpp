#include "EditorFindReplace.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

bool NotePattern::Usable() const noexcept
{
    if (notes.empty() || begin >= end)
        return false;
    return notes.begin()->first >= begin && notes.rbegin()->first <= end;
}

namespace
{
    struct ShortNote
    {
        Rational at;
        int key{};
        std::size_t id{};
    };
    struct LongSpan
    {
        Rational head, tail;
        std::vector<std::size_t> ids;
    };
    // The chart in whole notes: short notes by position and the spans of long notes.
    struct ChartNotes
    {
        std::vector<ShortNote> shorts;
        std::vector<LongSpan> longs;
    };

    ChartNotes CollectNotes(const IEditorDocument &document)
    {
        const auto &timeline = document.Timeline();
        ChartNotes chart;
        for (const auto &note : document.Pattern().notes)
        {
            if (note.actionType == 0)
            {
                chart.shorts.push_back({timeline.PositionToWholeNotes(note.position), note.keyType, note.sourceOrder});
                continue;
            }
            if (note.actionType != 1)
                continue;
            // An unpaired head still occupies its own position.
            const auto pair = document.FindNote(note.sourceOrder);
            LongSpan span{timeline.PositionToWholeNotes(note.position), {}, {note.sourceOrder}};
            span.tail = span.head;
            if (pair && pair->tail)
            {
                span.tail = timeline.PositionToWholeNotes(pair->tail->note.position);
                span.ids.push_back(pair->tail->note.sourceOrder);
            }
            chart.longs.push_back(std::move(span));
        }
        std::ranges::sort(chart.shorts, {}, &ShortNote::at);
        return chart;
    }

    bool TouchesLongNote(const ChartNotes &chart, const Rational &first, const Rational &last)
    {
        return std::ranges::any_of(chart.longs, [&](const LongSpan &span) { return span.head <= last && first <= span.tail; });
    }

    // The window [first, last] holds exactly the pattern's notes, keyed by their offsets.
    std::optional<std::vector<std::size_t>> MatchWindow(const ChartNotes &chart, const NotePattern &pattern,
                                                        const Rational &first, const Rational &last)
    {
        if (TouchesLongNote(chart, first, last))
            return std::nullopt;
        const auto from = std::ranges::lower_bound(chart.shorts, first, {}, &ShortNote::at);
        const auto to = std::ranges::upper_bound(chart.shorts, last, {}, &ShortNote::at);
        if (static_cast<std::size_t>(to - from) != pattern.notes.size())
            return std::nullopt;
        std::vector<std::size_t> ids;
        auto expected = pattern.notes.begin();
        for (auto note = from; note != to; ++note, ++expected)
        {
            const auto at = first + pattern.CellLength() * (expected->first - pattern.begin);
            if (note->at != at || note->key != expected->second)
                return std::nullopt;
            ids.push_back(note->id);
        }
        return ids;
    }

    bool InScope(const IEditorDocument &document, const PatternScope &scope, const Rational &first, const Rational &last)
    {
        if (!scope)
            return true;
        const auto &timeline = document.Timeline();
        return timeline.PositionToWholeNotes(scope->first) <= first && last <= timeline.PositionToWholeNotes(scope->second);
    }
} // namespace

std::vector<PatternMatch> FindNotePattern(const IEditorDocument &document, const NotePattern &pattern,
                                          const PatternScope &scope)
{
    if (!pattern.Usable())
        return {};
    const auto chart = CollectNotes(document);
    const auto &[firstCell, firstKey] = *pattern.notes.begin();
    const auto lead = pattern.CellLength() * (firstCell - pattern.begin);
    const auto length = pattern.CellLength() * (pattern.end - pattern.begin);
    std::vector<PatternMatch> matches;
    std::set<std::size_t> taken;
    // Every note of the first pattern key may start a window; earlier matches win shared notes.
    for (const auto &candidate : chart.shorts)
    {
        if (candidate.key != firstKey || taken.contains(candidate.id))
            continue;
        const auto first = candidate.at - lead;
        const auto last = first + length;
        // The window, leading blank included, starts at or after the chart's first measure.
        if (first < Rational{})
            continue;
        if (!InScope(document, scope, first, last))
            continue;
        auto ids = MatchWindow(chart, pattern, first, last);
        if (!ids)
            continue;
        const bool shared = std::ranges::any_of(*ids, [&taken](std::size_t id) { return taken.contains(id); });
        if (shared)
            continue;
        taken.insert(ids->begin(), ids->end());
        matches.push_back({first, std::move(*ids)});
    }
    return matches;
}

PatternDocument ReplaceNotePattern(const IEditorDocument &document, const NotePattern &replacement,
                                   std::span<const PatternMatch> matches)
{
    if (replacement.notes.empty() || replacement.begin >= replacement.end)
        throw std::invalid_argument("Draw at least one note in the replacement pattern.");
    const auto &timeline = document.Timeline();
    auto pattern = document.Pattern();
    std::set<std::size_t> removed;
    for (const auto &match : matches)
        removed.insert(match.notes.begin(), match.notes.end());
    std::size_t nextId = 0;
    for (const auto &note : pattern.notes)
        nextId = std::max(nextId, note.sourceOrder + 1);
    const auto chart = CollectNotes(document);
    // A placed note replaces what it lands on; a later match's note replaces an earlier one's.
    std::map<Rational, int> placed;
    for (const auto &match : matches)
        for (const auto &[cell, key] : replacement.notes)
            placed.insert_or_assign(match.begin + replacement.CellLength() * (cell - replacement.begin), key);
    for (const auto &[at, key] : placed)
    {
        for (const auto &note : chart.shorts)
            if (note.at == at)
                removed.insert(note.id);
        for (const auto &span : chart.longs)
            if (span.head <= at && at <= span.tail)
                removed.insert(span.ids.begin(), span.ids.end());
    }
    std::erase_if(pattern.notes, [&removed](const PatternNote &note) { return removed.contains(note.sourceOrder); });
    for (const auto &[at, key] : placed)
    {
        if (nextId == std::numeric_limits<std::size_t>::max())
            throw std::overflow_error("Note ID space exhausted.");
        PatternNote note;
        note.position = timeline.PositionAtWholeNotes(at);
        note.keyType = key;
        note.sourceOrder = nextId++;
        pattern.notes.push_back(std::move(note));
    }
    return pattern;
}
