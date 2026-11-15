#include "../../EditorSupport.h"
#include "../../EditorTimeAxis.h"
#include "App/AssetPaths.h"
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

    float OverviewX(const ScoreGrid &grid, const MusicalTimeline &timeline, std::int64_t firstMeasure,
                    std::int64_t measure, RhythmTime time)
    {
        const auto span = timeline.MeasureSpan(measure);
        const long double duration = static_cast<long double>(span.end.count()) - span.begin.count();
        const long double elapsed = static_cast<long double>(time.count()) - span.begin.count();
        return grid.left + static_cast<float>((measure - firstMeasure) % MeasuresPerRow) * grid.measureWidth +
               static_cast<float>(elapsed / std::max(1.0L, duration)) * grid.measureWidth;
    }

    std::wstring MeasureCaption(std::int64_t measure, long double wholeNotes)
    {
        std::wostringstream text;
        text.imbue(std::locale::classic());
        text << measure + 1 << L"  " << wholeNotes * 4 << L"/4";
        return text.str();
    }

} // namespace

void TaikoEditorMode::ScrollScore(IEditorContext &state, int direction)
{
    if (state.Score().realtime)
    {
        state.Seek(StepEditorTimeline(state.Document().Timeline(), state.TimeMilliseconds(),
                                      state.Score().division, direction).count() / 1000.0);
        return;
    }
    state.SetFirstMeasure(std::clamp<std::int64_t>(state.Score().firstMeasure + direction * MeasuresPerRow, 0,
                                                   MaximumMeasureCount - 1));
}

void TaikoEditorMode::DrawChart(IEditorModeCanvas &canvas, IEditorContext &state,
                                const TextCatalog &catalog)
{
    areas_ = canvas.Areas();
    noteHits_.clear();
    bodyHits_.clear();
    realtimeGrid_.clear();
    overviewGrid_.clear();
    if (state.Score().realtime)
        DrawRealtime(canvas, state, catalog);
    else
        DrawOverview(canvas, state, catalog);
}

void TaikoEditorMode::DrawOverview(IEditorModeCanvas &canvas, IEditorContext &state,
                                   const TextCatalog &catalog)
{
    const auto labels = TaikoEditor(catalog);
    const auto grid = ScoreGridFor(areas_.score);
    const auto &timeline = state.Document().Timeline();
    const int subdivisionsPerWholeNote = state.Score().division * 4;
    const auto xAt = [&](MusicalPosition p)
    { return OverviewX(grid, timeline, state.Score().firstMeasure, p.measure, timeline.Compile(p)); };
    const auto displayPosition = [&](const CompiledPatternNote &note)
    {
        const auto measure = MeasureNearTime(timeline, note.timing.count() / 1000.0);
        return std::pair{measure, OverviewX(grid, timeline, state.Score().firstMeasure, measure, note.timing)};
    };
    // Source fractions stay exact; screen positions use the surviving measure clock.
    for (int row = 0; row < VisibleRows; ++row)
    {
        const float y = grid.rowTop + row * grid.rowHeight;
        canvas.Box({grid.left, y, grid.measureWidth * MeasuresPerRow, StaffHeight}, {.957F, .984F, 1, 1}, 3);
        for (int col = 0; col < MeasuresPerRow; ++col)
        {
            const auto m = state.Score().firstMeasure + row * MeasuresPerRow + col;
            if (m >= MaximumMeasureCount)
                break;
            const float x = grid.left + col * grid.measureWidth;
            const auto span = timeline.MeasureSpan(m);
            if (const auto &range = state.NoteSelectionRange();
                range && m >= range->first.measure && m <= range->second.measure)
            {
                const float left =
                    m == range->first.measure ? std::clamp(xAt(range->first), x, x + grid.measureWidth) : x;
                const float right = m == range->second.measure
                                        ? std::clamp(xAt(range->second), x, x + grid.measureWidth)
                                        : x + grid.measureWidth;
                canvas.Box({left, y, std::max(2.0F, right - left), StaffHeight}, {.2F, .5F, 1, .18F});
            }
            canvas.Text({x, y - 22, 140, 18}, MeasureCaption(m, span.wholeNotes), 12);
            if (span.end <= span.begin)
                continue;
            canvas.Box({x, y, 2, StaffHeight}, Blue);
            const auto count = static_cast<int>(
                std::min(4096.0L, std::ceil(timeline.MeasureLength(m).Value() * subdivisionsPerWholeNote)));
            // Subpixel helper lines share a visible line; snapping still uses the full division.
            const int drawStride =
                std::max(1, static_cast<int>(std::ceil(count / static_cast<double>(grid.measureWidth))));
            for (int i = 0; i < count; ++i)
            {
                const MusicalPosition p{m, {i, subdivisionsPerWholeNote}};
                if (!timeline.IsPositionActive(p))
                    continue;
                const float gx = xAt(p);
                overviewGrid_.push_back({{gx, y}, p});
                if (i % drawStride == 0 && gx > x)
                    canvas.Box({gx, y, 1, StaffHeight}, Pale);
            }
        }
    }
    std::optional<CompiledPatternNote> head;
    const auto visibleNotes = PlayableInSourceOrder(state.Document().NotesInTimeRange(
        timeline.MeasureSpan(state.Score().firstMeasure).begin,
        timeline
            .MeasureSpan(
                std::min(state.Score().firstMeasure + MeasuresPerRow * VisibleRows, MaximumMeasureCount))
            .begin));
    // Interval bodies precede skin heads so the body cannot cover their artwork.
    for (const auto &note : visibleNotes)
    {
        const auto &n = note.note;
        if (n.actionType == 1 && !head)
            head = note;
        if (n.actionType == 2 && head && head->note.keyType == n.keyType)
        {
            const auto [headMeasure, headAt] = displayPosition(*head);
            const auto [tailMeasure, tailAt] = displayPosition(note);
            for (int row = 0; row < VisibleRows; ++row)
            {
                const auto begin = state.Score().firstMeasure + row * MeasuresPerRow, end = begin + MeasuresPerRow;
                if (tailMeasure < begin || headMeasure >= end)
                    continue;
                const float left = headMeasure < begin ? grid.left : headAt,
                            right = tailMeasure >= end ? grid.Right() : tailAt;
                const Rect body{left, grid.rowTop + 16 + row * grid.rowHeight, std::max(0.0F, right - left), 16};
                canvas.Box(body, Gold, 8);
                bodyHits_.push_back({body, head->note.sourceOrder});
            }
            if (headMeasure >= state.Score().firstMeasure &&
                headMeasure < state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
                canvas.Text({headAt + 16,
                             grid.rowTop + static_cast<float>((headMeasure - state.Score().firstMeasure) / MeasuresPerRow) *
                                               grid.rowHeight,
                             std::max(0.0F, grid.Right() - headAt - 18), 18},
                            LongNoteCaption(state.Document(), head->note, n, labels), 12);
            head.reset();
        }
    }
    for (const auto &note : visibleNotes)
    {
        const auto &n = note.note;
        const auto [measure, x] = displayPosition(note);
        if (measure < state.Score().firstMeasure ||
            measure >= state.Score().firstMeasure + MeasuresPerRow * VisibleRows)
            continue;
        const float y = grid.rowTop + 24 +
                        static_cast<float>((measure - state.Score().firstMeasure) / MeasuresPerRow) * grid.rowHeight;
        if (inspector_.IsSelected(n.sourceOrder) || state.SelectedNoteIds().contains(n.sourceOrder))
            canvas.Box({x - 19, y - 19, 38, 38}, ui_colors::theme::Sun, 19);
        Circle(canvas, x, y, VisualTool(n),
               (n.keyType >= 3 && n.keyType <= 5) || n.keyType == 13 || n.keyType == 14 ? 15.0F : 10.0F);
        noteHits_.push_back({{x, y}, n.sourceOrder});
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
    const float tickWidth = std::max(2.0F * editor_layout::LaneScale, 1.0F / pixelScale);
    const float measureWidth = std::max(5.0F * editor_layout::LaneScale, 2.0F / pixelScale);
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
        const bool big = (n.note.keyType >= 3 && n.note.keyType <= 5) || n.note.keyType == 13 || n.note.keyType == 14;
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
    const auto grid = ScoreGridFor(areas_.score);
    if (!(state.Score().realtime ? areas_.lane : grid.Bounds()).Contains(point))
        return;
    const auto nearest = std::ranges::min_element(noteHits_, {}, [point](const NoteHit &n)
                                                  { return std::hypot(n.point.x - point.x, n.point.y - point.y); });
    const float hitRadius = state.Score().realtime ? EditorTimeAxis::NormalHeadDiameter * .5F + 2 : 24;
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
        const int row = static_cast<int>((point.y - grid.rowTop) / grid.rowHeight),
                  col = static_cast<int>((point.x - grid.left) / grid.measureWidth);
        if (point.y - (grid.rowTop + row * grid.rowHeight) > StaffHeight)
            return;
        p.measure = state.Score().firstMeasure + row * MeasuresPerRow + col;
        if (p.measure >= MaximumMeasureCount)
            return;
        const auto nearestGrid = std::ranges::min_element(overviewGrid_, {},
                                                          [&](const auto &g)
                                                          {
                                                              return g.second.measure == p.measure
                                                                         ? std::abs(g.first.x - point.x)
                                                                         : std::numeric_limits<float>::infinity();
                                                          });
        if (nearestGrid == overviewGrid_.end() || nearestGrid->second.measure != p.measure)
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
    const bool big = (type >= 3 && type <= 5) || type == 13 || type == 14;
    const auto color = type == 2 || type == 4 || type == 18 ? Kat
                       : type == 5                          ? Color{.65F, .30F, .85F, 1}
                       : type > 5 && type < 17              ? Gold
                                                            : Don;
    const auto file = type == 15 ? L"Balloon" : type == 16 ? L"DengDeng" : big ? L"bignote" : L"note";
    const Rect bounds{x - radius, y - radius, radius * 2, radius * 2};
    canvas.Image(bounds, SkinImage(file), custom ? White : color);
    if (!custom)
        canvas.Image(bounds, SkinImage(big ? L"bigcircleoverlay" : L"noteoverlay"), White);
}
