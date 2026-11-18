#include "../../EditorSupport.h"
#include "../../EditorTimeAxis.h"
#include "App/AssetPaths.h"
#include "Presentation/ModePresentation.h"
#include "Taiko/Submodules/TaikoChartNotes.h"
#include "Taiko/Submodules/TaikoNoteOptions.h"
#include "TaikoEditorLayout.h"
#include "TaikoEditorMode.h"
#include "TaikoEditorTool.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <algorithm>
#include <limits>
#include <locale>
#include <sstream>
using namespace PhantomCore;

using namespace editor_ui;
using namespace taiko_editor_layout;

namespace
{
    constexpr Color Don{1, .33F, .43F, 1}, Kat{.10F, .76F, .82F, 1}, Gold{.96F, .80F, .29F, 1};

    Color RealtimeTickColor(const Rational &fraction)
    {
        // Reduced positions give the coarsest matching note value priority.
        switch (fraction.Denominator())
        {
        case 1: case 2: case 4: return White;
        case 8: return {1, .2F, .2F, 1};
        case 16: return {.2F, .5F, 1, 1};
        case 32: return {1, .9F, .1F, 1};
        case 64: return {.65F, .65F, .65F, 1};
        case 3: case 6: case 12: return {.15F, 1, .3F, 1};
        case 24: return {.8F, .35F, 1, 1};
        default: return {1, .55F, .1F, 1};
        }
    }

} // namespace

void TaikoEditorMode::ScrollScore(IEditorContext &state, int direction)
{
    // Realtime and Preview step one beat grid; the score, which follows the cursor, steps a measure.
    if (state.Score().realtime || state.Score().preview)
    {
        state.Seek(StepEditorTimeline(state.Document().Timeline(), state.TimeMilliseconds(),
                                      state.Score().division, direction).count() / 1000.0);
        return;
    }
    const auto &timeline = state.Document().Timeline();
    const auto measure = std::clamp<std::int64_t>(MeasureNearTime(timeline, state.TimeMilliseconds()) + direction, 0,
                                                  MaximumMeasureCount - 1);
    state.Seek(timeline.MeasureSpan(measure).begin.count() / 1000.0);
}

void TaikoEditorMode::DrawChart(IEditorModeCanvas &canvas, IEditorContext &state,
                                const TextCatalog &catalog)
{
    areas_ = canvas.Areas();
    noteHits_.clear();
    bodyHits_.clear();
    realtimeGrid_.clear();
    overviewGrid_.clear();
    if (state.Score().preview)
        DrawPreview(canvas, state);
    else if (state.Score().realtime)
        DrawRealtime(canvas, state, catalog);
    else
        DrawOverview(canvas, state, catalog);
}

namespace
{
    // Flat score colours (play's ambient note colours).
    constexpr Color ScoreDon = ui_colors::Rgb(0xF2404A), ScoreKat = ui_colors::Rgb(0x2E94F2),
                    ScorePurple = ui_colors::Rgb(0xA640E6), ScoreGold = ui_colors::Rgb(0xFFA31F);
    Color ScoreColor(int visualTool)
    {
        if (visualTool == 2 || visualTool == 4 || visualTool == 18)
            return ScoreKat;
        if (visualTool == 5)
            return ScorePurple;
        if (visualTool > 5 && visualTool < 17)
            return ScoreGold;
        return ScoreDon;
    }
    bool IsBig(int keyType)
    {
        const bool bigDonKat = keyType >= 3 && keyType <= 5;
        return bigDonKat || keyType == 13 || keyType == 14;
    }
} // namespace

const TaikoEditorMode::ScoreCell *TaikoEditorMode::ScoreCellNear(Point point) const
{
    // The cell whose row band (number band to lane band plus half the row gap) holds the point.
    // A bar line belongs to the measure it starts; the row's outer margins to its first and last cells.
    const ScoreCell *margin = nullptr;
    for (const auto &cell : scoreCells_)
    {
        const bool row = point.y >= cell.top && point.y <= cell.laneTop + cell.laneHeight + 5;
        if (!row)
            continue;
        if (point.x >= cell.x0 && point.x < cell.x0 + cell.width)
            return &cell;
        if (point.x >= cell.bandLeft && point.x <= cell.bandRight)
            margin = &cell;
    }
    return margin;
}

const TaikoEditorMode::ScoreCell *TaikoEditorMode::ScoreCellAt(std::int64_t measure) const
{
    const auto found = std::ranges::find(scoreCells_, measure, &ScoreCell::measure);
    return found == scoreCells_.end() ? nullptr : &*found;
}

std::optional<Point> TaikoEditorMode::ScorePoint(const MusicalTimeline &timeline, MusicalPosition position) const
{
    const auto *cell = ScoreCellAt(position.measure);
    if (!cell)
        return std::nullopt;
    const long double length = std::max(1e-9L, timeline.MeasureLength(position.measure).Value());
    return Point{cell->x0 + cell->width * static_cast<float>(position.fraction.Value() / length),
                 cell->laneTop + cell->laneHeight * .5F};
}

void TaikoEditorMode::DrawOverview(IEditorModeCanvas &canvas, IEditorContext &state, const TextCatalog &)
{
    // Score: the pattern alone, four measures per row with every beat the same width (taiko wiki
    // style). It follows the cursor and keeps one row above the current one.
    const auto &timeline = state.Document().Timeline();
    const auto area = areas_.score;
    constexpr float padX = 20, padY = 10, numberHeight = 16, gap = 10;
    const float lane = std::round(std::clamp((area.width - padX * 2) / MeasuresPerRow * .1F, 22.0F, 34.0F));
    const float pitch = numberHeight + lane + gap;
    const int rows = std::max(1, static_cast<int>((area.height - padY * 2 + gap) / pitch));
    scoreNormalRadius_ = lane * .5F * .74F;
    scoreBigRadius_ = lane * .5F * 1.02F;
    const float indent = std::round(scoreBigRadius_ + 4);
    const float cellWidth = (area.width - padX * 2 - indent * 2) / MeasuresPerRow;
    const auto current = MeasureNearTime(timeline, state.TimeMilliseconds());
    const auto first = std::max<std::int64_t>(0, current / MeasuresPerRow - 1) * MeasuresPerRow;
    const auto last = std::min<std::int64_t>(first + static_cast<std::int64_t>(rows) * MeasuresPerRow, MaximumMeasureCount);
    const int subdivisions = state.Score().division * 4;
    scoreCells_.clear();
    for (auto m = first; m < last; ++m)
    {
        const auto index = m - first;
        const auto column = static_cast<int>(index % MeasuresPerRow);
        const float x0 = area.x + padX + indent + cellWidth * static_cast<float>(column);
        const float top = area.y + padY + pitch * static_cast<float>(index / MeasuresPerRow);
        const float laneTop = top + numberHeight;
        const bool rowStart = column == 0, rowEnd = column == MeasuresPerRow - 1 || m == last - 1;
        const float bandLeft = rowStart ? area.x + padX : x0;
        const float bandRight = rowEnd ? x0 + cellWidth + indent : x0 + cellWidth;
        scoreCells_.push_back({m, x0, top, laneTop, cellWidth, lane, bandLeft, bandRight});
        canvas.Box({bandLeft, laneTop, bandRight - bandLeft, lane}, ui_colors::Rgb(0x141A2C));
        canvas.Box({bandLeft, laneTop, bandRight - bandLeft, 1}, {1, 1, 1, .07F});
        canvas.Box({bandLeft, laneTop + lane - 1, bandRight - bandLeft, 1}, {1, 1, 1, .07F});
        const long double length = std::max(1e-9L, timeline.MeasureLength(m).Value());
        const auto count = static_cast<int>(std::min(4096.0L, std::ceil(length * subdivisions)));
        // Beats and grid; positions cut away by a negative delay are washed red and do not snap.
        for (int i = 0; i < count; ++i)
        {
            const MusicalPosition p{m, {i, subdivisions}};
            const float x = x0 + cellWidth * static_cast<float>(p.fraction.Value() / length);
            if (!timeline.IsPositionActive(p))
            {
                const float next = x0 + cellWidth * static_cast<float>(std::min(1.0L, (i + 1.0L) / subdivisions / length));
                canvas.Box({x, laneTop, std::max(1.0F, next - x), lane}, ui_colors::Rgb(0xE5484D, .4F));
                continue;
            }
            overviewGrid_.push_back({{x, laneTop + lane * .5F}, p});
            if (i == 0)
                continue;
            const bool beat = i % state.Score().division == 0;
            canvas.Box({std::round(x), laneTop + (beat ? 0 : lane * .3F), 1, beat ? lane : lane * .4F},
                       {1, 1, 1, beat ? .22F : .08F});
        }
        const bool now = m == current;
        canvas.Box({std::round(x0), top + 2, now ? 2.0F : 1.5F, numberHeight + lane - 2},
                   now ? ui_colors::theme::Brand : ui_colors::Rgb(0x142F5E, .7F));
        canvas.Box({std::round(x0), laneTop, 2, lane}, White);
        canvas.Text({x0 + 5, top, 60, 14}, std::to_wstring(m + 1), now ? 14.0F : 13.0F,
                    now ? ui_colors::theme::Brand : ui_colors::Rgb(0x2A4269, .75F));
        if (now)
            canvas.Box({x0, laneTop + lane + 1, cellWidth, 3}, ui_colors::theme::Sun);
    }

    const auto visibleNotes = PlayableInSourceOrder(
        state.Document().NotesInTimeRange(timeline.MeasureSpan(first).begin, timeline.MeasureSpan(last).begin));
    DrawScoreBodies(canvas, state, visibleNotes);
    // Heads back to front, so an earlier note overlaps a later one as in the wiki scores.
    for (auto it = visibleNotes.rbegin(); it != visibleNotes.rend(); ++it)
    {
        const auto &n = it->note;
        if (n.actionType == 2)
            continue;
        const auto point = ScorePoint(timeline, n.position);
        if (!point)
            continue;
        const int visual = VisualTool(n);
        const float radius = IsBig(n.keyType) ? scoreBigRadius_ : scoreNormalRadius_;
        if (inspector_.IsSelected(n.sourceOrder) || state.SelectedNoteIds().contains(n.sourceOrder))
            canvas.Box({point->x - radius - 4, point->y - radius - 4, (radius + 4) * 2, (radius + 4) * 2},
                       ui_colors::theme::Sun, radius + 4);
        canvas.Box({point->x - radius - 1, point->y - radius - 1, (radius + 1) * 2, (radius + 1) * 2}, White, radius + 1);
        canvas.Box({point->x - radius + 1, point->y - radius + 1, (radius - 1) * 2, (radius - 1) * 2}, ScoreColor(visual),
                   radius - 1);
        if (n.keyType == 17)
            canvas.Box({point->x - radius * .2F, point->y - radius * .55F, radius * .4F, radius * 1.1F}, White, 1);
        noteHits_.push_back({*point, n.sourceOrder});
    }
    DrawScoreRangeAndCursor(canvas, state);
}

void TaikoEditorMode::DrawScoreBodies(IEditorModeCanvas &canvas, IEditorContext &state,
                                      const std::vector<CompiledPatternNote> &notes)
{
    // Long notes: a bar of the note's height through the cells, a round tail, and tick marks.
    const auto &timeline = state.Document().Timeline();
    std::optional<PatternNote> head;
    for (const auto &compiled : notes)
    {
        const auto &n = compiled.note;
        if (n.actionType == 1 && !head)
        {
            head = n;
            continue;
        }
        if (n.actionType != 2 || !head || head->keyType != n.keyType)
            continue;
        const float radius = IsBig(head->keyType) ? scoreBigRadius_ : scoreNormalRadius_;
        const float thickness = radius * 2 * .82F;
        const auto color = ScoreColor(VisualTool(*head));
        for (auto m = head->position.measure; m <= n.position.measure; ++m)
        {
            const auto *cell = ScoreCellAt(m);
            if (!cell)
                continue;
            const long double length = std::max(1e-9L, timeline.MeasureLength(m).Value());
            const float from = m == head->position.measure ? static_cast<float>(head->position.fraction.Value() / length) : 0;
            const float to = m == n.position.measure ? static_cast<float>(n.position.fraction.Value() / length) : 1;
            const float middle = cell->laneTop + cell->laneHeight * .5F;
            const Rect body{cell->x0 + cell->width * from, middle - thickness * .5F, cell->width * (to - from), thickness};
            canvas.Box(body, color);
            canvas.Box({body.x, body.y, body.width, 1.5F}, {1, 1, 1, .9F});
            canvas.Box({body.x, body.y + body.height - 1.5F, body.width, 1.5F}, {1, 1, 1, .9F});
            bodyHits_.push_back({body, head->sourceOrder});
        }
        const bool focus = head->keyType == 15 || head->keyType == 16;
        if (const auto tail = ScorePoint(timeline, n.position); tail && !focus)
            canvas.Box({tail->x - thickness * .5F, tail->y - thickness * .5F, thickness, thickness}, color, thickness * .5F);
        std::vector<Diagnostic> diagnostics;
        const bool ticked = UsesTickDivision(head->keyType);
        const auto division = ticked ? ReadPositiveOption(*head, TickDivision, 16, diagnostics) : std::nullopt;
        if (division)
        {
            const auto step = Rational{1, static_cast<std::int64_t>(*division)};
            auto position = timeline.AddWholeNotes(head->position, step);
            for (int count = 0; position <= n.position && count < 1024; ++count)
            {
                if (const auto tick = ScorePoint(timeline, position))
                {
                    if (head->keyType == 17)
                        canvas.Box({tick->x - 1.2F, tick->y - thickness * .38F, 2.4F, thickness * .76F}, White, 1);
                    else
                        canvas.Box({tick->x - 1.8F, tick->y - 1.8F, 3.6F, 3.6F}, White, 1.8F);
                }
                position = timeline.AddWholeNotes(position, step);
            }
        }
        head.reset();
    }
}

void TaikoEditorMode::DrawScoreRangeAndCursor(IEditorModeCanvas &canvas, IEditorContext &state)
{
    const auto &timeline = state.Document().Timeline();
    if (const auto &range = state.NoteSelectionRange())
        for (const auto &cell : scoreCells_)
        {
            if (cell.measure < range->first.measure || cell.measure > range->second.measure)
                continue;
            const long double length = std::max(1e-9L, timeline.MeasureLength(cell.measure).Value());
            const bool first = cell.measure == range->first.measure, last = cell.measure == range->second.measure;
            const float from = first ? static_cast<float>(range->first.fraction.Value() / length) : 0;
            const float to = last ? static_cast<float>(range->second.fraction.Value() / length) : 1;
            const float x = cell.x0 + cell.width * from, width = cell.width * (to - from);
            canvas.Box({x, cell.laneTop, width, cell.laneHeight}, {.3F, .61F, 1, .28F});
            if (first)
                canvas.Box({x - 1, cell.laneTop - 4, 2.5F, cell.laneHeight + 8}, ui_colors::theme::Sun);
            if (last)
                canvas.Box({x + width - 1.5F, cell.laneTop - 4, 2.5F, cell.laneHeight + 8}, ui_colors::theme::Sun);
        }
    // The playhead: the cursor's beat in its measure.
    const auto whole = timeline.WholeNotesAtTime(Microseconds(state.TimeMilliseconds()));
    if (whole < 0)
        return;
    const auto position = timeline.PositionAtWholeNotes(Rational{static_cast<std::int64_t>(std::floor(whole * 3840)), 3840});
    if (const auto point = ScorePoint(timeline, position))
    {
        const auto *cell = ScoreCellAt(position.measure);
        canvas.Box({point->x - 1, cell->laneTop - 4, 2.5F, cell->laneHeight + 8}, ui_colors::theme::Sun);
    }
}

void TaikoEditorMode::DrawPreview(IEditorModeCanvas &canvas, IEditorContext &state)
{
    // Preview: the play screen's gear and lane with Whole/Separate scroll and bar-line visibility
    // applied, an automatic performance on the keys, and no editing marks. It is read-only.
    const auto &document = state.Document();
    const auto &timeline = document.Timeline();
    const auto area = areas_.score;
    constexpr float k = editor_layout::LaneScale, laneHeight = editor_layout::LaneHeight;
    canvas.Box(area, ui_colors::Rgb(0xBCD3F7));
    if (!areas_.background.empty())
        canvas.Image(area, areas_.background, White);
    canvas.Box(area, ui_colors::Rgb(0xF0F8FF, .32F));
    const float gearTop = std::round(area.y + area.height * .5F - 160);
    const float panelTop = gearTop + 4, panelLeft = area.x + 28;
    const Rect lane{panelLeft + laneHeight, panelTop, area.x + area.width - panelLeft - laneHeight, laneHeight};
    const float judgeX = lane.x + laneHeight * .5F, centerY = panelTop + laneHeight * .5F;
    canvas.Box({area.x, gearTop, area.width, 160}, ui_colors::theme::Brand);
    canvas.Image({panelLeft, panelTop, laneHeight, laneHeight}, SkinImage(L"InputPanel"), ui_colors::Rgb(0xBAD1ED));
    canvas.Box(lane, ui_colors::Rgb(0x080818));

    const auto now = Microseconds(state.TimeMilliseconds());
    const auto axis = EditorTimeAxis::Realtime(timeline, 1, judgeX, state.TimeMilliseconds());
    // Distance: Whole integrated over time (as in play), times each note's fixed Separate multiplier.
    const auto xAt = [&](RhythmTime time, double separate) {
        return judgeX + static_cast<float>(document.ScrollDistance(time, now) * axis.pixelsPerMicrosecond * separate);
    };
    const auto visibleEnd = now + RhythmDuration{10'000'000};
    for (auto m = MeasureNearTime(timeline, state.TimeMilliseconds()); m < MaximumMeasureCount; ++m)
    {
        const auto span = timeline.MeasureSpan(m);
        if (span.begin > visibleEnd)
            break;
        if (span.end <= span.begin || span.begin < now)
            continue;
        if (document.EffectValueAt(EffectCommandType::MeasureLineVisible, span.sourceBegin) < .5)
            continue;
        const float x = xAt(span.begin, document.EffectValueAt(EffectCommandType::NoteSpeed, span.sourceBegin));
        if (x > lane.x + lane.width)
            break;
        canvas.Box({x - 1, lane.y, 2, lane.height}, ui_colors::Rgb(0xC7D9E6, .5F));
    }
    const float judgement = 144 * k;
    canvas.Image({judgeX - judgement * .5F, centerY - judgement * .5F, judgement, judgement}, SkinImage(L"JudgementCircle"),
                 White);

    // The automatic performance: notes reached in the last 90 ms light their keys; the combo counts them.
    const auto notes = PlayableInSourceOrder(document.NotesInTimeRange(RhythmTime{std::numeric_limits<RhythmTime::rep>::min() / 2},
                                                                       visibleEnd));
    std::array<bool, 4> lit{};
    int combo = 0;
    std::optional<RhythmTime> lastHit;
    for (const auto &note : notes)
    {
        if (note.note.actionType == 2 || note.timing > now)
            continue;
        ++combo;
        lastHit = note.timing;
        if (now - note.timing >= RhythmDuration{90'000})
            continue;
        const bool kat = note.note.keyType == 2 || note.note.keyType == 4;
        const bool both = IsBig(note.note.keyType);
        const bool left = note.note.sourceOrder % 2 == 1;
        if (both || left)
            lit[kat ? 0 : 1] = true;
        if (both || !left)
            lit[kat ? 3 : 2] = true;
    }
    const auto faces = TaikoPresentation.keyOverlay->faces;
    for (std::size_t i = 0; i < faces.size() && i < lit.size(); ++i)
    {
        const auto &face = faces[i];
        const Rect bounds{panelLeft + face.sourceBounds.x * k, panelTop + face.sourceBounds.y * k,
                          face.sourceBounds.width * k, face.sourceBounds.height * k};
        auto tint = face.tint;
        if (lit[i])
        {
            canvas.Image({bounds.x - 10 * k * .5F, bounds.y - 10 * k * .5F, 49 * k, 80 * k}, SkinImage(L"KeyLight"), face.tint);
            tint = {std::min(1.0F, tint.red * 1.38F + .1F), std::min(1.0F, tint.green * 1.38F + .1F),
                    std::min(1.0F, tint.blue * 1.38F + .1F), 1};
        }
        canvas.Image(bounds, SkinImage(std::wstring(face.skinImage.begin(), face.skinImage.end())), tint);
    }
    // Combo under the keys, centred on the panel.
    const auto comboText = std::to_wstring(combo);
    const float comboWidth = EstimateTextWidth(comboText, 26);
    canvas.Text({panelLeft + (laneHeight - comboWidth) * .5F, panelTop + 104, comboWidth + 4, 30}, comboText, 26,
                ui_colors::theme::Ink);
    canvas.Text({panelLeft + (laneHeight - EstimateTextWidth(L"Combo", 12)) * .5F, panelTop + 134, 60, 16}, L"Combo", 12,
                ui_colors::theme::InkSoft);

    // Notes: passed ones disappear; a long note's head waits at the judgement circle while it lasts.
    std::optional<std::pair<const CompiledPatternNote *, float>> open;
    std::vector<std::pair<const CompiledPatternNote *, float>> heads;
    for (const auto &note : notes)
    {
        const auto &n = note.note;
        if (n.actionType == 1)
        {
            open.emplace(&note, xAt(note.timing, note.scrollMultiplier));
            continue;
        }
        if (n.actionType == 2 && open)
        {
            const auto *head = open->first;
            const float end = xAt(note.timing, head->scrollMultiplier);
            if (note.timing >= now)
            {
                const float start = std::max(judgeX, open->second);
                const bool big = head->note.keyType == 13 || head->note.keyType == 14;
                const float thickness = EditorTimeAxis::NormalHeadDiameter * (big ? 1.5F : 1) * .75F;
                if (end > start && head->note.keyType != 15 && head->note.keyType != 16)
                    canvas.Box({start, centerY - thickness * .5F, std::min(end, lane.x + lane.width) - start, thickness},
                               ui_colors::Rgb(0xFFA31F), thickness * .5F);
                heads.emplace_back(head, start);
            }
            open.reset();
            continue;
        }
        if (note.timing >= now)
            heads.emplace_back(&note, xAt(note.timing, note.scrollMultiplier));
    }
    for (auto it = heads.rbegin(); it != heads.rend(); ++it)
    {
        const auto &[note, x] = *it;
        const float radius = EditorTimeAxis::NormalHeadDiameter * .5F * (IsBig(note->note.keyType) ? 1.5F : 1);
        if (x - radius > lane.x + lane.width || x + radius < lane.x)
            continue;
        Circle(canvas, x, centerY, VisualTool(note->note), radius);
    }
    if (lastHit && now - *lastHit < RhythmDuration{300'000})
    {
        const float fade = 1 - static_cast<float>((now - *lastHit).count()) / 300'000.0F;
        canvas.Text({judgeX - 40, panelTop + 6, 80, 26}, L"MAX", 22, {1, 1, 1, fade});
    }
}

void TaikoEditorMode::DrawRealtime(IEditorModeCanvas &canvas, IEditorContext &state,
                                   const TextCatalog &catalog)
{
    const auto labels = TaikoEditor(catalog);
    const auto &timeline = state.Document().Timeline();
    const auto lane = areas_.lane;
    const float right = lane.x + lane.width, centerY = lane.y + lane.height * .5F;
    const int subdivisionsPerWholeNote = state.Score().division * 4;
    const float pixelScale = std::max(.001F, canvas.PixelScale());
    // Bar lines are 5 and grid ticks 2 device pixels wide at any window size.
    const float tickWidth = 2.0F / pixelScale, measureWidth = 5.0F / pixelScale;
    // The editor's time axis stays uniform across Whole and Separate speed effects.
    const auto axis =
        EditorTimeAxis::Realtime(timeline, state.Score().realtimeZoom, areas_.judgeX, state.TimeMilliseconds());
    const float normalRadius = EditorTimeAxis::NormalHeadDiameter * .5F, bigRadius = normalRadius * 1.5F;
    canvas.Box(lane, {.031F, .031F, .094F, 1});
    if (const auto &range = state.NoteSelectionRange(); range)
    {
        const float first = axis.X(timeline.Compile(range->first));
        const float last = axis.X(timeline.Compile(range->second));
        const float left = std::clamp(std::min(first, last), lane.x, right);
        const float end = std::clamp(std::max(first, last), lane.x, right);
        canvas.Box({left, lane.y, std::max(2.0F, end - left), lane.height}, {.3F, .61F, 1, .18F});
    }
    const auto currentMeasure = MeasureNearTime(timeline, state.TimeMilliseconds());
    const auto gridBegin = std::max<std::int64_t>(0, currentMeasure - 1);
    const auto visibleBegin = axis.TimeAt(lane.x - bigRadius);
    const auto visibleEnd = axis.TimeAt(right + bigRadius);
    // Drawing and input share the same visible snap positions.
    for (std::int64_t m = gridBegin; m < std::min(currentMeasure + 64, MaximumMeasureCount); ++m)
    {
        const int count = static_cast<int>(
            std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
        const auto span = timeline.MeasureSpan(m);
        if (span.begin > visibleEnd)
            break;
        if (span.end < visibleBegin)
            continue;
        const float barX = axis.X(span.begin);
        const bool barInLane = barX >= lane.x && barX <= right;
        if (span.end > span.begin && barInLane &&
            state.Document().EffectValueAt(EffectCommandType::MeasureLineVisible, span.sourceBegin) >= .5)
            canvas.Box({barX - measureWidth * .5F, lane.y, measureWidth, lane.height}, {1, 1, 1, .85F});
        float lastDrawnX = -std::numeric_limits<float>::infinity();
        for (int i = 0; i < count; ++i)
        {
            MusicalPosition p{m, {i, subdivisionsPerWholeNote}};
            if (!timeline.IsPositionActive(p))
                continue;
            const float x = axis.X(timeline.Compile(p));
            if (x < lane.x || x > right)
                continue;
            if (i != 0 && std::abs(x - lastDrawnX) >= tickWidth)
            {
                auto color = RealtimeTickColor(p.fraction);
                color.alpha = .55F;
                canvas.Box({x - tickWidth * .5F, lane.y + lane.height * .18F, tickWidth, lane.height * .64F}, color);
                lastDrawnX = x;
            }
            realtimeGrid_.push_back({x, p});
        }
    }
    const float judgement = 144 * editor_layout::LaneScale;
    canvas.Image({areas_.judgeX - judgement * .5F, centerY - judgement * .5F, judgement, judgement},
                 SkinImage(L"JudgementCircle"), White);
    std::optional<float> headX;
    std::optional<PatternNote> realtimeHead;
    const auto visibleNotes = PlayableInSourceOrder(state.Document().NotesInTimeRange(visibleBegin, visibleEnd));
    // Borrow from this frame's snapshot only until the head pass below.
    std::vector<std::pair<const CompiledPatternNote *, float>> heads;
    for (const auto &n : visibleNotes)
    {
        const float x = axis.X(n.timing);
        if (n.note.actionType == 1 && !headX)
        {
            headX = x;
            realtimeHead = n.note;
        }
        if (n.note.actionType == 2 && headX && realtimeHead->keyType == n.note.keyType)
        {
            const float left = std::max(*headX, lane.x), end = std::min(x, right);
            const bool big = realtimeHead->keyType == 13 || realtimeHead->keyType == 14;
            const float thickness = (big ? bigRadius : normalRadius) * 1.5F;
            if (end > left)
            {
                const Rect body{left, centerY - thickness * .5F, end - left, thickness};
                canvas.Box(body, Gold, thickness * .5F);
                if (realtimeHead)
                    bodyHits_.push_back({body, realtimeHead->sourceOrder});
            }
            if (realtimeHead && *headX >= lane.x && *headX <= right - 120)
                canvas.Text({*headX - 40, centerY - (big ? bigRadius : normalRadius) - 22, 220, 18},
                            LongNoteCaption(state.Document(), *realtimeHead, n.note, labels), 12, White);
            headX.reset();
        }
        heads.emplace_back(&n, x);
    }
    for (const auto &[source, x] : heads)
    {
        const auto &n = *source;
        const bool big = IsBig(n.note.keyType);
        const float radius = big ? bigRadius : normalRadius;
        if (x < lane.x + radius || x > right - radius)
            continue;
        if (inspector_.IsSelected(n.note.sourceOrder) || state.SelectedNoteIds().contains(n.note.sourceOrder))
            canvas.Box({x - radius - 5, centerY - radius - 5, (radius + 5) * 2, (radius + 5) * 2},
                       ui_colors::theme::Sun, radius + 5);
        Circle(canvas, x, centerY, VisualTool(n.note), radius);
        noteHits_.push_back({{x, centerY}, n.note.sourceOrder});
    }
}

void TaikoEditorMode::EditScore(IEditorContext &state, Point point, bool erase)
{
    if (state.Score().preview || !(state.Score().realtime ? areas_.lane : areas_.score).Contains(point))
        return;
    const auto nearest = std::ranges::min_element(noteHits_, {}, [point](const NoteHit &n)
                                                  { return std::hypot(n.point.x - point.x, n.point.y - point.y); });
    const float hitRadius = state.Score().realtime ? EditorTimeAxis::NormalHeadDiameter * .5F + 2 : scoreBigRadius_ + 2;
    const bool hitsNote =
        nearest != noteHits_.end() && std::hypot(nearest->point.x - point.x, nearest->point.y - point.y) < hitRadius;
    if (erase || tool_ == 0)
    {
        if (hitsNote)
        {
            if (erase)
                state.DeleteNote(nearest->order);
            else
            {
                inspector_.Select(state.Document(), nearest->order);
                const std::array ids{nearest->order};
                state.SelectNotes(ids);
            }
            state.RequestRebuild();
            return;
        }
        if (erase)
            return;
        const auto body =
            std::ranges::find_if(bodyHits_, [point](const auto &hit) { return hit.bounds.Contains(point); });
        if (body != bodyHits_.end())
        {
            inspector_.Select(state.Document(), body->order);
            const std::array ids{body->order};
            state.SelectNotes(ids);
            state.RequestRebuild();
            return;
        }
        if (inspector_.Clear())
            state.RequestRebuild();
        state.SelectNotes({});
    }
    MusicalPosition p;
    if (state.Score().realtime)
    {
        if (realtimeGrid_.empty())
            return;
        p = std::ranges::min_element(realtimeGrid_, {}, [point](const auto &g) { return std::abs(g.first - point.x); })
                ->second;
    }
    else
    {
        const auto *cell = ScoreCellNear(point);
        if (!cell || point.x < cell->bandLeft || point.x > cell->bandRight)
            return;
        const auto nearestGrid = std::ranges::min_element(overviewGrid_, {},
                                                          [&](const auto &g)
                                                          {
                                                              return g.second.measure == cell->measure
                                                                         ? std::abs(g.first.x - point.x)
                                                                         : std::numeric_limits<float>::infinity();
                                                          });
        if (nearestGrid == overviewGrid_.end() || nearestGrid->second.measure != cell->measure)
            return;
        p = nearestGrid->second;
    }
    PlaceNote(state, p);
}

int TaikoEditorMode::VisualTool(const PatternNote &note)
{

    if (note.keyType == static_cast<int>(TaikoNoteType::Buzz))
    {
        std::vector<Diagnostic> diagnostics;
        if (ReadBuzzAction(note, diagnostics) == TaikoAction::Kat)
            return 18;
    }
    return note.keyType;
}

std::wstring TaikoEditorMode::LongNoteCaption(const IEditorDocument &document, const PatternNote &head,
                                              const PatternNote &tail,
                                              const TaikoEditorTextSet &text) const
{

    const int id = VisualTool(head);
    const std::wstring name(Name(id, text.toolVariants));
    std::vector<Diagnostic> diagnostics;
    if (UsesTickDivision(id))
    {
        const auto division = ReadPositiveOption(head, TickDivision, 16, diagnostics);
        // Persisted TickDivision divides a whole note; a quarter-note beat is 1/4.
        return name + L" | " + std::wstring(text.tickInterval) + L" " +
               (division ? Wide(Fraction(Rational{4, static_cast<std::int64_t>(*division)})) + L" " +
                               std::wstring(text.beats)
                         : L"?");
    }
    const auto count = ReadHitCount(
        head, std::max(std::size_t{1}, document.Timeline().CountSubdivisions(head.position, tail.position, 12)),
        diagnostics);
    return name + L" | " + (count ? std::to_wstring(*count) : L"?") + L" " + std::wstring(text.hits);
}

const std::filesystem::path &TaikoEditorMode::SkinImage(std::wstring_view file)
{
    // Path resolution can probe external skin files. Do it once per selected
    // skin revision; the common renderer continues to own/caches image handles.
    const auto revision = SkinSetSelection::Instance().Revision();
    if (skinRevision_ != revision)
    {
        skinImagePaths_.clear();
        skinRevision_ = revision;
    }
    const auto cached = skinImagePaths_.find(file);
    if (cached != skinImagePaths_.end())
        return cached->second;
    return skinImagePaths_.emplace(std::wstring(file), InGame(file)).first->second;
}

void TaikoEditorMode::Circle(IEditorModeCanvas &canvas, float x, float y, int type, float radius)
{
    const bool custom = type == 15 || type == 16;
    const bool big = IsBig(type);
    const auto color = type == 2 || type == 4 || type == 18 ? Kat
                       : type == 5                          ? Color{.65F, .30F, .85F, 1}
                       : type > 5 && type < 17              ? Gold
                                                            : Don;
    const auto file = type == 15 ? L"Balloon" : type == 16 ? L"DengDeng" : big ? L"bignote" : L"note";
    // Balloon/DengDeng art is the plate as its top square with the knot or handle below (128 x 160).
    const float height = custom ? radius * 2.5F : radius * 2;
    const Rect bounds{x - radius, y - radius, radius * 2, height};
    canvas.Image(bounds, SkinImage(file), custom ? White : color);
    if (!custom)
        canvas.Image(bounds, SkinImage(big ? L"bigcircleoverlay" : L"noteoverlay"), White);
}
