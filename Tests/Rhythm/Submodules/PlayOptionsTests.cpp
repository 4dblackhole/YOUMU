#include "Records/LocalOffsetStore.h"
#include "Records/PlayRecordStore.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "TestSupport.h"
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <winsqlite/winsqlite3.h>
#pragma comment(lib, "winsqlite3.lib")

namespace tests
{
    void TestAcceleratedClockAndWindows()
    {
        AccuracyRange range("test", 60);
        for (double rate : {.25, .5, 1., 1.5, 2.})
        {
            RhythmTimer timer;
            timer.Start(1'000'000, 1'000'000, RhythmTime{-100'000}, rate);
            timer.AnchorDspClock(RhythmTime{-100'000}, 48'000, 48'000);
            const auto at = timer.Now(2'000'000);
            Require(at == RhythmTime{static_cast<RhythmTime::rep>(rate * 1'000'000 - 100'000)},
                    "Rate scales QPC chart time once.");
            Require(timer.ToDspClock(at) == 96'000 && timer.FromDspClock(96'000) == at,
                    "Rate preserves roundtrip QPC/DSP mapping.");
            const auto window = range.HalfWindow(JudgementGrade::Good);
            const auto wallEdge = static_cast<std::int64_t>(std::ceil(window.count() / rate));
            timer.Start(0, 1'000'000, RhythmTime{}, rate);
            Require(timer.Now(wallEdge).count() >= window.count(),
                    "Real-time judgement half-window is chart window divided by rate.");
            timer.Pause(2'000'000);
            const auto frozen = timer.Now(9'000'000);
            timer.Resume(9'000'000);
            Require(timer.Now(10'000'000) == frozen + RhythmTime{static_cast<RhythmTime::rep>(rate * 1'000'000)},
                    "Pause/resume preserves rate.");
            timer.Seek(RhythmTime{123}, 11'000'000);
            Require(timer.Now(11'000'000) == RhythmTime{123}, "Seek uses original chart coordinates.");
        }
    }
    void TestTaikoPlayOptions()
    {
        // HP gauge choice (UI-02 Play options): only the default gauge exists and owns the 60% clear criterion.
        PlayOptions gauge;
        Require(gauge.hpGauge == HpGaugeKind::Default && HpGauges.size() == 1 &&
                    FindHpGauge(HpGaugeKind::Default)->clearPercent == 60.0,
                "Play options start with the default HP gauge and its 60% clear criterion.");
        gauge.hpGauge = static_cast<HpGaugeKind>(7);
        bool unknownRejected{};
        try { gauge.Validate(); }
        catch (const std::invalid_argument &) { unknownRejected = true; }
        Require(unknownRejected, "An unknown HP gauge is not a valid play option.");
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.judgementLevel = 50;
        auto add = [&](int measure, int type, int action, std::vector<std::string> extra = {})
        {
            PatternNote n;
            n.position = {measure, {}};
            n.keyType = type;
            n.actionType = action;
            n.extraData = std::move(extra);
            n.sourceOrder = pattern.notes.size();
            pattern.notes.push_back(n);
        };
        add(0, 1, 0);
        add(1, 2, 0);
        add(2, 3, 0);
        add(3, 4, 0);
        add(4, 5, 0);
        add(5, 17, 1, {"Action=Don", "TickDivision=16"});
        add(6, 17, 2);
        add(7, 17, 1, {"Action=Kat", "TickDivision=16"});
        add(8, 17, 2);
        EffectDocument effects;
        EffectCommand speed;
        speed.type = EffectCommandType::ScrollSpeed;
        speed.target = "Whole";
        speed.beginValue = speed.endValue = 3;
        effects.commands.push_back(speed);
        speed.type = EffectCommandType::NoteSpeed;
        speed.target = "Separate";
        speed.beginValue = speed.endValue = 2;
        effects.commands.push_back(speed);
        PlaySessionOptions options;
        options.play.mirror = true;
        options.play.judgeLevel = 60;
        options.play.scrollUnit = PlayOptions::ScrollUnit::Bpm;
        options.play.scrollSpeed = 240;
        options.play.ignoreScrollEffects = true;
        options.play.metronome = true;
        auto loaded = TaikoMode(options).CreateSession(pattern, effects);
        Require(loaded.Succeeded(), "Option fixture loads.");
        const std::array expected{NoteVisualKind::Kat,    NoteVisualKind::Don,    NoteVisualKind::BigKat,
                                  NoteVisualKind::BigDon, NoteVisualKind::Purple, NoteVisualKind::KatBuzz,
                                  NoteVisualKind::DonBuzz};
        for (std::size_t i = 0; i < expected.size(); ++i)
            Require(loaded.session->FindNotePresentation(i + 1)->visualKind == expected[i],
                    "Mirror changes all eligible color groups, preserving purple and long pairing.");
        Require(loaded.session->Effects().empty(), "Ignore speed removes Whole/Separate automation.");
        Require(loaded.session->ScrollDistance(RhythmTime{1'000'000}, RhythmTime{}) == 2'000'000,
                "BPM scroll target scales against base BPM.");
        Require(loaded.session->MinimumScrollMultiplier() == 2, "Visibility expansion uses global scroll multiplier.");
        Require(!loaded.session->metronomeBeats.empty() && loaded.session->metronomeBeats.front().accent,
                "Metronome compiles quarter beats and measure accents off the update thread.");
        const auto hit = loaded.session->ProcessInput('D', InputEdge::Pressed, RhythmTime{});
        Require(HasEvent(hit, NoteEventType::Completed) && hit.audioCues.front().sound == "Taiko.Kat.Hit",
                "Mirrored gameplay action and default sound match presentation.");
        auto result = loaded.session->TakeResult(PlayEndReason::Interrupted, RhythmTime{});
        Require(result->Summary().judgementLevel == 60 && result->Summary().playOptions.mirror,
                "Result freezes effective JudgeLevel and options.");
        Require(pattern.notes.front().keyType == 1, "Play options never mutate source pattern documents.");
        // Accelerate by target BPM: the chart's base BPM (120) turns it into the session's multiplier, within 0.5..2x.
        {
            PlaySessionOptions bpm;
            bpm.play.accelerateUnit = PlayOptions::ScrollUnit::Bpm;
            const auto multiplierFor = [&](const double target) {
                bpm.play.accelerate = target;
                auto session = TaikoMode(bpm).CreateSession(pattern);
                Require(session.Succeeded(), "A positive target BPM is a valid Accelerate.");
                Require(session.session->Options().accelerateUnit == PlayOptions::ScrollUnit::Multiplier,
                        "The session keeps the resolved Accelerate multiplier, not the target BPM.");
                const auto multiplier = session.session->Options().accelerate;
                auto result = session.session->TakeResult(PlayEndReason::Interrupted, RhythmTime{});
                Require(result->Summary().playOptions.accelerate == multiplier &&
                            result->Summary().playOptions.accelerateUnit == PlayOptions::ScrollUnit::Multiplier,
                        "Results and records keep the multiplier that was played.");
                return multiplier;
            };
            Require(multiplierFor(180) == 1.5, "Target BPM 180 on a 120 BPM chart plays at 1.5x.");
            Require(multiplierFor(300) == 2.0 && multiplierFor(30) == 0.5,
                    "A target BPM beyond 0.5..2x of the chart plays at the nearest limit.");
            PlayOptions invalid;
            invalid.accelerateUnit = PlayOptions::ScrollUnit::Bpm;
            invalid.accelerate = 0;
            bool zeroRejected{};
            try { invalid.Validate(); }
            catch (const std::invalid_argument &) { zeroRejected = true; }
            invalid.accelerateUnit = PlayOptions::ScrollUnit::Multiplier;
            invalid.accelerate = 2.5;
            bool multiplierRejected{};
            try { invalid.Validate(); }
            catch (const std::invalid_argument &) { multiplierRejected = true; }
            Require(zeroRejected && multiplierRejected, "Target BPM must be positive and a multiplier stays 0.5..2.");
        }
        // Max strokes: each column plays only its first assigned keys in slot order (default 2, range 1..4).
        {
            PlaySessionOptions strokes;
            strokes.inputKeys["LeftKat"] = {'D', 'F', 'G', 'H'};
            strokes.inputKeys["LeftDon"] = {0, 'V', 0, 'B'};
            const auto keysFor = [&](const int limit, const std::string &id) {
                strokes.play.maxStrokes = limit;
                auto session = TaikoMode(strokes).CreateSession(pattern);
                Require(session.Succeeded(), "Max strokes 1..4 is a valid play option.");
                const auto &bindings = session.session->InputLayout().bindings;
                return std::ranges::find(bindings, id, &ModeInputBinding::id)->keys;
            };
            Require(PlayOptions{}.maxStrokes == 2, "Max strokes starts at 2.");
            Require(keysFor(2, "LeftKat") == std::vector<PhysicalKey>{'D', 'F', 0, 0},
                    "Two strokes keep the primary key and the first secondary key.");
            Require(keysFor(4, "LeftKat") == std::vector<PhysicalKey>{'D', 'F', 'G', 'H'},
                    "Four strokes keep every assigned key.");
            Require(keysFor(1, "LeftDon") == std::vector<PhysicalKey>{0, 'V', 0, 0},
                    "Empty slots do not count as strokes; the first assigned key stays.");
            const auto cuesFor = [&](const PhysicalKey key) {
                strokes.play.maxStrokes = 3;
                auto session = TaikoMode(strokes).CreateSession(pattern);
                return session.session->ProcessInput(key, InputEdge::Pressed, RhythmTime{}).audioCues.size();
            };
            Require(cuesFor('G') > 0 && cuesFor('H') == 0, "A key beyond the limit plays nothing, not even a free hit.");
            PlayOptions invalid;
            bool lowRejected{}, highRejected{};
            invalid.maxStrokes = 0;
            try { invalid.Validate(); }
            catch (const std::invalid_argument &) { lowRejected = true; }
            invalid.maxStrokes = 5;
            try { invalid.Validate(); }
            catch (const std::invalid_argument &) { highRejected = true; }
            Require(lowRejected && highRejected, "Max strokes stays within 1..4.");
            ModeInputLayout fixed{"Fixed", {{"A", "A", 1, {'A', 'S'}}}, false};
            bool fixedRejected{};
            try { static_cast<void>(LimitKeysPerBinding(fixed, 1)); }
            catch (const std::invalid_argument &) { fixedRejected = true; }
            Require(fixedRejected && LimitKeysPerBinding(fixed, 2).bindings.front().keys.size() == 2,
                    "A layout without empty slots cannot drop a key.");
        }
        options.play.random = true;
        options.play.randomAttemptProbability = 0;
        auto unchanged = TaikoMode(options).CreateSession(pattern);
        for (std::size_t i = 0; i < expected.size(); ++i)
            Require(unchanged.session->FindNotePresentation(i + 1)->visualKind == expected[i],
                    "Zero attempt probability preserves mirrored notes.");
        options.play.randomAttemptProbability = 1;
        options.play.randomSeed = 123456;
        auto a = TaikoMode(options).CreateSession(pattern), b = TaikoMode(options).CreateSession(pattern);
        bool stayed{}, changed{};
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            const auto kind = a.session->FindNotePresentation(i + 1)->visualKind;
            Require(kind == b.session->FindNotePresentation(i + 1)->visualKind,
                    "Same Random seed reproduces the whole attempt.");
            if (i < 4)
            {
                stayed |= kind == expected[i];
                changed |= kind != expected[i];
            }
            if (i < 2)
                Require(kind == NoteVisualKind::Don || kind == NoteVisualKind::Kat,
                        "Small Random stays in the small group.");
            else if (i < 4)
                Require(kind == NoteVisualKind::BigDon || kind == NoteVisualKind::BigKat,
                        "Big Random stays in the big group.");
            else if (i == 4)
                Require(kind == NoteVisualKind::Purple, "Random excludes purple.");
            else
                Require(kind == NoteVisualKind::DonBuzz || kind == NoteVisualKind::KatBuzz,
                        "Buzz randomization stays in the buzz group.");
        }
        Require(stayed && changed, "100 percent attempts allow both same-color and changed assignments.");
    }
    void TestPersistentRecordConditions()
    {
        TemporaryDirectory directory("youmu-record-options");
        const auto database = directory.Path() / "plays.sqlite3";
        const auto pattern = directory.Path() / "real-pattern.ymp";
        std::ofstream(pattern) << "fixture";
        PlayRecord r;
        r.patternPath = pattern;
        r.mode = "Taiko";
        r.completed = true;
        r.cleared = true;
        r.judgeLevel = 70;
        r.options.random = true;
        r.options.randomSeed = std::numeric_limits<std::uint64_t>::max();
        r.options.randomAttemptProbability = .35;
        r.options.accelerate = 1.5;
        r.options.preserveHitSoundPitch = false;
        r.options.judgeLevel = 70;
        r.accuracy = AccuracyScore::FromUnits(99'123'456);
        r.totalNotes = 123;
        r.processedNotes = 123;
        PlayRecordStore(database).Save(r);
        r.judgeLevel = 50;
        r.options.random = false;
        r.options.judgeLevel.reset();
        PlayRecordStore(database).Save(r);
        auto all = PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10);
        Require(all.size() == 2 && all[0].id > all[1].id, "A new store instance sees persistent newest-first records.");
        const auto filtered =
            PlayRecordStore(database).Query(pattern, "Taiko", "JudgeLevel >= 60 AND Random = true", 0, 10);
        Require(filtered.size() == 1 && filtered[0].accuracy.Units() == 99'123'456 &&
                    filtered[0].options.randomSeed == std::numeric_limits<std::uint64_t>::max(),
                "Condition query preserves exact accuracy, full uint64 seed and actual effective level.");
        Require(filtered[0].options.judgeLevel == JudgeLevel{70} && !filtered[0].options.preserveHitSoundPitch &&
                    filtered[0].options.accelerate == 1.5,
                "Records preserve independent pitch and JudgeLevel settings.");
        Require(PlayRecordStore(database)
                        .Query(pattern, "Taiko", "NOT (Random = false) OR JudgeLevel < 60", 0, 10)
                        .size() == 2,
                "Boolean precedence and nested conditions work.");
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "RandomSeed = 18446744073709551615", 0, 10).size() ==
                    2,
                "Seed equality uses the full integer range without floating-point loss.");
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 1, 1).size() == 1,
                "History pagination does not hide older records.");
        for (const auto invalid : {"JudgeLevel >= 60; DELETE FROM Plays", "FakeOption = 1", "Random > true",
                                   "Random = 2", "JudgeLevel = nan", "(Random=true", "RandomSeed > 1"})
        {
            bool rejected{};
            try
            {
                PlayRecordStore::ValidateQuery(invalid);
            }
            catch (const std::invalid_argument &)
            {
                rejected = true;
            }
            Require(rejected, "Invalid query is rejected before execution.");
        }
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10).size() == 2,
                "Invalid queries never mutate stored records.");
    }

    void TestOldRecordSchemaSetAside()
    {
        TemporaryDirectory directory("youmu-record-schema");
        const auto database = directory.Path() / "Result" / "plays.sqlite3";
        std::filesystem::create_directories(database.parent_path());
        // An accuracy-only database as schema 1 left it.
        const auto writeSchemaOne = [&]
        {
            sqlite3 *raw{};
            Require(sqlite3_open16(database.c_str(), &raw) == SQLITE_OK, "Schema-1 fixture opens.");
            const int status = sqlite3_exec(raw, "CREATE TABLE Plays (Id INTEGER PRIMARY KEY); PRAGMA user_version=1",
                                            nullptr, nullptr, nullptr);
            sqlite3_close_v2(raw);
            Require(status == SQLITE_OK, "Schema-1 fixture is written.");
        };
        const auto schemaOf = [](const std::filesystem::path &file)
        {
            sqlite3 *raw{};
            sqlite3_stmt *statement{};
            int version = -1;
            const bool opened = sqlite3_open16(file.c_str(), &raw) == SQLITE_OK;
            if (opened && sqlite3_prepare_v2(raw, "PRAGMA user_version", -1, &statement, nullptr) == SQLITE_OK)
            {
                if (sqlite3_step(statement) == SQLITE_ROW)
                    version = sqlite3_column_int(statement, 0);
            }
            sqlite3_finalize(statement);
            sqlite3_close_v2(raw);
            return version;
        };
        writeSchemaOne();
        const auto pattern = directory.Path() / "pattern.ymp";
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10).empty(),
                "Opening a schema-1 database starts the records over.");
        const auto setAside = database.parent_path() / "plays.v1.sqlite3";
        Require(schemaOf(setAside) == 1 && schemaOf(database) == PlayRecordStore::SchemaVersion,
                "The old database is kept beside the new one as plays.v1.sqlite3.");
        PlayRecord record;
        record.patternPath = pattern;
        record.mode = "Taiko";
        PlayRecordStore(database).Save(record);
        std::filesystem::remove(database);
        writeSchemaOne();
        PlayRecordStore(database).Save(record);
        Require(schemaOf(database.parent_path() / "plays.v1 (2).sqlite3") == 1 && schemaOf(setAside) == 1,
                "A second old database takes the next free name and the first stays.");
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10).size() == 1,
                "Saving into a schema-1 database keeps only the new record.");
    }

    void TestRecordResultDetail()
    {
        TemporaryDirectory directory("youmu-record-detail");
        const auto database = directory.Path() / "plays.sqlite3";
        const auto pattern = directory.Path() / "pattern.ymp";
        std::ofstream(pattern) << "fixture";
        TaikoPlayResult result;
        auto &s = result.summary;
        s.patternPath = pattern;
        s.patternName = "Oni";
        s.accuracy = AccuracyScore::FromUnits(97'531'400);
        s.score = 97'531'400;
        s.totalNotes = 400;
        s.processedNotes = 398;
        s.unprocessedNotes = 2;
        s.maximumCombo = 175;
        s.endReason = PlayEndReason::Completed;
        s.initialTime = RhythmTime{-2'000'000};
        s.endTime = RhythmTime{112'345'678};
        s.judgementLevel = 70;
        s.playOptions.mirror = true;
        s.playOptions.accelerate = 1.25;
        s.controls = {{PlayControlType::Pause, RhythmTime{5'000}, RhythmTime{5'000}, 3, true},
                      {PlayControlType::Resume, RhythmTime{5'000}, RhythmTime{4'000}, 3, false}};
        s.hpGauges = {{83.25, true, true, 60.0}};
        s.hpHistory = std::vector<HpHistoryPoint>{{RhythmTime{0}, 0.0}, {RhythmTime{1'500'000}, 12.5}};
        result.judgements = {302, 62, 30, 2, 0, 6};
        result.okCount = 12;
        result.poorCount = 1;
        result.acceptedTicks = 64;
        result.missedTicks = 3;
        result.groups[0] = {300, 300, AccuracyScore::FromUnits(97'260'000)};
        result.groups[2] = {10, 8, AccuracyScore::FromUnits(100'000'000)};
        result.timingErrors = {{7, 0, RhythmTime{1'002'000}, RhythmDuration{-4'321}, JudgementGrade::Perfect,
                                AccuracyScore::FromUnits(99'000'000)},
                               {std::numeric_limits<NoteId>::max(), 2, RhythmTime{2'000'000}, RhythmDuration{58'000},
                                JudgementGrade::Bad, AccuracyScore{}}};
        result.meanErrorMilliseconds = 0.58;
        result.varianceMillisecondsSquared = 149.3;
        result.standardDeviationMilliseconds.reset();

        auto record = PlayRecord::From(result);
        Require(record.score == 97'531'400 && record.maximumCombo == 175 && !record.detail.empty(),
                "A record carries the score, the maximum combo and the mode's detail.");
        PlayRecordStore(database).Save(record);
        const auto stored = PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 10);
        Require(stored.size() == 1, "The record is stored once.");
        Require(stored[0].score == 97'531'400 && stored[0].maximumCombo == 175,
                "Queries return the score and the maximum combo.");
        Require(stored[0].detail.empty(), "Queries leave the detail for the one record that needs it.");
        auto loaded = stored[0];
        loaded.detail = PlayRecordStore(database).LoadDetail(loaded.id);
        Require(loaded.detail == record.detail && PlayRecordStore(database).LoadDetail(loaded.id + 1).empty(),
                "The detail reads back byte for byte; a missing record has none.");
        const auto restored = loaded.LoadResult();
        const auto *taiko = dynamic_cast<const TaikoPlayResult *>(restored.get());
        Require(taiko != nullptr, "A Taiko record restores a Taiko result.");
        const auto &r = taiko->summary;
        Require(r.accuracy.Units() == 97'531'400 && r.score == 97'531'400, "Accuracy and score survive the record.");
        Require(r.maximumCombo == 175 && r.totalNotes == 400, "Maximum combo and note total survive the record.");
        Require(r.processedNotes == 398 && r.unprocessedNotes == 2, "Processed counts survive the record.");
        Require(r.endReason == PlayEndReason::Completed && r.judgementLevel == JudgeLevel{70},
                "End reason and JudgeLevel survive the record.");
        Require(r.initialTime == s.initialTime && r.endTime == s.endTime, "Start and end times survive the record.");
        Require(r.playOptions.mirror && r.playOptions.accelerate == 1.25, "Play options survive the record.");
        Require(r.controls.size() == 2 && r.controls[1].to == RhythmTime{4'000}, "Control markers survive the record.");
        Require(r.controls[0].pauseMenuShown && r.controls[0].inputIndex == 3, "Control marker fields survive the record.");
        Require(r.hpGauges.size() == 1 && r.hpGauges[0].percent == 83.25, "HP gauges survive the record.");
        Require(r.hpGauges[0].passed && r.hpHistory.has_value(), "The gauge clear and the HP curve survive the record.");
        Require(r.hpHistory->size() == 2 && r.hpHistory->back().percent == 12.5, "HP curve points survive the record.");
        Require(taiko->judgements == result.judgements && taiko->okCount == 12, "Judgement counts survive the record.");
        Require(taiko->poorCount == 1, "The POOR count survives the record.");
        Require(taiko->acceptedTicks == 64 && taiko->missedTicks == 3, "Tick counts survive the record.");
        Require(taiko->groups[0].accuracy.Units() == 97'260'000 && taiko->groups[2].processed == 8,
                "Group accuracies survive the record.");
        Require(taiko->groups[1].total == 0, "An empty group stays empty.");
        Require(taiko->timingErrors.size() == 2 && taiko->timingErrors[0].error == RhythmDuration{-4'321},
                "Timing samples survive the record.");
        Require(taiko->timingErrors[1].noteId == std::numeric_limits<NoteId>::max() &&
                    taiko->timingErrors[1].grade == JudgementGrade::Bad,
                "Timing sample ids and grades survive the record.");
        Require(taiko->meanErrorMilliseconds == 0.58 && taiko->varianceMillisecondsSquared == 149.3,
                "Timing statistics survive exactly.");
        Require(!taiko->standardDeviationMilliseconds, "An absent timing statistic stays absent.");
        Require(r.IsCleared() == s.IsCleared(), "The clear follows from the restored HP and controls.");

        const auto rejects = [&](std::string detail)
        {
            auto damaged = loaded;
            damaged.detail = std::move(detail);
            try
            {
                static_cast<void>(damaged.LoadResult());
            }
            catch (const std::runtime_error &)
            {
                return true;
            }
            return false;
        };
        auto badGrade = record.detail;
        badGrade.replace(badGrade.find(" 4 0\n"), 5, " 9 0\n");
        Require(rejects("") && rejects(record.detail.substr(0, record.detail.size() / 2)),
                "A missing or cut detail is rejected.");
        Require(rejects(record.detail + "1") && rejects("taiko-result 2\n"), "A padded or newer detail is rejected.");
        Require(rejects(badGrade), "An out-of-range value is rejected.");
    }

    void TestLocalOffsetStore()
    {
        TemporaryDirectory directory("youmu-local-offsets");
        const auto file = directory.Path() / "Data" / "LocalOffsets.txt";
        const auto first = directory.Path() / "Songs" / "A" / "Hard.ymp";
        const auto second = directory.Path() / "Songs" / "B" / "Easy.ymp";
        LocalOffsetStore store(file);
        Require(store.Get(first) == 0 && !std::filesystem::exists(file), "Without a file every offset is zero.");
        std::string error;
        Require(store.Set(first, 15, error) && store.Set(second, -40, error) && error.empty(), "Offsets save.");
        Require(LocalOffsetStore(file).Get(first) == 15 && LocalOffsetStore(file).Get(second) == -40,
                "A new store instance reads each pattern's offset.");
        auto spelled = directory.Path() / "songs" / "a" / "." / "HARD.YMP";
        Require(store.Get(spelled) == 15, "Patterns are matched by their normalized, case-insensitive path.");
        Require(store.Set(first, 5000, error) && store.Get(first) == LocalOffsetStore::Limit,
                "Offsets are clamped to the limit.");
        Require(store.Set(first, 0, error) && store.Get(first) == 0 && store.Get(second) == -40,
                "A zero offset is removed without touching other patterns.");
        {
            std::ofstream damaged(file, std::ios::app | std::ios::binary);
            damaged << "not-a-number\tsomething\r\n";
        }
        Require(store.Get(second) == -40, "A damaged line is skipped.");
        Require(!std::filesystem::exists(file.string() + ".tmp"), "No temporary file is left.");
    }

    void TestTimerTimelineShift()
    {
        RhythmTimer timer;
        timer.Start(1'000, 1'000, RhythmTime{250'000});
        timer.AnchorDspClock(RhythmTime{250'000}, 48'000, 48'000);
        const auto before = timer.Now(2'000);
        const auto dspBefore = timer.ToDspClock(RhythmTime{1'000'000});
        timer.ShiftTimeline(RhythmDuration{-5'000});
        Require(timer.Now(2'000) == before - RhythmDuration{5'000},
                "Shifting the timeline moves every instant's rhythm time by the shift.");
        Require(timer.ToDspClock(RhythmTime{1'000'000} - RhythmDuration{5'000}) == dspBefore,
                "The DSP mapping moves with it, so scheduled audio keeps its physical time.");
    }
} // namespace tests
