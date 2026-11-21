#include "TaikoEditorLayout.h"
#include "TaikoEditorMode.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace PhantomCore;

using namespace taiko_editor_layout;

std::optional<MusicalPosition> TaikoEditorMode::SnapPosition(IEditorContext &state, Point point) const
{
    if (state.Score().realtime)
    {
        if (realtimeGrid_.empty())
            return {};
        return std::ranges::min_element(realtimeGrid_, {},
                                        [point](const auto &grid) { return std::abs(grid.first - point.x); })
            ->second;
    }
    if (overviewGrid_.empty())
        return {};
    // Dragging past a row edge keeps snapping inside the nearest drawn measure.
    const ScoreCell *nearest = nullptr;
    float distance = std::numeric_limits<float>::infinity();
    for (const auto &cell : scoreCells_)
    {
        const float dx = std::max({cell.bandLeft - point.x, 0.0F, point.x - cell.bandRight});
        const float dy = std::max({cell.top - point.y, 0.0F, point.y - (cell.laneTop + cell.laneHeight)});
        if (dx + dy < distance)
        {
            distance = dx + dy;
            nearest = &cell;
        }
    }
    if (!nearest)
        return {};
    const auto measure = nearest->measure;
    const auto x = std::clamp(point.x, nearest->x0, nearest->x0 + nearest->width);
    const auto begin =
        std::ranges::lower_bound(overviewGrid_, measure, {}, [](const auto &g) { return g.second.measure; });
    const auto end =
        std::ranges::upper_bound(overviewGrid_, measure, {}, [](const auto &g) { return g.second.measure; });
    if (begin == end)
        return {};
    return std::min_element(begin, end, [x](const auto &a, const auto &b)
                            { return std::abs(a.first.x - x) < std::abs(b.first.x - x); })
        ->second;
}

bool TaikoEditorMode::BeginScoreSelection(IEditorContext &state, Point point)
{
    if (tool_ != 0 || state.Score().preview || !(state.Score().realtime ? areas_.lane : areas_.score).Contains(point))
        return false;
    // A gold edge of the selected range moves that edge; a note drawn on the edge keeps its own click.
    const auto edge = std::ranges::find_if(rangeEdgeHits_, [point](const auto &hit) { return hit.bounds.Contains(point); });
    if (edge != rangeEdgeHits_.end() && !NoteHitAt(state, point))
    {
        selectionAnchor_ = edge->otherEdge;
        selectionStart_ = point;
        selecting_ = true;
        return true;
    }
    const auto position = SnapPosition(state, point);
    if (!position)
        return false;
    EditScore(state, point, false);
    selectionAnchor_ = position;
    selectionStart_ = point;
    selecting_ = false;
    return true;
}

bool TaikoEditorMode::UpdateScoreSelection(IEditorContext &state, Point point, bool released)
{
    if (!selectionAnchor_)
        return false;
    selecting_ |= std::hypot(point.x - selectionStart_.x, point.y - selectionStart_.y) >= 4;
    if (selecting_)
        if (const auto end = SnapPosition(state, point))
        {
            inspector_.Clear();
            state.SelectNoteRange(*selectionAnchor_, *end);
        }
    if (released)
    {
        selectionAnchor_.reset();
        selecting_ = false;
    }
    return true;
}

void TaikoEditorMode::ActivateScore(IEditorContext &state, Point point)
{
    if (state.Score().preview)
        return;
    // A head or tail moves the cursor to that endpoint; a long-note body to its head.
    std::optional<MusicalPosition> position;
    if (const auto *hit = NoteHitAt(state, point))
        position = hit->position;
    else if (const auto order = NoteAt(state, point))
        if (const auto note = state.Document().FindNote(*order))
            position = note->head.note.position;
    if (!position)
        return;
    state.Seek(static_cast<double>(state.Document().Timeline().Compile(*position).count()) / 1000.0);
    state.RequestRebuild();
}

void TaikoEditorMode::ToggleScoreNote(IEditorContext &state, Point point)
{
    if (state.Score().preview)
        return;
    const auto order = NoteAt(state, point);
    const auto note = order ? state.Document().FindNote(*order) : std::nullopt;
    if (!note)
        return;
    // Selection keeps heads and tails; a long note is added or removed as one.
    std::vector<std::size_t> ids;
    const auto head = note->head.note.sourceOrder;
    const bool selected = state.SelectedNoteIds().contains(head);
    for (const auto id : state.SelectedNoteIds())
    {
        const auto owner = state.Document().FindNote(id);
        if (owner && owner->head.note.sourceOrder != head)
            ids.push_back(id);
    }
    if (!selected)
        ids.push_back(head);
    inspector_.Clear();
    if (ids.size() == 1)
        inspector_.Select(state.Document(), ids.front());
    state.SelectNotes(ids);
    state.RequestRebuild();
}
