#include "TaikoEditorLayout.h"
#include "TaikoEditorMode.h"
#include <algorithm>
#include <cmath>

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
    const auto grid = ScoreGridFor(areas_.score);
    const auto x = std::clamp(point.x, grid.left, grid.Right() - .01F);
    const auto y = std::clamp(point.y, grid.rowTop, grid.rowTop + grid.rowHeight * VisibleRows - .01F);
    const auto measure = state.Score().firstMeasure +
                         static_cast<std::int64_t>((y - grid.rowTop) / grid.rowHeight) * MeasuresPerRow +
                         static_cast<std::int64_t>((x - grid.left) / grid.measureWidth);
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
    if (tool_ != 0 || !(state.Score().realtime ? areas_.lane : ScoreGridFor(areas_.score).Bounds()).Contains(point))
        return false;
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
