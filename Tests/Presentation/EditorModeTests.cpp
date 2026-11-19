#include "EditorModeTests.h"
#include "Editing/ChartEditor.h"
#include "EditorScene/Submodules/EditorPatternCreation.h"
#include "EditorScene/Submodules/EditorView.h"
#include "EditorScene/Submodules/Modes/EditorModeFactory.h"
#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorMode.h"
#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorNoteInspector.h"
#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorTool.h"
#include "Parsing/ChartParser.h"
#include "Taiko/TaikoMode.h"
#include "Texts/EditorScene/Taiko/TaikoEditorTexts.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <Windows.h>

using namespace PhantomCore;

namespace
{
    const TextCatalog &TestCatalog(Language language)
    {
        struct Catalogs
        {
            TextCatalog ko, en;
            Catalogs()
            {
                en.SetLanguage(Language::English);
            }
        };
        static const Catalogs catalogs;
        return language == Language::Korean ? catalogs.ko : catalogs.en;
    }
} // namespace
namespace
{
    // Default areas lent to a mode: lane at y 160..312 with the judgement line at 184 + 152 * .62.
    // Score (cursor in the first row): bar lines from x 40, measures 300 wide, the first lane band
    // centred at y 173.5 (31 high below a 16-high number band, 10 below the area top 132).
    constexpr float JudgeX = 184 + 152 * .62F, LaneY = 236, ScoreLeft = 40, MeasureWidth = 300, ScoreY = 173.5F;
    // At spacing 1 a whole note at Base BPM spans 16 sixteenths of 85% of the 128 * 152 / 228 head.
    constexpr float WholeNotePixels = 128 * (152 / 228.0F) * 16 * .85F;

    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    class RecordingCanvas final : public IEditorModeCanvas
    {
      public:
        std::vector<Rect> rectangles;
        std::vector<Color> rectangleColors;
        std::vector<bool> selectedButtons;
        float deviceScale{1};
        float PixelScale() const noexcept override { return deviceScale; }
        std::vector<std::function<void()>> actions;
        std::vector<std::filesystem::path> images;
        std::vector<Rect> imageRectangles;
        std::vector<std::wstring> labels;
        std::vector<std::wstring> buttonLabels;
        void Box(Rect r, Color color, float) override
        {
            rectangles.push_back(r);
            rectangleColors.push_back(color);
        }
        void Text(Rect, std::wstring text, float, Color) override
        {
            labels.push_back(std::move(text));
        }
        void Image(Rect rect, const std::filesystem::path &path, Color color) override
        {
            rectangles.push_back(rect);
            rectangleColors.push_back(color);
            images.push_back(path);
            imageRectangles.push_back(rect);
        }
        void Button(Rect, std::wstring text, std::function<void()> action, bool selected) override
        {
            actions.push_back(std::move(action));
            selectedButtons.push_back(selected);
            buttonLabels.push_back(text);
            labels.push_back(std::move(text));
        }
    };

    // A test-only format adapter demonstrates that the workspace neither
    // constructs ChartEditor nor forces YMP serialization during Save.
    class ProbeDocument final : public IEditorDocument
    {
      public:
        ChartEditor score{PatternDocument{}};
        bool saved{};
        const PatternDocument &Pattern() const noexcept override
        {
            return score.Pattern();
        }
        const EffectDocument &Effects() const noexcept override
        {
            return score.Effects();
        }
        const MusicalTimeline &Timeline() const noexcept override
        {
            return score.Timeline();
        }
        const std::vector<CompiledPatternNote> &Notes() const noexcept override
        {
            return score.Notes();
        }
        bool Dirty() const noexcept override
        {
            return score.Dirty();
        }
        std::vector<CompiledPatternNote> NotesInMeasures(std::int64_t begin, std::int64_t end) const override
        {
            return score.NotesInMeasures(begin, end);
        }
        std::vector<CompiledPatternNote> NotesInTimeRange(RhythmTime begin,
                                                                 RhythmTime end) const override
        {
            return score.NotesInTimeRange(begin, end);
        }
        std::uint64_t Revision() const noexcept override
        {
            return score.Revision();
        }
        void Replace(PatternDocument p, EffectDocument e) override
        {
            score.Replace(std::move(p), std::move(e));
        }
        void AddNote(MusicalPosition p, int key, std::optional<MusicalPosition> end,
                     std::vector<std::string> extra, std::span<const std::size_t> replaced) override
        {
            score.AddNote(p, key, end, std::move(extra), replaced);
        }
        void ReplaceTiming(std::vector<TimingDirective> timing) override
        {
            score.ReplaceTiming(std::move(timing));
        }
        void ReplaceEffects(EffectDocument effects) override
        {
            score.ReplaceEffects(std::move(effects));
        }
        std::optional<EditorNoteSelection> FindNote(std::size_t id) const override
        {
            return score.FindNote(id);
        }
        void UpdateNote(std::size_t id, PatternNote note, std::optional<MusicalPosition> end) override
        {
            score.UpdateNote(id, std::move(note), end);
        }
        void DeleteNote(std::size_t order) override
        {
            score.DeleteNote(order);
        }
        void SetMeasureLength(std::int64_t measure, Rational length) override
        {
            score.SetMeasureLength(measure, length);
        }
        void Save() override
        {
            saved = true;
        }
    };
    class ProbeMode final : public IEditorMode
    {
      public:
        int selected{42};
        mutable int markerCalls{};
        bool failDrawing{};
        int chartCalls{};
        std::map<std::string, std::filesystem::path> audioFiles;
        std::string_view Id() const noexcept override
        {
            return "TestOnly";
        }
        std::unique_ptr<IEditorDocument> OpenDocument(const GameplayLaunchRequest &) const override
        {
            return std::make_unique<ProbeDocument>();
        }
        void SelectTool(int id) override
        {
            selected = id;
        }
        void PlaceNote(IEditorContext &state, MusicalPosition p) override
        {
            state.AddNote(p, selected);
            state.RequestRebuild();
        }
        bool HasPendingPlacement() const noexcept override
        {
            return false;
        }
        bool CancelInteraction() noexcept override
        {
            return false;
        }
        void CloseToolMenu() noexcept override
        {
        }
        bool OpenToolMenu(Point) override
        {
            return false;
        }
        void DrawTools(IEditorModeCanvas &canvas, IEditorContext &state,
                       const TextCatalog &) override
        {
            canvas.Button({30, 130, 60, 40}, L"PROBE TOOL", [&state] { state.SelectTool(70); });
        }
        void DrawChart(IEditorModeCanvas &canvas, IEditorContext &, const TextCatalog &) override
        {
            ++chartCalls;
            if (failDrawing)
                throw std::runtime_error("Injected chart drawing failure.");
            // Vertical rectangular lanes instead of Taiko circles/horizontal lane.
            canvas.Box({600, 200, 40, 500}, {.2F, .8F, .4F, 1});
            canvas.Box({650, 200, 40, 500}, {.2F, .4F, .8F, 1});
            canvas.Text({600, 160, 200, 30}, L"PROBE CHART");
        }
        void DrawToolMenu(IEditorModeCanvas &, IEditorContext &, const TextCatalog &) override
        {
        }
        void EditScore(IEditorContext &state, Point point, bool erase) override
        {
            if (erase)
                state.DeleteNote(0);
            else
                PlaceNote(state, {static_cast<std::int64_t>(point.y / 100), {}});
        }
        void ScrollScore(IEditorContext &state, int direction) override
        {
            state.SetFirstMeasure(state.Score().firstMeasure + direction);
        }
        void DrawMetadata(IEditorModeCanvas &canvas, IEditorContext &, const TextCatalog &) override
        {
            canvas.Text({600, 200, 300, 30}, L"PROBE FORMAT METADATA");
        }
        std::vector<EditorSoundChoice> SoundChoices(const TextCatalog &) const override
        {
            return {{42, L"Probe key", L"PROBE SOUND"}};
        }
        std::wstring_view SoundEffectHelp(const TextCatalog &) const override
        {
            return L"PROBE SOUND HELP";
        }
        std::map<std::string, std::filesystem::path> AudioFiles(
            const IEditorDocument &, const GameplayLaunchRequest &) const override
        {
            return audioFiles;
        }
        std::vector<AudioMarker> AudioMarkers(const IEditorDocument &) const override
        {
            ++markerCalls;
            return {{.5, "ProbeCue"}};
        }
    };

    void TestVorbisAudioView(ScreenVisual2DManager &visuals, const EngineServices &services,
                             TextCatalog &texts,
                             const std::function<const std::vector<DrawPacket> &()> &packets)
    {
        const auto fixture =
            std::filesystem::path(__FILE__).parent_path().parent_path() / "Rhythm/Fixtures/Audio/OppositePhase.ogg";
        auto mode = std::make_unique<ProbeMode>();
        mode->audioFiles = {{"Music", fixture}, {"ProbeCue", fixture}};
        EditorWorkspace state(std::move(mode), std::make_unique<ProbeDocument>());
        state.UpdateAnalysis();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (state.Analysis().Running() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            state.UpdateAnalysis();
        }
        Check(!state.Analysis().Running() && state.Analysis().Data().errors.empty() &&
                  !state.Analysis().Data().sounds.at("Music").frames.empty(),
              "The editor worker must publish real Ogg Vorbis analysis without codec errors.");
        state.audioWindow = 2;
        state.Seek(0);
        state.SelectTab(EditorTab::Audio);
        EditorView view(visuals, state, texts);
        view.Initialize(services);
        visuals.Render({});

        // Inspect the actual waveform and both spectrum tracks after rectangle
        // batches are expanded by the recording renderer, not synthetic FFT data.
        const auto visibleColumns = [&](float top, float bottom, bool waveform)
        {
            return std::ranges::count_if(
                packets(),
                [&](const auto &packet)
                {
                    const auto corner = TransformPoint(
                        {packet.bounds.x, packet.bounds.y + packet.bounds.height, 0}, packet.nodeTransform);
                    const float y = 360 - corner.y;
                    const bool colored = waveform ? packet.color.green > .5F && packet.color.red < .3F
                                                  : packet.color.red > .3F && packet.color.green < .95F;
                    const bool inRows = y >= top && y < bottom;
                    const bool inColumns = corner.x >= 24 - 640 && corner.x < 1256 - 640;
                    const bool inBand = inRows && inColumns;
                    return packet.type == DrawPacketType::Rectangle && inBand && colored;
                });
        };
        // Track rows of the 16:9 frame: waveform 1.2, music 1, expected hits .8 of the space below the tools.
        const float unit = (642.0F - 138.0F - 20.0F - 24.0F) / 3;
        const float musicTop = 138 + unit * 1.2F + 36, hitTop = musicTop + unit + 8;
        Check(visibleColumns(138, 138 + unit * 1.2F, true) > 10 && visibleColumns(musicTop, musicTop + unit, false) > 10 &&
                  visibleColumns(hitTop, hitTop + unit * .8F, false) > 10,
              "Ogg Vorbis must draw a waveform, music FFT and hit-sound FFT in the audio tab.");
        view.Shutdown();
        Check(visuals.CanvasCount() == 0, "The Vorbis view must release its canvases.");
    }

    void TestTaikoEditing()
    {
        for (const std::string_view id : {"", "unsupported"})
        {
            bool rejected = false;
            try
            {
                static_cast<void>(CreateEditorMode(id));
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected, "Empty or unknown modes must not silently use Taiko tools.");
        }
        for (const int id : {1, 2, 3, 4, 5, 11, 12, 13, 14, 15, 16, 17, 18})
        {
            EditorWorkspace state(CreateEditorMode("Taiko"),
                                  std::make_unique<ChartEditor>(PatternDocument{}));
            state.SelectTool(id);
            state.PlaceNote({0, {}});
            const bool isLong = id >= 11;
            Check(state.Mode().HasPendingPlacement() == isLong, "Taiko long-note placement changed.");
            if (isLong)
                state.PlaceNote({0, {1, 4}});
            const auto &notes = state.Document().Notes();
            Check(notes.size() == (isLong ? 2 : 1) && notes.front().note.keyType == (id == 18 ? 17 : id),
                  "Taiko tool persisted the wrong note type.");
            if (id == 18)
                Check(notes.front().note.extraData == std::vector<std::string>{"Action=Kat", "TickDivision=16"},
                      "Kat Buzz options changed.");
            const auto markers = state.Mode().AudioMarkers(state.Document());
            const bool tickRoll = id == 12 || id == 14;
            const bool tickBuzz = id == 17 || id == 18;
            const bool hasTicks = tickRoll || tickBuzz;
            Check(markers.size() == (hasTicks ? 5 : id == 5 ? 2 : 1), "Taiko expected-hit count changed.");
            Check(markers.front().sound == (id == 2 || id == 18 ? "Taiko.Kat.Hit"
                                            : id == 4           ? "Taiko.BigKat.FirstHit"
                                            : id == 3           ? "Taiko.BigDon.FirstHit"
                                                                : "Taiko.Don.Hit"),
                  "Taiko hit-sound mapping changed.");
        }
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(PatternDocument{}));
        state.SetRealtime(false);
        state.SelectTool(1);
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        state.Mode().EditScore(state, {ScoreLeft + MeasureWidth * .25F, ScoreY}, false);
        Check(state.Document().Notes().front().note.position.fraction == Rational{1, 4},
              "Score click no longer snaps using quarter-note division.");
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        state.Mode().EditScore(state, {ScoreLeft + MeasureWidth * .25F, ScoreY}, true);
        Check(state.Document().Notes().empty(), "Mode-owned note hit testing did not delete the note.");
        state.SetRealtime(true);
        state.SelectTool(1);
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        for (const Point outside : {Point{120, LaneY}, {1290, LaneY}, {JudgeX, 150}, {JudgeX, 320}})
            state.Mode().EditScore(state, outside, false);
        Check(state.Document().Notes().empty(), "Clicks outside the realtime lane must not place notes.");
        state.Mode().EditScore(state, {JudgeX, LaneY}, false);
        Check(state.Document().Notes().size() == 1, "A realtime lane click must still place a snapped note.");
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        state.Mode().EditScore(state, {120, LaneY}, true);
        Check(state.Document().Notes().size() == 1, "Outside-lane deletion must not affect nearby notes.");
        state.Mode().EditScore(state, {JudgeX, LaneY}, true);
        Check(state.Document().Notes().empty(), "In-lane realtime deletion stopped working.");
        state.Seek(123);
        state.SelectTool(11);
        state.PlaceNote({0, {1, 4}});
        Check(state.TimeMilliseconds() == 123 && state.Mode().CancelInteraction() &&
                  !state.Mode().HasPendingPlacement(),
              "Realtime long-note placement must preserve time and support cancellation.");
        state.Mode().DrawTools(canvas, state, TestCatalog(Language::English));
        canvas.actions.front()();
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::English));
        state.Mode().EditScore(state, {700, LaneY}, false);
        Check(state.Document().Notes().empty() && state.TimeMilliseconds() == 123,
              "Clicking empty realtime space with the selection tool must preserve the cursor and place no note.");
    }

    // A placement replaces every note whose span shares a position with it (Taiko has
    // one lane). The replacement is one Undo step; the same ordinary note is kept.
    void TestTaikoPlacementConflicts()
    {
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(PatternDocument{}));
        const auto keys = [&state]
        {
            std::vector<std::pair<MusicalPosition, int>> result;
            for (const auto &note : state.Document().Notes())
                result.emplace_back(note.note.position, note.note.keyType);
            return result;
        };
        using Notes = std::vector<std::pair<MusicalPosition, int>>;
        state.SelectTool(1);
        state.PlaceNote({0, {}});
        const auto revision = state.Document().Revision();
        state.PlaceNote({0, {}});
        Check(state.Document().Revision() == revision && keys() == Notes{{{0, {}}, 1}},
              "Placing the same note on itself must keep the existing note.");
        state.SelectTool(2);
        state.PlaceNote({0, {}});
        Check(keys() == Notes{{{0, {}}, 2}}, "A note placed on an existing note must replace it, not stack.");
        state.Undo();
        Check(keys() == Notes{{{0, {}}, 1}}, "One Undo must restore the replaced note.");
        state.Redo();

        // Ordinary notes on a roll's head, body or tail replace the whole roll.
        for (const MusicalPosition position : {MusicalPosition{1, {}}, MusicalPosition{1, {1, 2}},
                                               MusicalPosition{2, {}}})
        {
            state.SelectTool(11);
            state.PlaceNote({1, {}});
            state.PlaceNote({2, {}});
            state.SelectTool(1);
            state.PlaceNote(position);
            Check(keys() == Notes{{{0, {}}, 2}, {position, 1}},
                  "An ordinary note touching a roll must replace the entire roll.");
            state.Undo();
            Check(state.Document().Notes().size() == 3, "One Undo must restore both roll endpoints.");
            state.Undo();
        }

        // A roll replaces the notes it covers, including one on its endpoints.
        state.SelectTool(1);
        state.PlaceNote({3, {}});
        state.PlaceNote({3, {1, 2}});
        state.PlaceNote({4, {}});
        state.PlaceNote({5, {}});
        state.SelectTool(11);
        state.PlaceNote({4, {}});
        state.PlaceNote({3, {}});
        Check(keys() == Notes{{{0, {}}, 2}, {{3, {}}, 11}, {{4, {}}, 11}, {{5, {}}, 1}},
              "A new roll must replace every note between and on its endpoints.");
        state.Undo();
        Check(state.Document().Notes().size() == 5, "One Undo must restore every note the roll replaced.");
        state.Redo();
        state.SelectTool(12);
        state.PlaceNote({3, {1, 2}});
        state.PlaceNote({5, {}});
        Check(keys() == Notes{{{0, {}}, 2}, {{3, {1, 2}}, 12}, {{5, {}}, 12}},
              "A roll overlapping another roll must replace it.");

        // A zero-length second click is still rejected and keeps the first endpoint.
        const auto kept = state.Document().Revision();
        state.SelectTool(11);
        state.PlaceNote({6, {}});
        bool rejected = false;
        try
        {
            state.PlaceNote({6, {}});
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && state.Document().Revision() == kept && state.Mode().HasPendingPlacement(),
              "Failed second endpoint must preserve the document and remain available for correction.");
        state.Mode().CancelInteraction();
        state.SetFirstMeasure(MaximumMeasureCount - 1);
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        state.SetRealtime(true);
        state.Seek(state.Document().Timeline().Compile({MaximumMeasureCount - 1, {}}).count() / 1000.0);
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
    }

    void TestDelayViewsAndSnapping()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {3, 4}}, 1}, {{1, {1, 4}}, 2}};
        pattern.timing = {{{0, {3, 4}}, TimingDirectiveType::Bpm, 120},
                          {{1, {1, 4}}, TimingDirectiveType::DelayMilliseconds, -1500}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        state.SetRealtime(false);
        state.SelectTool(0);
        RecordingCanvas overview;
        state.Mode().DrawChart(overview, state, TestCatalog(Language::English));
        // Cut positions are washed red in the score and never snap.
        const auto red = std::ranges::count_if(overview.rectangleColors, [](const auto &color) {
            return color.red > .85F && color.green < .35F && std::abs(color.alpha - .4F) < .01F;
        });
        Check(red > 0 && std::ranges::find(overview.labels, L"1") != overview.labels.end() &&
              std::ranges::find(overview.labels, L"2") != overview.labels.end(),
              "The score must number its measures and mark the cut section without BPM captions.");
        Check(std::ranges::none_of(overview.labels, [](const auto &label) { return label.starts_with(L"BPM"); }),
              "The score must number its measures and mark the cut section without BPM captions.");
        // Beats keep uniform widths: the note at 1/4 of the second measure sits a quarter into it.
        state.Mode().EditScore(state, {ScoreLeft + MeasureWidth * 1.25F, ScoreY}, false);
        RecordingCanvas selected;
        state.Mode().DrawNotePanel(selected, state, TestCatalog(Language::English));
        Check(std::ranges::find(selected.labels, L"1/4") != selected.labels.end(),
              "A note at the shortened measure start must select its unchanged source fraction.");
        state.Mode().CancelInteraction();
        state.SelectNotes({});
        state.DeleteNote(state.Document().Pattern().notes.back().sourceOrder);
        state.Mode().DrawChart(selected, state, TestCatalog(Language::English));
        state.Mode().EditScore(state, {ScoreLeft + MeasureWidth, ScoreY}, false);
        Check(state.TimeMilliseconds() == 1000,
              "Overview snapping must use the visible delayed grid rather than the original uniform source spacing.");
        state.SetRealtime(true);
        EffectDocument effects;
        effects.commands.push_back(
            {.position = {0, {}}, .type = EffectCommandType::MeasureLineVisible, .endValue = 1});
        state.Replace(state.Document().Pattern(), effects);
        RecordingCanvas realtime;
        state.Mode().DrawChart(realtime, state, TestCatalog(Language::English));
        Check(std::ranges::any_of(realtime.rectangles, [](const auto &rect)
                                  { return std::abs(rect.x + rect.width * .5F - JudgeX) < .01F && rect.y == 160 &&
                                           rect.width == 5; }),
              "Realtime must draw the clipped measure start at the judgement line.");
        state.SelectTool(1);
        state.Mode().EditScore(state, {JudgeX, LaneY}, false);
        Check(std::ranges::any_of(state.Document().Pattern().notes, [](const auto &note) {
                  return note.position == MusicalPosition{1, {1, 4}};
              }) && state.TimeMilliseconds() == 1000,
              "Realtime and overview must select the same surviving beat at the clipped bar boundary.");
    }

    // Compose panels: timing kinds with Add/Apply, toggles with Exclude and Audio FX buses,
    // all as single history steps on the document.
    void TestComposePanelEdits()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.timing = {{{0, {}}, TimingDirectiveType::Bpm, 120}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        state.OpenPanel(EditorPanel::Timing);
        Check(state.Focus() == EditorFocus::Timing && state.Tab() == EditorTab::Compose,
              "The Timing panel must own the timing list.");
        state.timingForm.kind = EditorTimingKind::Delay;
        state.timingForm.measure = "3";
        state.timingForm.fraction = "0/1";
        state.timingForm.delay = "-250";
        state.AddTiming();
        const auto delayRow = [&state]() -> std::optional<std::size_t> {
            const auto &timing = state.Document().Timing();
            for (std::size_t i = 0; i < timing.size(); ++i)
                if (timing[i].type == TimingDirectiveType::DelayMilliseconds)
                    return i;
            return std::nullopt;
        };
        Check(delayRow() && state.Document().Timing()[*delayRow()].value == -250,
              "Add must write a delay at the form's position.");
        const auto revision = state.Document().Revision();
        state.SelectListRow(*delayRow(), false, false);
        Check(state.ListAnchor() == delayRow(), "The clicked row is the anchor the form edits.");
        state.timingForm.fraction = "1/4";
        state.timingForm.delay = "125";
        state.ApplyTimingRow();
        Check(state.Document().Timing().size() == 2 && state.Document().Timing()[*delayRow()].value == 125 &&
                  state.Document().Timing()[*delayRow()].position == MusicalPosition{2, {1, 4}},
              "Apply must move and change the selected row instead of adding another.");
        state.Undo();
        Check(state.Document().Revision() != revision && state.Document().Timing()[*delayRow()].value == -250,
              "Apply must undo in one step.");
        state.timingForm.kind = EditorTimingKind::Bpm;
        state.timingForm.measure = "2";
        state.timingForm.fraction = "0/1";
        state.timingForm.bpm = "180";
        state.AddTiming();
        Check(state.Document().Timing().size() == 3, "Add must keep the other timing kinds.");

        state.OpenPanel(EditorPanel::Zones);
        auto &form = state.effectForm;
        form.selection = EffectCommandType::Kiai;
        form.startMeasure = "2";
        form.startFraction = "0/1";
        form.beginValue = form.endValue = "1";
        state.ApplyEffect();
        form.selection = EffectCommandType::SyncopationZone;
        form.excluded = "1|2";
        state.ApplyEffect();
        const auto written = ChartEditor::WriteEffects(state.Document().Effects());
        Check(written.find("#Kiai ON") != std::string::npos &&
                  written.find("#Syncopation ON, Exclude=1|2") != std::string::npos,
              "Zone toggles must write ON states and the Exclude divisors.");
        form.excluded = "0";
        bool rejected = false;
        try
        {
            state.ApplyEffect();
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected, "Exclude divisors outside 1..1024 must be rejected.");

        state.OpenPanel(EditorPanel::AudioEffects);
        form.selection = EffectCommandType::LowPassCutoff;
        form.audioBus = "Music";
        form.startMeasure = "2";
        form.startFraction = "0/1";
        form.endMeasure = "3";
        form.endFraction = "0/1";
        form.beginValue = "800";
        form.endValue = "20000";
        form.curve = AutomationCurve::Exponential;
        form.curveName.clear();
        form.excluded.clear();
        state.ApplyEffect();
        const auto &commands = state.Document().Effects().commands;
        const auto lowPass = std::ranges::find(commands, EffectCommandType::LowPassCutoff, &EffectCommand::type);
        Check(lowPass != commands.end() && lowPass->target == "Music" && lowPass->endPosition.has_value(),
              "Audio FX must keep its bus and Area.");
        const auto row = static_cast<std::size_t>(lowPass - commands.begin());
        state.SelectListRow(row, false, false);
        form.Load(state.Document().Effects(), row);
        Check(form.audioBus == "Music" && form.endMeasure == "3", "Loading an Audio FX row must restore bus and Area.");
        form.beginValue = "1200";
        state.ApplyEffectRow();
        Check(std::ranges::count(state.Document().Effects().commands, EffectCommandType::LowPassCutoff,
                                 &EffectCommand::type) == 1,
              "Apply must replace the selected Audio FX row.");
    }

    void TestInitialSelectionAndRealtimeGrid()
    {
        PatternDocument pattern;
        pattern.baseBpm = 120;
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        Check(state.Score().realtime, "Compose must open on the Realtime view.");
        // The score view seeks to a clicked beat; the realtime lane keeps the cursor.
        state.SetRealtime(false);
        state.PlaceNote({0, {1, 4}});
        Check(state.Document().Notes().empty() && !state.Mode().HasPendingPlacement() &&
                  state.TimeMilliseconds() == 500,
              "A new editor must select/seek without placing a note.");
        RecordingCanvas tools;
        state.Mode().DrawTools(tools, state, TestCatalog(Language::English));
        const bool selectLit = std::ranges::any_of(std::views::iota(std::size_t{}, tools.rectangles.size()), [&](auto i) {
            const auto &rect = tools.rectangles[i];
            const auto &color = tools.rectangleColors[i];
            const bool inToolColumn = rect.x >= 238 && rect.x < 286;
            const bool litColor = color.red > .9F && color.blue < .3F;
            return inToolColumn && rect.height == 3 && litColor;
        });
        Check(selectLit && !tools.actions.empty(), "Selection must start active and display the cursor/eraser icon.");
        Check(std::ranges::any_of(tools.images, [](const auto &path) { return path.filename() == L"SelectionTool.png" && std::filesystem::is_regular_file(path); }),
              "Selection must start active and display the cursor/eraser icon.");
        const std::array cases{
            std::pair{1, Color{1, 1, 1, 1}}, std::pair{2, Color{1, .2F, .2F, 1}},
            std::pair{4, Color{.2F, .5F, 1, 1}}, std::pair{8, Color{1, .9F, .1F, 1}},
            std::pair{16, Color{.65F, .65F, .65F, 1}}, std::pair{3, Color{.15F, 1, .3F, 1}},
            std::pair{6, Color{.8F, .35F, 1, 1}}, std::pair{5, Color{1, .55F, .1F, 1}}};
        state.SetRealtime(true);
        state.Seek(0);
        for (const auto &[division, expected] : cases)
        {
            state.SetDivision(division);
            RecordingCanvas grid;
            grid.deviceScale = .5F;
            state.Mode().DrawChart(grid, state, TestCatalog(Language::English));
            // Default spacing is 50%.
            const float tickX = JudgeX + WholeNotePixels * .5F / static_cast<float>(division * 4);
            bool foundTick = false, foundBar = false;
            for (std::size_t i = 0; i < grid.rectangles.size(); ++i)
            {
                const auto &rect = grid.rectangles[i];
                const float center = rect.x + rect.width * .5F;
                if (rect.y == 160 && rect.height == 152)
                    foundBar |= std::abs(center - JudgeX) < .01F && rect.width * grid.deviceScale >= 5;
                if (std::abs(rect.y - (160 + 152 * .18F)) >= .01F || std::abs(center - tickX) >= .02F) continue;
                if (std::abs(rect.width * grid.deviceScale - 2) >= .01F) continue;
                const auto &color = grid.rectangleColors[i];
                const bool rgbMatches = std::abs(color.red - expected.red) < .001F &&
                                        std::abs(color.green - expected.green) < .001F &&
                                        std::abs(color.blue - expected.blue) < .001F;
                foundTick = rgbMatches && std::abs(color.alpha - .55F) < .001F;
            }
            Check(foundTick && foundBar, "Realtime grid must use the requested colors and device-pixel widths.");
        }
    }

    void TestRealtimeZoomAndTransport()
    {
        for (const double zoom : {.25, .5, 1., 2., 4.})
        {
            PatternDocument pattern;
            pattern.baseBpm = 120;
            EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
            state.SelectTool(1);
            state.SetRealtime(true);
            state.SetRealtimeZoom(zoom);
            RecordingCanvas empty;
            state.Mode().DrawChart(empty, state, TestCatalog(Language::English));
            Check(std::ranges::any_of(
                      empty.rectangles,
                      [](const auto &rect)
                      {
                          const bool origin = rect.x == 184 && rect.y == 160;
                          return origin && rect.width == 1096 && rect.height == 152;
                      }),
                  "Spacing must not resize the realtime lane.");
            // An eighth note stays inside the lane even at 400%.
            const float x = JudgeX + WholeNotePixels / 8 * static_cast<float>(zoom);
            state.Mode().EditScore(state, {x, LaneY}, false);
            Check(state.Document().Notes().size() == 1 &&
                      state.Document().Notes().front().note.position.fraction == Rational{1, 8},
                  "Realtime snapping must use the same zoomed time coordinates as drawing.");
            RecordingCanvas notes;
            state.Mode().DrawChart(notes, state, TestCatalog(Language::English));
            const float head = 128 * (152 / 228.0F);
            Check(std::ranges::any_of(notes.imageRectangles, [&](const auto &rect) {
                      return std::abs(rect.x + head * .5F - x) < .1F && std::abs(rect.width - head) < .01F;
                  }),
                  "Spacing must scale note distance without scaling the game-sized skin head.");
            const auto revision = state.Document().Revision();
            state.SetRealtimeZoom(1);
            Check(state.Document().Revision() == revision && state.Document().Notes().front().timing.count() == 250'000,
                  "Spacing must not edit source beats or compiled note times.");
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 250, "First Z must seek to the first note.");
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 0, "Second Z must seek to zero.");
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 250, "Subsequent Z must return to the first note.");
            state.Seek(100);
            state.SeekFirstNote();
            Check(state.TimeMilliseconds() == 250, "A manual seek must reset the consecutive-Z toggle.");
            state.Seek(0);
            state.Mode().ScrollScore(state, 1);
            Check(state.TimeMilliseconds() == 125,
                  "Realtime wheel must advance one selected sixteenth-note grid step.");
            state.Mode().ScrollScore(state, -1);
            state.Mode().ScrollScore(state, -1);
            Check(state.TimeMilliseconds() == 0 && state.Document().Revision() == revision,
                  "Realtime wheel must clamp at the chart start and preserve the score.");
            bool rejected = false;
            try
            {
                state.SetRealtimeZoom(std::numeric_limits<double>::infinity());
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected && state.Score().realtimeZoom == 1, "Invalid zoom must preserve the current scale.");
        }
    }

    void TestSelectionClipboardAndHistory()
    {
        PatternDocument source;
        source.baseBpm = 120;
        source.notes = {{{0, {}}, 1}, {{0, {1, 4}}, 2}, {{1, {}}, 2}, {{1, {1, 8}}, 1}, {{1, {1, 4}}, 1}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(source));
        state.SetRealtime(false);
        state.SelectTool(0);
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::English));
        Check(state.Mode().BeginScoreSelection(state, {ScoreLeft, ScoreY}), "Q must enable beat-range dragging.");
        Check(state.Mode().UpdateScoreSelection(state, {ScoreLeft + MeasureWidth * .25F, ScoreY}, true),
              "Dragging must complete on release.");
        Check(state.SelectedNoteIds().size() == 2 &&
                  state.NoteSelectionRange()->second.fraction == Rational{1, 4},
              "Drag selection must preserve its exact beat interval.");
        state.CopySelection();
        state.Seek(2000);
        const auto before = ChartEditor::WritePattern(state.Document().Pattern());
        state.PasteSelection(false);
        const auto pasted = ChartEditor::WritePattern(state.Document().Pattern());
        const auto &notes = state.Document().Notes();
        Check(notes.size() == 5 &&
                  std::ranges::any_of(
                      notes, [](const auto &n)
                      { return n.note.position == MusicalPosition{1, {1, 8}} && n.note.keyType == 1; }),
              "Paste without blanks must preserve an unrelated note in the copied gap.");
        Check(std::ranges::any_of(notes, [](const auto &n)
                                  { return n.note.position == MusicalPosition{1, {}} && n.note.keyType == 1; }),
              "Paste must overwrite a destination Kat with the copied Don.");
        state.Undo();
        Check(ChartEditor::WritePattern(state.Document().Pattern()) == before,
              "One Undo must restore every overwritten note, including the original Kat.");
        state.Redo();
        Check(ChartEditor::WritePattern(state.Document().Pattern()) == pasted,
              "One Redo restores the whole paste.");
        state.Undo();
        state.PasteSelection(true);
        Check(state.Document().Notes().size() == 4 &&
                  std::ranges::none_of(state.Document().Notes(), [](const auto &n)
                                       { return n.note.position == MusicalPosition{1, {1, 8}}; }),
              "Paste with blanks must remove the destination gap's old note.");
        state.Undo();
        Check(ChartEditor::WritePattern(state.Document().Pattern()) == before,
              "Undo of paste-with-blanks must restore gap contents and replaced notes together.");

        // A copied selection retains blank margins even when it contains no note.
        state.SelectNoteRange({2, {}}, {2, {1, 4}});
        state.CopySelection();
        state.Seek(2000);
        state.PasteSelection(true);
        Check(state.Document().Notes().size() == 2,
              "An entirely blank copied interval must erase the destination interval.");
        state.Undo();
        Check(ChartEditor::WritePattern(state.Document().Pattern()) == before,
              "Blank paste must be fully reversible.");

        state.OpenPanel(EditorPanel::Timing);
        state.timingForm.measure = "1";
        state.timingForm.fraction = "0/1";
        state.timingForm.bpm = "180";
        state.ApplyBpm();
        state.timingForm.fraction = "1/4";
        state.timingForm.bpm = "240";
        state.ApplyBpm();
        state.SelectListRow(0, false, false);
        state.SelectListRow(1, false, true);
        Check(state.IsListRowSelected(0) && state.IsListRowSelected(1), "Shift-click selects a timing range.");
        state.CopySelection();
        state.Seek(state.Document().Timeline().Compile({1, {}}).count() / 1000.0);
        state.PasteSelection(false);
        Check(state.Document().Timing().size() == 4, "Timing multi-paste must keep both copied BPM changes.");
        state.Undo();
        Check(state.Document().Timing().size() == 2, "Timing multi-paste is one undo action.");

        EffectDocument effects;
        EffectCommand point;
        point.type = EffectCommandType::NoteSpeed;
        point.beginValue = point.endValue = 2;
        effects.commands.push_back(point);
        point.type = EffectCommandType::BusVolume;
        point.target = "HitSound";
        point.beginValue = point.endValue = .6;
        point.position = {0, {1, 4}};
        effects.commands.push_back(point);
        state.Replace(state.Document().Pattern(), effects);
        state.OpenPanel(EditorPanel::Volume);
        state.SelectListRow(0, false, false);
        state.SelectListRow(1, true, false);
        state.CopySelection();
        state.PasteSelection(false);
        Check(state.Document().Effects().commands.size() == 4, "Ctrl-clicked effect rows must paste as a group.");
        state.Undo();
        Check(state.Document().Effects().commands.size() == 2, "Effect multi-paste must undo as a group.");
        auto metadata = state.Document().Pattern();
        metadata.name = "Changed";
        metadata.romanizedName = "Changed (romanized)";
        metadata.romanizedMaker = "Maker (romanized)";
        state.Replace(metadata, state.Document().Effects());
        state.Undo();
        Check(state.Document().Pattern().name.empty() && state.Document().Pattern().romanizedName.empty() &&
                  state.Document().Pattern().romanizedMaker.empty(),
              "Metadata edits must participate in the same history.");
        state.Redo();
        Check(state.Document().Pattern().name == "Changed", "Metadata redo must restore its value.");
        Check(state.Document().Pattern().romanizedName == "Changed (romanized)" &&
                  state.Document().Pattern().romanizedMaker == "Maker (romanized)",
              "Metadata redo must restore the romanized lines too.");
    }

    void TestLastNoteSeeking()
    {
        PatternDocument pattern;
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {1, 8}}, 1, 0}, {{0, {1, 4}}, 11, 1}, {{1, {1, 2}}, 11, 2}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        const auto revision = state.Document().Revision();
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == 3000, "First X must include the last long-note tail.");
        bool unavailable = false;
        try
        {
            state.SeekLastNote();
        }
        catch (const std::runtime_error &)
        {
            unavailable = true;
        }
        Check(unavailable && state.TimeMilliseconds() == 3000,
              "Unavailable music duration must preserve the cursor and allow retrying the second X.");
        state.SeekFirstNote();
        Check(state.TimeMilliseconds() == 250, "Z must reset X and seek the first note.");
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == 3000, "Changing from Z to X must restart at the last note.");
        state.SeekFirstNote();
        Check(state.TimeMilliseconds() == 250, "Changing from X to Z must restart at the first note.");
        state.Seek(100);
        state.SeekLastNote();
        Check(state.TimeMilliseconds() == 3000 && state.Document().Revision() == revision,
              "Manual seeking must reset X without editing the score.");
        EditorWorkspace empty(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(PatternDocument{}));
        empty.SeekLastNote();
        Check(empty.TimeMilliseconds() == 0, "An empty chart must have a zero last-note destination.");
    }

    void TestPreviewMatchesPlay()
    {

        const auto parsed = ChartParser{}.ParsePattern("Version: 1\nBase BPM: 180\nMode: Taiko\n[Pattern]\n"
                                                              "0/1,17,1,,Action = Kat,TickDivision = 64\n1/2,17,2\n");
        Check(parsed.Succeeded(), "Spaced Buzz regression chart failed to parse.");
        ChartEditor document(parsed.document);
        auto editorMode = CreateEditorMode("Taiko");
        const auto markers = editorMode->AudioMarkers(document);
        auto play = TaikoMode{}.CreateSession(document.Pattern());
        Check(play.Succeeded(), "Spaced options failed to load in play.");
        const auto hit = play.session->ProcessInput('D', InputEdge::Pressed, {});
        const auto *presentation = play.session->FindNotePresentation(1);
        Check(markers.size() == 33 && markers.size() == presentation->tickTimes.size() + 1 &&
                  markers.front().sound == hit.audioCues.front().sound,
              "Editor/play must share spaced/case-insensitive Action and TickDivision interpretation.");
        for (std::size_t i = 1; i < markers.size(); ++i)
            Check(markers[i].seconds == presentation->tickTimes[i - 1].count() / 1e6 &&
                      markers[i].sound == KatHit,
                  "Preview Buzz ticks diverged from play times/action.");
        auto purple = PatternDocument{};
        purple.mode = "Taiko";
        purple.notes.push_back({{0, {}}, 5, 0});
        ChartEditor purpleDocument(purple);
        const auto purpleMarkers = editorMode->AudioMarkers(purpleDocument);
        auto purplePlay = TaikoMode{}.CreateSession(purple);
        Check(purplePlay.Succeeded(), "Explicit Taiko Purple fixture failed to load.");
        const auto don = purplePlay.session->ProcessInput('C', InputEdge::Pressed, {});
        const auto kat = purplePlay.session->ProcessInput('D', InputEdge::Pressed, {});
        Check(purpleMarkers.size() == 2 && purpleMarkers[0].sound == don.audioCues.front().sound &&
                  purpleMarkers[1].sound == kat.audioCues.front().sound,
              "Purple preview must include both play sounds.");

        // Explicit table assignments beat timed defaults; timed overrides are
        // resolved independently for the two Purple input sounds.
        EffectDocument effects;
        effects.hitSounds = {{"own", "own.wav"}, {"don", "don.wav"}, {"kat", "kat.wav"}};
        effects.hitSoundChanges = {{{0, {}}, "don", 1}, {{0, {}}, "kat", 2}};
        purpleDocument.Replace(purple, effects);
        const auto changed = editorMode->AudioMarkers(purpleDocument);
        auto changedPlay = TaikoMode{}.CreateSession(purple, effects);
        Check(changedPlay.Succeeded(), "Explicit Taiko sound override fixture failed to load.");
        const auto changedDon = changedPlay.session->ProcessInput('C', InputEdge::Pressed, {});
        const auto changedKat = changedPlay.session->ProcessInput('D', InputEdge::Pressed, {});
        Check(changed[0].sound == changedDon.audioCues.front().sound &&
                  changed[1].sound == changedKat.audioCues.front().sound,
              "Timed sound overrides diverged between preview/play.");
        purple.notes.front().hitSound = "own";
        purpleDocument.Replace(purple, effects);
        const auto assigned = editorMode->AudioMarkers(purpleDocument);
        Check(assigned[0].sound == "Chart.HitSound.own" && assigned[1].sound == assigned[0].sound,
              "Explicit hit sound must override both default Purple sounds.");

        auto invalid = parsed.document;
        invalid.notes.front().extraData = {"Action = Kat", "TickDivision = 1025"};
        document.Replace(invalid, {});
        bool rejected = false;
        try
        {
            static_cast<void>(editorMode->AudioMarkers(document));
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && !TaikoMode{}.CreateSession(invalid).Succeeded(),
              "Editor/play must reject the same out-of-range TickDivision.");
    }

    void TestSeekAndEffectEdits()
    {
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(PatternDocument{}));
        state.FinishBuild();
        state.SetDivision(8);
        Check(state.Score().division == 8 && state.NeedsRebuild(), "Grid setting must request its own rebuild.");
        state.FinishBuild();
        bool badDivision = false;
        try
        {
            state.SetDivision(1025);
        }
        catch (const std::invalid_argument &)
        {
            badDivision = true;
        }
        Check(badDivision && state.Score().division == 8 && !state.NeedsRebuild(),
              "Rejected grid settings must not change state or request a rebuild.");
        state.SetRealtime(true);
        Check(state.Score().realtime && state.NeedsRebuild(), "View mode changes must request a rebuild.");
        state.Seek(-1000);
        for (const double invalid :
             {1e12, 1e100, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            bool rejected = false;
            try
            {
                state.Seek(invalid);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected && state.TimeMilliseconds() == -1000, "Invalid seek must preserve the previous time.");
        }
        state.effectForm.selection = EffectCommandType::ScrollSpeed;
        state.effectForm.beginValue = "2";
        state.effectForm.endMeasure = "2";
        state.effectForm.endFraction = "0/4";
        state.effectForm.endValue = "3";
        state.effectForm.curve = AutomationCurve::Linear;
        state.ApplyEffect();
        state.effectForm.Load(state.Document().Effects(), 0);
        Check(state.effectForm.endMeasure == "2" && state.effectForm.endFraction == "0/1" &&
                  state.effectForm.curve == AutomationCurve::Linear,
              "Named effect fields lost the region/curve during row selection.");
        const auto revision = state.Document().Revision();
        state.effectForm.beginValue = "0";
        bool rejected = false;
        try
        {
            state.ApplyEffect();
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && state.Document().Revision() == revision &&
                  state.Document().Effects().commands.front().beginValue == 2,
              "Invalid effect draft must leave the document unchanged.");
        state.RemoveEffect(0);
        Check(state.Document().Effects().commands.empty(), "Effect row deletion failed.");
        state.timingForm.bpm = "180";
        state.ApplyBpm();
        Check(state.Document().Pattern().timing.front().value == 180, "Timing command failed.");
        state.RemoveTiming(0);
        Check(state.Document().Pattern().timing.empty(), "Timing deletion failed.");
    }

    void TestInspectorMusicalTiming()
    {
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(PatternDocument{}));
        state.AddNote({0, {}}, 12, MusicalPosition{0, {1, 4}});
        const auto id = state.Document().Notes().front().note.sourceOrder;
        TaikoEditorNoteInspector inspector;
        inspector.Select(state.Document(), id);
        inspector.Form().tool = 1;
        const auto initialRevision = state.Document().Revision();
        bool groupRejected = false;
        try
        {
            inspector.Apply(state);
        }
        catch (const std::exception &)
        {
            groupRejected = true;
        }
        Check(groupRejected && state.Document().Revision() == initialRevision && state.Document().Notes().size() == 2,
              "Changing to another tool group must preserve the source note and its paired end.");
        inspector.Form().tool = 14;
        inspector.Form().startMeasure = "2";
        inspector.Form().startBeat = "1/8";
        inspector.Form().endMeasure = "3";
        inspector.Form().endBeat = "1/4";
        inspector.Apply(state);
        const auto selected = *state.Document().FindNote(id);
        Check(selected.head.note.position == MusicalPosition{1, {1, 8}} && selected.tail &&
              selected.tail->note.position == MusicalPosition{2, {1, 4}},
              "Musical input must use one-based measures, exact whole-note fractions and preserve the paired end.");
        Check(selected.head.note.keyType == 14 && selected.tail->note.keyType == 14 &&
              selected.head.timing == RhythmTime{2'250'000},
              "Musical input must use one-based measures, exact whole-note fractions and preserve the paired end.");
        Check(selected.tail->timing == RhythmTime{4'500'000},
              "Musical input must use one-based measures, exact whole-note fractions and preserve the paired end.");
        for (const auto language : {Language::Korean, Language::English})
        {
            RecordingCanvas panel;
            inspector.Draw(panel, state, TestCatalog(language));
            const auto &text = TaikoEditor(TestCatalog(language));
            Check(std::ranges::find(panel.labels, std::wstring(text.beatPosition)) != panel.labels.end() &&
                      std::ranges::find(panel.labels, L"2250.000 ms") != panel.labels.end() &&
                      std::ranges::none_of(panel.buttonLabels,
                                           [](const auto &label) { return label.find(L"ms") != std::wstring::npos; }),
                  "The inspector must localize musical fields and leave derived milliseconds read-only.");
        }
        // BPM edits recompile time without changing the selected musical source or its IDs.
        state.timingForm.measure = "2";
        state.timingForm.fraction = "0/1";
        state.timingForm.bpm = "180";
        state.ApplyBpm();
        RecordingCanvas refreshed;
        inspector.Draw(refreshed, state, TestCatalog(Language::English));
        Check(inspector.Form().startMeasure == "2" && inspector.Form().startBeat == "1/8" &&
              state.Document().FindNote(id)->tail->note.sourceOrder == selected.tail->note.sourceOrder,
              "Timeline changes must refresh derived times while retaining source coordinates and paired IDs.");
        Check(state.Document().FindNote(id)->head.timing == RhythmTime{2'166'667},
              "Timeline changes must refresh derived times while retaining source coordinates and paired IDs.");
        const auto revision = state.Document().Revision();
        for (int invalid = 0; invalid < 5; ++invalid)
        {
            inspector.Select(state.Document(), id);
            auto &draft = inspector.Form();
            if (invalid == 0)
                draft.startMeasure = "0";
            if (invalid == 1)
                draft.startMeasure = "100001";
            if (invalid == 2)
                draft.startBeat = "1/1";
            if (invalid == 3)
                draft.startBeat = "250ms";
            if (invalid == 4)
            {
                draft.endMeasure = draft.startMeasure;
                draft.endBeat = draft.startBeat;
            }
            bool rejected = false;
            try
            {
                inspector.Apply(state);
            }
            catch (const std::exception &)
            {
                rejected = true;
            }
            Check(rejected && state.Document().Revision() == revision &&
                  state.Document().FindNote(id)->head.note.position == selected.head.note.position,
                  "Invalid musical fields must preserve both endpoints and all document caches.");
            Check(state.Document().FindNote(id)->tail->note.position == selected.tail->note.position,
                  "Invalid musical fields must preserve both endpoints and all document caches.");
        }
        inspector.Select(state.Document(), id);
        inspector.Form().tool = 12;
        inspector.Form().startMeasure = "4";
        inspector.Form().startBeat = "3/8";
        inspector.Form().endMeasure = "5";
        inspector.Apply(state);
        Check(state.Document().Notes().size() == 2 && state.Document().FindNote(id)->tail &&
              state.Document().FindNote(id)->head.note.position == MusicalPosition{3, {3, 8}},
              "Changing variants must retain the paired end IDs and update both musical positions in the index.");
        Check(state.Document().FindNote(id)->tail->note.sourceOrder == selected.tail->note.sourceOrder &&
              state.Document().NotesInMeasures(1, 3).empty() && state.Document().NotesInMeasures(3, 5).size() == 2,
              "Changing variants must retain the paired end IDs and update both musical positions in the index.");
    }

    void TestRealtimeIgnoresScrollEffects()
    {
        auto pattern = PatternDocument{};
        pattern.baseBpm = 120;
        pattern.timing = {{{2, {}}, TimingDirectiveType::Bpm, 240}};
        pattern.notes = {{{0, {1, 4}}, 1}, {{1, {}}, 11, 1}, {{3, {}}, 11, 2}, {{4, {}}, 1}};
        const auto parsed = ChartParser{}.ParseEffect(R"([Interpolation]
Dip: 1-3*u*(1-u)
[Speed]
1,0/1,Area,9,0/1,#ScrollSpeed Whole,From=1,To=2,Curve=Linear
1,0/1,Area,9,0/1,#ScrollSpeed Separate,From=1,To=1,Curve=Dip
)");
        Check(parsed.Succeeded(), "Realtime scroll-effect regression fixture must parse.");
        EditorWorkspace baseline(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(pattern, parsed.document));
        Check(state.Document().Notes().back().scrollMultiplier < 1 &&
                  state.Document().ScrollDistance(RhythmTime{1'000'000}, {}) > 1'000'000,
              "The fixture must retain active Whole and custom Separate effects in the chart.");
        baseline.SetRealtime(true);
        state.SetRealtime(true);
        for (const double zoom : {.25, .5, 1., 2., 4.})
            for (const double time : {0., 1000., 2500., 4500., 5500.})
            {
                baseline.SetRealtimeZoom(zoom);
                state.SetRealtimeZoom(zoom);
                baseline.Seek(time);
                state.Seek(time);
                RecordingCanvas expected, actual;
                baseline.Mode().DrawChart(expected, baseline, TestCatalog(Language::Korean));
                state.Mode().DrawChart(actual, state, TestCatalog(Language::Korean));
                Check(actual.rectangles.size() == expected.rectangles.size(),
                      "Scroll effects must not change realtime note, long-body or grid visibility.");
                for (std::size_t i = 0; i < expected.rectangles.size(); ++i)
                {
                    const auto &a = actual.rectangles[i], &b = expected.rectangles[i];
                    Check(std::abs(a.x - b.x) < .01F && std::abs(a.y - b.y) < .01F &&
                          std::abs(a.width - b.width) < .01F,
                          "Realtime notes, long bodies and grids must use the same time axis with or without effects.");
                    Check(std::abs(a.height - b.height) < .01F,
                          "Realtime notes, long bodies and grids must use the same time axis with or without effects.");
                }
            }
        state.SetRealtimeZoom(1);
        state.Seek(5000);
        state.SelectTool(1);
        RecordingCanvas canvas;
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        state.Mode().EditScore(state, {JudgeX + WholeNotePixels / 2, LaneY}, true);
        Check(state.Document().Notes().size() == 3,
              "Deleting at the uniform time coordinate must find the note despite its speed effects.");
        canvas.rectangles.clear();
        state.Mode().DrawChart(canvas, state, TestCatalog(Language::Korean));
        state.Mode().EditScore(state, {JudgeX + WholeNotePixels / 2, LaneY}, false);
        Check(state.Document().Notes().size() == 4 &&
                  state.Document().Notes().back().note.position == MusicalPosition{4, {}},
              "Placement must snap to the uniform time grid under Whole and custom Separate effects.");
    }

    // New difficulty: Metadata only until Create, which writes the pair and continues on it.
    void TestNewDifficultyDraft()
    {
        namespace fs = std::filesystem;
        const auto root = fs::temp_directory_path() / L"youmu-editor-new" / std::to_wstring(GetCurrentProcessId());
        fs::remove_all(root);
        const auto songFolder = root / L"가수 - 음악";
        const auto music = songFolder / L"Music/음악.ymm";
        fs::create_directories(music.parent_path());
        std::ofstream(music, std::ios::binary) << "Version: 1\nFile: sample.wav\nMusic Name 1: Test\nArtist 1: Test\n";
        std::ofstream(songFolder / L"Music/sample.wav", std::ios::binary) << "fixture";
        const auto parsedMusic = ChartParser{}.ParseMusicFile(music);
        Check(parsedMusic.Succeeded(), "New difficulty fixture YMM must parse.");
        GameplayLaunchRequest request;
        request.mode = "Taiko";
        request.songsRoot = root;
        request.musicPath = songFolder / L"Music/sample.wav";
        request.editorNewDifficulty =
            std::make_shared<const SongCatalogEntry>(SongCatalogEntry{music, request.musicPath, parsedMusic.document, {}});
        const auto draftOf = [&](const char *name) {
            auto state = std::make_unique<EditorWorkspace>(request);
            state->Initialize();
            auto pattern = state->Document().Pattern();
            pattern.name = name;
            pattern.makers = {"Tester"};
            auto effects = state->Document().Effects();
            effects.hitSounds["1"] = "custom.wav";
            state->Replace(std::move(pattern), std::move(effects));
            return state;
        };

        auto state = draftOf("Fresh");
        Check(state->Creating() && state->Document().Dirty() && state->Tab() == EditorTab::Metadata,
              "A new difficulty must open on Metadata as an uncreated draft.");
        Check(state->MusicMetadataPath() == music, "The draft must use the YMM of the song it will be created in.");
        state->SelectTab(EditorTab::Compose);
        Check(state->Tab() == EditorTab::Metadata, "Compose and Audio must wait for Create.");
        state->Save();
        const auto folder = songFolder / L"Chart/Tester - Fresh";
        Check(!state->Creating() && !state->Document().Dirty() && state->Document().SourcePath().parent_path() == folder,
              "Create must write the pair and continue editing the created file.");
        Check(state->MusicMetadataPath() == music, "The created difficulty must find its YMM from its own folder.");
        const auto effects = ChartParser{}.ParseEffectFile(*state->Request().effectPath);
        Check(effects.Succeeded() && effects.document.hitSounds.contains("1"),
              "A hit sound table typed before Create must be saved into the new YME.");
        state->SelectTab(EditorTab::Compose);
        Check(state->Tab() == EditorTab::Compose, "Created difficulties must unlock every tab.");

        // A refused Create leaves no files behind and keeps every typed value.
        auto duplicate = draftOf("Fresh");
        bool refused{};
        try
        {
            duplicate->Save();
        }
        catch (const Error &error)
        {
            refused = error.reason == Failure::ExistingFolder;
        }
        Check(refused && duplicate->Creating() && duplicate->Document().Pattern().name == "Fresh",
              "A refused Create must keep the draft unchanged.");
        Check(duplicate->Document().Effects().hitSounds.contains("1"),
              "A refused Create must keep the draft unchanged.");
        fs::remove_all(root);
    }
    void TestTaikoToolGroups(TextCatalog &texts)
    {
        constexpr std::array keys{'W', 'E', 'R', 'T', 'Y', 'U'};
        const std::array<std::vector<int>, 6> groups{{{1, 2}, {3, 4, 5}, {11, 13}, {12, 14}, {15, 16}, {17, 18}}};
        for (std::size_t group = 0; group < groups.size(); ++group)
        {
            EditorWorkspace state(CreateEditorMode("Taiko"),
                                  std::make_unique<ChartEditor>(PatternDocument{}));
            state.SelectTool(0);
            for (std::size_t variant = 0; variant <= groups[group].size(); ++variant)
            {
                Check(static_cast<TaikoEditorMode &>(state.Mode())
                          .HandleToolKey(state, static_cast<std::uint16_t>(keys[group])),
                      "A grouped hotkey must be handled by the mode.");
                state.PlaceNote({static_cast<std::int64_t>(variant), {}});
                const int expected = groups[group][variant % groups[group].size()];
                if (expected >= 11)
                    state.PlaceNote({static_cast<std::int64_t>(variant), {1, 4}});
                const auto notes = state.Document().NotesInMeasures(static_cast<std::int64_t>(variant),
                                                                    static_cast<std::int64_t>(variant + 1));
                Check(notes.front().note.keyType == (expected == 18 ? 17 : expected),
                      "Group cycling selected the wrong persisted note type.");
                if (expected == 18)
                    Check(notes.front().note.extraData[0] == "Action=Kat", "Kat Buzz cycle lost its action.");
            }
            state.SelectTool(groups[group].front());
            RecordingCanvas palette;
            state.Mode().DrawTools(palette, state, texts);
            palette.actions[1 + group]();
            const auto measure = static_cast<std::int64_t>(groups[group].size() + 1);
            state.PlaceNote({measure, {}});
            const auto expected = groups[group][1];
            if (expected >= 11)
                state.PlaceNote({measure, {1, 4}});
            const auto head = state.Document().NotesInMeasures(measure, measure + 1).front().note;
            Check(head.keyType == (expected == 18 ? 17 : expected),
                  "Pressing an active tool button must cycle its group.");

            // The inspector must share precisely the same six categories as the palette.
            TaikoEditorNoteInspector inspector;
            inspector.Select(state.Document(), head.sourceOrder);
            for (std::size_t variant = 0; variant < groups[group].size(); ++variant)
            {
                RecordingCanvas panel;
                inspector.Draw(panel, state, texts);
                panel.actions.front()();
                const int next = groups[group][(variant + 2) % groups[group].size()];
                Check(inspector.Form().tool == next, "The inspector cycled outside its note category.");
                inspector.Apply(state);
                Check(state.Document().FindNote(head.sourceOrder)->head.note.keyType == (next == 18 ? 17 : next),
                      "The inspector must apply each variant within its category.");
            }
            const auto revision = state.Document().Revision();
            inspector.Form().tool = groups[(group + 1) % groups.size()].front();
            bool rejected = false;
            try
            {
                inspector.Apply(state);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Check(rejected && state.Document().Revision() == revision,
                  "Cross-category inspector drafts must not modify the chart.");

            state.SelectTool(11);
            state.PlaceNote({100, {}});
            Check(state.Mode().HasPendingPlacement(), "Q test requires an unfinished long note.");
            Check(static_cast<TaikoEditorMode &>(state.Mode()).HandleToolKey(state, 'Q') &&
                      !state.Mode().HasPendingPlacement(),
                  "Q must activate selection and cancel an unfinished placement.");
            const auto notesBefore = state.Document().Notes().size();
            state.PlaceNote({100, {}});
            Check(state.Document().Notes().size() == notesBefore && !state.Mode().HasPendingPlacement(),
                  "Q selection must not place another note or start another long note.");
        }
    }
} // namespace

void TestEditorModes(ScreenVisual2DManager &visuals, const EngineServices &services,
                     TextCatalog &texts,
                     const std::function<const std::vector<DrawPacket> &()> &packets)
{
    TestInitialSelectionAndRealtimeGrid();
    TestVorbisAudioView(visuals, services, texts, packets);
    {
        TestTaikoToolGroups(texts);
        EditorWorkspace state(CreateEditorMode("Taiko"),
                              std::make_unique<ChartEditor>(PatternDocument{}));
        state.AddNote({0, {}}, 15, MusicalPosition{0, {1, 2}}, {"HitCount=4", "Custom=keep"});
        const auto id = state.Document().Notes().front().note.sourceOrder;
        TaikoEditorNoteInspector inspector;
        inspector.Select(state.Document(), id + 1);
        inspector.Form().tool = 16;
        inspector.Form().hitCount = "6";
        inspector.Apply(state);
        Check(state.Document().FindNote(id)->head.note.keyType == 16 &&
                  state.Document().FindNote(id)->tail->note.keyType == 16 &&
                  state.Document().FindNote(id)->head.note.extraData ==
                      std::vector<std::string>{"Custom=keep", "HitCount=6"},
              "The inspector must update the paired type/count and preserve unrelated options.");
        state.SetRealtime(false);
        state.SelectTool(0);
        RecordingCanvas score;
        state.Mode().DrawChart(score, state, TestCatalog(Language::Korean));
        state.Mode().EditScore(state, {ScoreLeft, ScoreY}, false);
        score.labels.clear();
        state.Mode().DrawChart(score, state, TestCatalog(Language::Korean));
        state.Mode().DrawNotePanel(score, state, TestCatalog(Language::Korean));
        Check(std::ranges::find(score.labels, std::wstring(Name(16, TaikoEditor(TestCatalog(Language::Korean)).toolVariants))) !=
                  score.labels.end(),
              "Selecting a note must expose the note panel.");
        state.SetRealtime(true);
        score.images.clear();
        state.Mode().DrawChart(score, state, TestCatalog(Language::Korean));
        Check(std::ranges::any_of(score.images, [](const auto &path) { return path.filename() == L"DengDeng.png"; }),
              "The realtime lane must draw the focus-note skin sprite.");
        Check(std::ranges::any_of(score.labels,
                                  [](const auto &label) { return label.find(L"6 hits") != std::wstring::npos; }),
              "A long-note head must show its required hit count in both views.");
        inspector.Form().hitCount = "0";
        const auto revision = state.Document().Revision();
        bool rejected = false;
        try
        {
            inspector.Apply(state);
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        Check(rejected && state.Document().Revision() == revision,
              "Invalid inspector edits must preserve the document.");
    }
    {
        PatternDocument pattern;
        pattern.notes = {{{0, {}}, 11, 1, {}, {"HitCount=6"}}, {{40, {}}, 11, 2}};
        EditorWorkspace state(CreateEditorMode("Taiko"), std::make_unique<ChartEditor>(pattern));
        state.SetRealtime(false);
        state.SelectTool(0);
        RecordingCanvas score;
        state.Mode().DrawChart(score, state, TestCatalog(Language::English));
        state.Mode().EditScore(state, {800, ScoreY}, false);
        score.labels.clear();
        state.Mode().DrawNotePanel(score, state, TestCatalog(Language::English));
        Check(std::ranges::find(score.labels, L"6") != score.labels.end() && state.TimeMilliseconds() == 0,
              "A long body spanning the viewport must select its source without moving the cursor.");
        state.SetRealtime(true);
        state.Seek(8000);
        state.Mode().DrawChart(score, state, TestCatalog(Language::English));
        state.Mode().EditScore(state, {800, LaneY}, false);
        score.labels.clear();
        state.Mode().DrawNotePanel(score, state, TestCatalog(Language::English));
        Check(std::ranges::find(score.labels, L"6") != score.labels.end() && state.TimeMilliseconds() == 8000,
              "Realtime body selection must preserve the audio cursor and resolve the head.");
    }
    TestTaikoEditing();
    TestTaikoPlacementConflicts();
    TestPreviewMatchesPlay();
    TestRealtimeZoomAndTransport();
    TestLastNoteSeeking();
    TestSelectionClipboardAndHistory();
    TestDelayViewsAndSnapping();
    TestComposePanelEdits();
    TestSeekAndEffectEdits();
    TestInspectorMusicalTiming();
    TestRealtimeIgnoresScrollEffects();
    TestNewDifficultyDraft();
    auto mode = std::make_unique<ProbeMode>();
    auto *probe = mode.get();
    EditorWorkspace state(GameplayLaunchRequest{}, std::move(mode));
    state.Initialize();
    state.UpdateAnalysis();
    Check(!state.Analysis().Running(), "A mode without audio sources must not start the worker.");
    RecordingCanvas palette;
    state.Mode().DrawTools(palette, state, texts);
    palette.actions.front()();
    state.Mode().EditScore(state, {625, 280}, false);
    Check(state.Document().Notes().front().note.keyType == 70 &&
              state.Document().Notes().front().note.position.measure == 2,
          "Alternative mode tools/vertical coordinates did not reach the document adapter.");
    state.Save();
    Check(static_cast<const ProbeDocument &>(state.Document()).saved, "Save bypassed the selected format adapter.");
    state.Analysis().CacheAudioMarkers(state.Document(), state.Mode());
    state.Analysis().CacheAudioMarkers(state.Document(), state.Mode());
    Check(probe->markerCalls == 1 && state.Analysis().Markers().front().sound == "ProbeCue",
          "Common analysis must use the mode marker policy and cache the document revision.");
    state.PlaceNote({3, {}});
    state.Analysis().CacheAudioMarkers(state.Document(), state.Mode());
    Check(probe->markerCalls == 2, "An edit must invalidate the mode marker cache.");
    EditorView view(visuals, state, texts);
    view.Initialize(services);
    const auto hasText = [&](std::wstring_view text)
    { return std::ranges::any_of(packets(), [text](const auto &p) { return p.text && p.text->value == text; }); };
    visuals.Render({});
    Check(hasText(L"PROBE TOOL") && hasText(L"PROBE CHART") && !hasText(L"DON"),
          "Common view rendered Taiko instead of the selected mode.");
    state.SelectTab(EditorTab::Metadata);
    view.Build();
    visuals.Render({});
    Check(hasText(L"PROBE FORMAT METADATA") && !hasText(L"Chart by"),
          "Metadata view is still tied to YMP/YMM.");
    state.effectForm.selection = EditorHitSoundTarget{42};
    state.OpenPanel(EditorPanel::HitSounds);
    view.Build();
    visuals.Render({});
    Check(hasText(L"PROBE SOUND"), "Effects view still assumes exactly Don/Kat sound choices.");
    state.ClosePanel();
    state.SelectTab(EditorTab::Compose);
    probe->failDrawing = true;
    const auto oldCalls = probe->chartCalls;
    InputState input;
    const UpdateContext update{0, 0, 1, input, services.audio, {}};
    Check(!view.Update(update), "Drawing error must not leave the editor.");
    visuals.Render({});
    Check(state.Status() == "Injected chart drawing failure." && hasText(L"Injected chart drawing failure.") &&
          !state.NeedsRebuild(), "Drawing failure must render its status and stop automatic retry.");
    Check(probe->chartCalls == oldCalls + 1, "Drawing failure must render its status and stop automatic retry.");
    static_cast<void>(view.Update(update));
    Check(probe->chartCalls == oldCalls + 1, "A failed build retried without a new action.");
    probe->failDrawing = false;
    state.SelectTab(EditorTab::Metadata);
    static_cast<void>(view.Update(update));
    visuals.Render({});
    Check(hasText(L"PROBE FORMAT METADATA"), "Navigation must recover from a drawing failure.");
    view.Shutdown();
    Check(visuals.CanvasCount() == 0, "Mode replacement retained the previous editor Canvas.");
}
