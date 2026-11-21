#include "EditorView.h"
#include <algorithm>
#include <cmath>
#include <format>
using namespace PhantomCore;

using namespace editor_ui;
namespace layout = editor_layout;
namespace theme = ui_colors::theme;

namespace
{
    constexpr float DialogWidth = 760, DialogHeight = 372, LaneHeight = 56, EdgeGrab = 10;
    constexpr int MaximumMeasures = 4;

    float CellX(const Rect lane, const NotePattern &pattern, const int cell)
    {
        return lane.x + lane.width * static_cast<float>(cell) / static_cast<float>(pattern.cells);
    }
    int NearestCell(const Rect lane, const NotePattern &pattern, const float x)
    {
        const float cell = std::round((x - lane.x) / lane.width * static_cast<float>(pattern.cells));
        return std::clamp(static_cast<int>(cell), 0, pattern.cells);
    }
    // One 4/4 measure at the Beat divider, with a one-beat window to start from.
    NotePattern EmptyPattern(const int division)
    {
        NotePattern pattern;
        pattern.division = division;
        pattern.cells = division * 4;
        pattern.begin = 0;
        pattern.end = std::min(pattern.cells, division);
        return pattern;
    }
    void ResizePattern(NotePattern &pattern, const int measures)
    {
        pattern.cells = pattern.division * 4 * measures;
        pattern.end = std::min(pattern.end, pattern.cells);
        pattern.begin = std::min(pattern.begin, pattern.end - 1);
        std::erase_if(pattern.notes, [&pattern](const auto &note) { return note.first > pattern.cells; });
    }
} // namespace

void EditorView::OpenFind()
{
    // Patterns survive closing the dialog; a fresh pair follows the current Beat divider.
    const bool empty = find_.find.notes.empty() && find_.replace.notes.empty();
    if (empty)
    {
        find_.find = EmptyPattern(state_.Score().division);
        find_.replace = find_.find;
    }
    find_.scope = state_.NoteSelectionRange();
    find_.matches.clear();
    find_.current = 0;
    find_.dragging.reset();
    find_.dirty = true;
    findOpen_ = true;
    state_.RequestRebuild();
}

void EditorView::CloseFind()
{
    findOpen_ = false;
    find_.dragging.reset();
    state_.RequestRebuild();
}

void EditorView::RefreshFindMatches()
{
    if (!find_.dirty && find_.revision == state_.Document().Revision())
        return;
    find_.matches = FindNotePattern(state_.Document(), find_.find, find_.scope);
    find_.current = find_.matches.empty() ? 0 : std::min(find_.current, find_.matches.size() - 1);
    find_.revision = state_.Document().Revision();
    find_.dirty = false;
}

void EditorView::ShowFindMatch(const std::size_t index)
{
    RefreshFindMatches();
    if (find_.matches.empty())
        return;
    find_.current = index % find_.matches.size();
    const auto &match = find_.matches[find_.current];
    // The match's notes become the selection, so both score views mark them; the cursor goes to the first.
    state_.SelectNotes(match.notes);
    if (const auto note = state_.Document().FindNote(match.notes.front()))
        state_.Seek(static_cast<double>(note->head.timing.count()) / 1000.0);
    state_.RequestRebuild();
}

void EditorView::ReplaceFindMatches(const bool all)
{
    RefreshFindMatches();
    if (find_.matches.empty())
        return;
    if (!find_.replace.Usable())
        throw std::invalid_argument("Draw the replacement inside its window.");
    const auto &document = state_.Document();
    const std::span<const PatternMatch> matches =
        all ? std::span<const PatternMatch>{find_.matches} : std::span<const PatternMatch>{&find_.matches[find_.current], 1};
    // One Replace is one Undo step, however many matches it changes.
    state_.Replace(ReplaceNotePattern(document, find_.replace, matches), document.Effects());
    state_.SelectNotes({});
    find_.dirty = true;
    RefreshFindMatches();
}

bool EditorView::UpdateFindPointer(const InputState &input, const Point point)
{
    // An edge drag follows the pointer until the button comes up.
    if (find_.dragging)
    {
        const auto [index, begin] = *find_.dragging;
        auto &pattern = index == 0 ? find_.find : find_.replace;
        const int cell = NearestCell(find_.lanes[static_cast<std::size_t>(index)], pattern, point.x);
        if (begin)
            pattern.begin = std::min(cell, pattern.end - 1);
        else
            pattern.end = std::max(cell, pattern.begin + 1);
        find_.dirty = true;
        state_.RequestRebuild();
        if (!input.IsMouseButtonDown(MouseButton::Left))
            find_.dragging.reset();
        return true;
    }
    const bool left = input.WasMouseButtonPressed(MouseButton::Left);
    if (!left && !input.WasMouseButtonPressed(MouseButton::Right))
        return false;
    for (int index = 0; index < 2; ++index)
    {
        const auto lane = find_.lanes[static_cast<std::size_t>(index)];
        auto &pattern = index == 0 ? find_.find : find_.replace;
        const int cell = NearestCell(lane, pattern, point.x);
        // The gold edges stick out above and below the lane; that is where they are grabbed.
        const bool besideLane = point.y < lane.y || point.y > lane.y + lane.height;
        const Rect edges{lane.x - EdgeGrab, lane.y - EdgeGrab, lane.width + EdgeGrab * 2, lane.height + EdgeGrab * 2};
        if (left && besideLane && edges.Contains(point))
        {
            const float toBegin = std::abs(CellX(lane, pattern, pattern.begin) - point.x);
            const float toEnd = std::abs(CellX(lane, pattern, pattern.end) - point.x);
            find_.dragging = std::pair{index, toBegin <= toEnd};
            return true;
        }
        if (!lane.Contains(point))
            continue;
        if (!left)
            pattern.notes.erase(cell);
        else if (const auto key = state_.Mode().PatternKey())
            pattern.notes.insert_or_assign(cell, *key);
        else
            state_.SetStatus("Pick a short-note tool (W, E) to draw a pattern.");
        find_.dirty = true;
        state_.RequestRebuild();
        return true;
    }
    return false;
}

void EditorView::DrawPatternLane(const int index, const Rect lane, std::wstring caption)
{
    auto &pattern = index == 0 ? find_.find : find_.replace;
    find_.lanes[static_cast<std::size_t>(index)] = lane;
    Label({lane.x, lane.y - 26, 200, 20}, std::move(caption), {14, theme::InkSoft, TextAlignment::Leading, 800});
    Box(lane, ui_colors::Rgb(0x141A2C));
    const float x0 = CellX(lane, pattern, pattern.begin), x1 = CellX(lane, pattern, pattern.end);
    Box({x0, lane.y, x1 - x0, lane.height}, ui_colors::Rgb(0x4D9CFF, .22F));
    const int beat = pattern.division, measure = pattern.division * 4;
    for (int cell = 1; cell < pattern.cells; ++cell)
    {
        const bool onMeasure = cell % measure == 0, onBeat = cell % beat == 0;
        const float x = std::round(CellX(lane, pattern, cell));
        const float tick = onBeat ? lane.height : lane.height * .4F;
        Box({x, lane.y + (lane.height - tick) * .5F, onMeasure ? 2.0F : 1.0F, tick},
            {1, 1, 1, onMeasure ? .5F : onBeat ? .22F : .1F});
    }
    for (const float x : {x0, x1})
        Box({x - 1.5F, lane.y - 8, 3, lane.height + 16}, theme::Sun);
    const float cellWidth = lane.width / static_cast<float>(pattern.cells);
    const float radius = std::clamp(cellWidth * .9F, 6.0F, 18.0F);
    for (const auto &[cell, key] : pattern.notes)
        state_.Mode().DrawPatternNote(*this, {CellX(lane, pattern, cell), lane.y + lane.height * .5F}, radius, key);
}

void EditorView::DrawFindDialog()
{
    if (!findOpen_)
        return;
    RefreshFindMatches();
    // The scrim owns clicks outside the dialog.
    Box({0, 0, frame_.width, layout::Height}, ui_colors::Rgb(0x142F5E, .42F));
    Control({0, 0, frame_.width, layout::Height}, [] {});
    const Rect dialog{(frame_.width - DialogWidth) * .5F, (layout::Height - DialogHeight) * .5F, DialogWidth, DialogHeight};
    Box(dialog, theme::Surface);
    Control(dialog, [] {});
    Label({dialog.x + 24, dialog.y + 16, 400, 28}, std::wstring(texts_.Get("Editor.menuFindReplace")),
          {18, theme::Ink, TextAlignment::Leading, 900});
    const Rect close{dialog.x + dialog.width - 44, dialog.y + 14, 32, 32};
    Icon({close.x + 7, close.y + 7, 18, 18}, L"x", theme::InkSoft);
    Control(close, [this] { CloseFind(); });

    // Lane length in measures, shared by both lanes.
    const int measures = std::max(1, find_.find.cells / (find_.find.division * 4));
    const float stepperX = dialog.x + dialog.width - 44 - 8 - 120;
    const auto resize = [this](int count) {
        ResizePattern(find_.find, count);
        ResizePattern(find_.replace, count);
        find_.dirty = true;
        state_.RequestRebuild();
    };
    Button({stepperX, dialog.y + 14, 32, 32}, L"−", [resize, measures] { resize(std::max(1, measures - 1)); });
    Label({stepperX + 32, dialog.y + 14, 56, 32}, std::to_wstring(measures), {15, theme::Brand, TextAlignment::Center, 800, true});
    Button({stepperX + 88, dialog.y + 14, 32, 32}, L"+", [resize, measures] { resize(std::min(MaximumMeasures, measures + 1)); });

    const Rect findLane{dialog.x + 24, dialog.y + 92, dialog.width - 48, LaneHeight};
    const Rect replaceLane{dialog.x + 24, dialog.y + 192, dialog.width - 48, LaneHeight};
    DrawPatternLane(0, findLane, std::wstring(texts_.Get("Editor.findLane")));
    DrawPatternLane(1, replaceLane, std::wstring(texts_.Get("Editor.replaceLane")));

    // Bottom row: the match count, previous/next, Replace and Replace all.
    const float rowY = dialog.y + dialog.height - 24 - 36;
    const auto count = find_.matches.size();
    Label({dialog.x + 24 + 2 * 52, rowY, 120, 36},
          count == 0 ? std::wstring(L"0") : std::format(L"{} / {}", find_.current + 1, count),
          {16, theme::Brand, TextAlignment::Leading, 800, true});
    Button({dialog.x + 24, rowY, 44, 36}, L"◀", [this, count] {
        if (count != 0)
            ShowFindMatch(find_.current + count - 1);
    });
    Button({dialog.x + 24 + 52, rowY, 44, 36}, L"▶", [this, count] {
        if (count != 0)
            ShowFindMatch(find_.current + 1);
    });
    const float right = dialog.x + dialog.width - 24;
    FlatButton({right - 140 - 8 - 120, rowY, 120, 36}, std::wstring(texts_.Get("Editor.replaceOne")),
               [this] { ReplaceFindMatches(false); });
    FlatButton({right - 140, rowY, 140, 36}, std::wstring(texts_.Get("Editor.replaceAll")),
               [this] { ReplaceFindMatches(true); }, true);
}
