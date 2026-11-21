#include "EditorTime.h"
#include "EditorWorkspace.h"
#include <limits>

void EditorWorkspace::SelectNotes(std::span<const std::size_t> ids)
{
    std::set<std::size_t> selected;
    std::optional<std::pair<MusicalPosition, MusicalPosition>> range;
    for (const auto id : ids)
        if (const auto note = Document().FindNote(id))
        {
            selected.insert(note->head.note.sourceOrder);
            const auto begin = note->head.note.position;
            const auto end = note->tail ? note->tail->note.position : begin;
            if (note->tail)
                selected.insert(note->tail->note.sourceOrder);
            if (!range)
                range = {begin, end};
            else
            {
                range->first = std::min(range->first, begin);
                range->second = std::max(range->second, end);
            }
        }
    selectedNotes_.swap(selected);
    noteSelectionRange_ = range;
    RequestRebuild();
}

void EditorWorkspace::SelectNoteRange(MusicalPosition begin, MusicalPosition end)
{
    if (end < begin)
        std::swap(begin, end);
    std::set<std::size_t> ids, visited;
    for (const auto &candidate : Document().NotesInMeasures(begin.measure, end.measure + 1))
        if (const auto note = Document().FindNote(candidate.note.sourceOrder))
        {
            if (!visited.insert(note->head.note.sourceOrder).second)
                continue;
            const auto head = note->head.note.position;
            const auto tail = note->tail ? note->tail->note.position : head;
            if (head > end || tail < begin)
                continue;
            ids.insert(note->head.note.sourceOrder);
            if (note->tail)
                ids.insert(note->tail->note.sourceOrder);
        }
    selectedNotes_.swap(ids);
    noteSelectionRange_ = {begin, end}; // Preserve blank margins for paste-with-blanks.
    RequestRebuild();
}

void EditorWorkspace::SelectListRow(std::size_t row, bool toggle, bool range)
{
    const auto focus = Focus();
    if (focus == EditorFocus::Notes)
        return;
    auto &selected = focus == EditorFocus::Timing ? selectedTiming_ : selectedEffects_;
    auto &anchor = focus == EditorFocus::Timing ? timingSelectionAnchor_ : effectSelectionAnchor_;
    const auto count = focus == EditorFocus::Timing
                           ? Document().Timing().size()
                           : Document().Effects().commands.size() + Document().Effects().hitSoundChanges.size();
    if (row >= count)
        throw std::out_of_range("Selected editor row no longer exists.");
    if (range && anchor)
    {
        if (!toggle)
            selected.clear();
        for (auto i = std::min(row, *anchor); i <= std::max(row, *anchor); ++i)
            selected.insert(i);
    }
    else
    {
        if (!toggle)
            selected.clear();
        if (toggle && selected.contains(row))
            selected.erase(row);
        else
            selected.insert(row);
        anchor = row;
    }
    RequestRebuild();
}

bool EditorWorkspace::IsListRowSelected(std::size_t row) const noexcept
{
    const auto focus = Focus();
    return focus == EditorFocus::Timing ? selectedTiming_.contains(row)
                                        : focus == EditorFocus::Effects && selectedEffects_.contains(row);
}

std::optional<std::size_t> EditorWorkspace::ListAnchor() const noexcept
{
    const auto focus = Focus();
    if (focus == EditorFocus::Notes)
        return std::nullopt;
    const auto &anchor = focus == EditorFocus::Timing ? timingSelectionAnchor_ : effectSelectionAnchor_;
    if (!anchor || !IsListRowSelected(*anchor))
        return std::nullopt;
    return anchor;
}

void EditorWorkspace::ClearEditSelection()
{
    selectedNotes_.clear();
    selectedTiming_.clear();
    selectedEffects_.clear();
    noteSelectionRange_.reset();
    timingSelectionAnchor_.reset();
    effectSelectionAnchor_.reset();
    mode_->CancelInteraction();
}

void EditorWorkspace::Undo()
{
    if (!Document().CanUndo())
        return;
    editor_->Undo();
    ClearEditSelection();
    DocumentChanged();
}

void EditorWorkspace::Redo()
{
    if (!Document().CanRedo())
        return;
    editor_->Redo();
    ClearEditSelection();
    DocumentChanged();
}

void EditorWorkspace::CopySelection()
{
    Clipboard result;
    const auto focus = Focus();
    result.focus = focus;
    std::optional<std::pair<MusicalPosition, MusicalPosition>> bounds;
    const auto extend = [&bounds](auto first, auto last)
    {
        if (!bounds)
            bounds = {first, last};
        else
        {
            bounds->first = std::min(bounds->first, first);
            bounds->second = std::max(bounds->second, last);
        }
    };
    if (focus == EditorFocus::Notes)
    {
        if (noteSelectionRange_)
            bounds = noteSelectionRange_;
        std::set<std::size_t> visited;
        for (const auto id : selectedNotes_)
            if (const auto note = Document().FindNote(id); note && visited.insert(note->head.note.sourceOrder).second)
            {
                result.notes.push_back({note->head.note, note->tail ? std::optional{note->tail->note} : std::nullopt});
                // Whole long notes are copied even when only one endpoint was dragged.
                extend(note->head.note.position, note->tail ? note->tail->note.position : note->head.note.position);
            }
    }
    else if (focus == EditorFocus::Timing)
        for (auto row : selectedTiming_)
        {
            const auto &event = Document().Timing().at(row);
            result.timing.push_back({event, {}});
            extend(event.position, event.position);
        }
    else
        for (auto row : selectedEffects_)
        {
            const auto &effects = Document().Effects();
            if (row < effects.commands.size())
            {
                const auto &event = effects.commands[row];
                result.effects.push_back({event, {}});
                extend(event.position, event.endPosition.value_or(event.position));
            }
            else
            {
                const auto &event = effects.hitSoundChanges.at(row - effects.commands.size());
                result.sounds.push_back({event, {}});
                extend(event.position, event.position);
            }
        }
    if (!bounds)
        return;
    const auto &timeline = Document().Timeline();
    const auto offset = [&](auto position) { return timeline.WholeNotesBetween(bounds->first, position); };
    result.length = offset(bounds->second);
    for (auto &note : result.notes)
    {
        note.headOffset = offset(note.head.position);
        note.tailOffset = note.tail ? offset(note.tail->position) : note.headOffset;
    }
    for (auto &item : result.timing)
        item.offset = offset(item.value.position);
    for (auto &item : result.sounds)
        item.offset = offset(item.value.position);
    for (auto &item : result.effects)
    {
        item.offset = offset(item.value.position);
        if (item.value.endPosition)
            item.endOffset = offset(*item.value.endPosition);
    }
    clipboard_ = std::move(result);
}

MusicalPosition EditorWorkspace::CursorGridPosition() const
{
    const auto &timeline = Document().Timeline();
    const auto divisions = static_cast<std::int64_t>(score_.division) * 4;
    const auto whole = std::max(0.0L, timeline.WholeNotesAtTime(Microseconds(timeMs_)));
    const auto value = std::round(whole * divisions);
    if (!std::isfinite(value) || value >= std::numeric_limits<std::int64_t>::max())
        throw std::out_of_range("The cursor position exceeds the supported musical range.");
    return timeline.PositionAtWholeNotes({static_cast<std::int64_t>(value), divisions});
}

void EditorWorkspace::PasteSelection(bool includeBlanks)
{
    if (!clipboard_)
        return;
    const auto focus = Focus();
    if (clipboard_->focus != focus)
        throw std::invalid_argument("Paste into the same kind of list as the copied items.");
    const bool noNotesOrTiming = clipboard_->notes.empty() && clipboard_->timing.empty();
    const bool noEffectsOrSounds = clipboard_->effects.empty() && clipboard_->sounds.empty();
    if (!includeBlanks && noNotesOrTiming && noEffectsOrSounds)
        return;
    const auto anchor = CursorGridPosition();
    if (focus == EditorFocus::Notes)
        PasteNotes(*clipboard_, anchor, includeBlanks);
    else if (focus == EditorFocus::Timing)
        PasteTiming(*clipboard_, anchor, includeBlanks);
    else
        PasteEffects(*clipboard_, anchor, includeBlanks);
    ClearEditSelection();
}

void EditorWorkspace::DeleteSelection()
{
    auto pattern = Document().Pattern();
    auto effects = Document().Effects();
    const auto focus = Focus();
    if (focus == EditorFocus::Notes)
    {
        if (selectedNotes_.empty())
            return;
        std::erase_if(pattern.notes, [this](const auto &note) { return selectedNotes_.contains(note.sourceOrder); });
    }
    else if (focus == EditorFocus::Timing)
    {
        if (selectedTiming_.empty())
            return;
        std::size_t row = 0;
        std::erase_if(pattern.timing, [this, &row](const auto &) { return selectedTiming_.contains(row++); });
    }
    else
    {
        if (selectedEffects_.empty())
            return;
        std::size_t row = 0;
        std::erase_if(effects.commands, [this, &row](const auto &) { return selectedEffects_.contains(row++); });
        std::erase_if(effects.hitSoundChanges, [this, &row](const auto &) { return selectedEffects_.contains(row++); });
    }
    Replace(std::move(pattern), std::move(effects));
    ClearEditSelection();
}
