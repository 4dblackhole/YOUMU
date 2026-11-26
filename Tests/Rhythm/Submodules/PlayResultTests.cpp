#include "Taiko/Results/TaikoPlayResult.h"
#include "Taiko/Results/TaikoResultCollector.h"
#include "TestSupport.h"
#include <limits>

namespace tests
{
    void TestLivePlayStatistics()
    {
        auto parsed = ChartParser{}.ParsePattern(
            "[Metadata]\nName: HUD\nBase BPM: 120\n[Difficulty]\nMode: Taiko\n"
            "[Time Signature]\n[Pattern]\n0/1,1,0\n1/4,3,0\n"
            "1/2,11,1,,HitCount=2\n3/4,11,2\n--\n0/1,2,0\n1/4,1,0\n1/2,1,0\n", "hud.ymp");
        Require(parsed.Succeeded(), "HUD fixture must parse.");
        auto loaded = TaikoMode{}.CreateSession(parsed.document);
        Require(loaded.Succeeded(), "HUD fixture must load.");
        auto &session = *loaded.session;
        const auto hit = [&](char key, std::int64_t time)
        {
            static_cast<void>(session.ProcessInput(key, InputEdge::Pressed, RhythmTime{time}));
            static_cast<void>(session.ProcessInput(key, InputEdge::Released, RhythmTime{time}));
        };
        Require(!session.Accuracy() && session.Score() == 0 && session.Statistics().combo == 0,
                "A new attempt has no accuracy sample and zero statistics.");
        hit('C', 0);
        Require(session.Accuracy()->Units() == 100'000'000 && session.Score() == 16'666'666 &&
                    session.Statistics().combo == 1,
                "A first MAX keeps current accuracy at 100 percent and earns one sixth of the full score.");
        hit('C', 500'000);
        Require(session.Statistics().combo == 1, "The first half of a big note is not another combo.");
        hit('M', 500'000);
        Require(session.Statistics().combo == 2, "A successful big note contributes one logical combo.");
        hit('C', 1'000'000);
        hit('C', 1'050'000);
        Require(session.Statistics().combo == 2, "Roll hits do not each increment combo.");
        static_cast<void>(session.Update(RhythmTime{1'500'000}));
        auto statistics = session.Statistics();
        Require(statistics.combo == 3 && statistics.maximumCombo == 3 && statistics.okCount == 1,
                "A successful roll adds one OK/combo; live counts share the result collector.");
        Require(statistics.judgements[0] == 2 && session.Accuracy()->Units() == 100'000'000 &&
                session.Score() == 50'000'000,
                "A successful roll adds one OK/combo; live counts share the result collector.");
        // The big note counts one MAX for its two hits, which are both timing samples.
        const auto gradedHits = [](const PlayStatistics &live)
        { return live.judgements[0] + live.judgements[1] + live.judgements[2] + live.judgements[3] + live.judgements[4] + 1; };
        Require(session.Statistics().timingErrorSum == RhythmDuration{} &&
                    session.Statistics().timingErrorCount == gradedHits(session.Statistics()),
                "On-time graded hits add zero error; every graded hit is one timing sample.");
        hit('D', 2'070'000);
        Require(session.Statistics().combo == 0 && session.Statistics().judgements[4] == 1,
                "BAD breaks combo even though the note completes.");
        Require(session.Statistics().timingErrorSum == RhythmDuration{70'000},
                "A late BAD adds its signed error to the live timing sum.");
        hit('C', 2'500'000);
        hit('D', 3'000'000);
        Require(session.Statistics().combo == 1 && session.Statistics().poorCount == 1,
                "A POOR counts at once but its note's combo waits for the note to finish; releases never add POOR.");
        hit('C', 3'000'000);
        Require(session.Statistics().combo == 0 && session.Statistics().maximumCombo == 3,
                "The MAX that answers a POOR is a recovery: it breaks the combo and keeps the maximum.");
        Require(session.Statistics().judgements[0] == 3 && session.Statistics().judgements[4] == 1 &&
                    session.Statistics().poorCount == 1,
                "The hit that answers a POOR adds no count: the note's one POOR is its result.");
        session.RewindJudgements(RhythmTime{2'000'000});
        statistics = session.Statistics();
        Require(statistics.combo == 3 && statistics.maximumCombo == 3 && statistics.poorCount == 0,
                "Rewind rebuilds score, combo, maximum and all counts from retained judgement history.");
        Require(statistics.judgements[4] == 0 && statistics.okCount == 1 && session.Accuracy()->Units() == 100'000'000,
                "Rewind rebuilds score, combo, maximum and all counts from retained judgement history.");
        Require(session.Score() == 50'000'000,
                "Rewind rebuilds score, combo, maximum and all counts from retained judgement history.");
        Require(statistics.timingErrorSum == RhythmDuration{} && statistics.timingErrorCount == gradedHits(statistics) &&
                    statistics.timingErrorCount > 0,
                "Rewind drops later timing samples and keeps the earlier ones, as the result does.");
        session.RetireRemaining(RhythmTime{2'000'000});
        Require(session.Statistics().combo == 0 && session.Statistics().judgements[5] == 3,
                "Retirement applies remaining logical misses to live counts and combo.");
        auto result = session.TakeResult(PlayEndReason::Interrupted, RhythmTime{2'000'000});
        Require(result->Summary().maximumCombo == 3 && session.Score() == 50'000'000 &&
                result->Summary().score == session.Score(),
                "Retire preserves earned points and maximum combo, and the result shows the same score.");
        Require(result->Summary().accuracy.Units() == 50'000'000,
                "Retire preserves earned points and maximum combo, and the result shows the same score.");
    }

    void TestComboEventOrder()
    {
        TaikoResultCollector collector({}, 2);
        collector.RegisterNote(1, TaikoNoteType::Don, {});
        collector.RegisterNote(2, TaikoNoteType::Don, RhythmTime{1});
        NoteEvent hit;
        hit.noteId = 1;
        hit.type = NoteEventType::HitAccepted;
        hit.judgement = {JudgementGrade::Max, {}, 1};
        auto completed = hit;
        completed.type = NoteEventType::Completed;
        completed.stateAfter = NoteState::Completed;
        NoteEvent poor = hit;
        poor.noteId = 2;
        poor.type = NoteEventType::InputRejected;
        NoteAccuracy accuracy;
        accuracy.noteId = 1;
        accuracy.finalized = true;
        accuracy.hitScoreRates = {1};
        accuracy.acceptedHits = 1;
        NoteProcessResult batch;
        batch.events = {hit, completed, poor};
        batch.finalizedAccuracies = {accuracy};
        collector.Consume(batch, true);
        Require(collector.Statistics().combo == 1 && collector.Statistics().poorCount == 1,
                "A POOR counts at once but leaves the combo of the note completed before it in the same batch.");
        // GAME-05: the combo settles when the POOR note finishes; left alone it stays that POOR.
        NoteEvent missed = poor;
        missed.type = NoteEventType::Missed;
        missed.stateAfter = NoteState::Missed;
        NoteAccuracy missedAccuracy;
        missedAccuracy.noteId = 2;
        missedAccuracy.finalized = true;
        missedAccuracy.hitScoreRates = {0};
        NoteProcessResult settle;
        settle.events = {missed};
        settle.finalizedAccuracies = {missedAccuracy};
        collector.Consume(settle, false);
        Require(collector.Statistics().judgements[5] == 0 && collector.Statistics().poorCount == 1,
                "A POOR note left alone stays that POOR, not a MISS.");
        Require(collector.Statistics().combo == 0 && collector.Statistics().maximumCombo == 1,
                "The POOR note breaks the combo when it settles and the maximum stays.");
        collector.ResetJudgements();
        Require(collector.Statistics().combo == 0 && collector.Statistics().maximumCombo == 0 &&
                    collector.Statistics().poorCount == 0, "Reset removes all HUD statistics.");
        hit.judgement.grade = JudgementGrade::Bad;
        batch.events = {hit, completed};
        collector.Consume(batch, true);
        Require(collector.Statistics().combo == 0, "A terminal event cannot turn a BAD hit into a successful combo.");
    }

    // A big note counts one judgement (user decisions): the grade of its two hits' average score, or only its POOR
    // or BAD with no average; a POOR wins over a BAD on the same note (GAME-03). A purple note is a kind of big note.
    // One hit with the other missing makes the note a MISS with zero accuracy.
    void TestBigNoteCountsOnce()
    {
        constexpr int BigDon = 3, Purple = 5;
        constexpr std::size_t Max = 0, Perfect = 1, Good = 3, Bad = 4, Miss = 5;
        const auto load = [](int keyType)
        {
            PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.notes.push_back(PatternNote{.keyType = keyType});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Big note fixture must load.");
            return std::move(loaded.session);
        };
        const auto play = [&](int keyType, std::initializer_list<std::pair<char, std::int64_t>> presses)
        {
            auto session = load(keyType);
            for (const auto &[key, at] : presses)
                static_cast<void>(session->ProcessInput(key, InputEdge::Pressed, RhythmTime{at}));
            static_cast<void>(session->Update(RhythmTime{500'000}));
            return session;
        };
        const auto counted = [](const PlayStatistics &statistics)
        {
            std::size_t total = statistics.poorCount;
            for (const auto count : statistics.judgements)
                total += count;
            return total;
        };

        auto statistics = play(BigDon, {{'C', 0}, {'M', 0}})->Statistics();
        Require(statistics.judgements[Max] == 1 && counted(statistics) == 1 && statistics.combo == 1,
                "Two MAX hits count one MAX.");
        statistics = play(BigDon, {{'C', 0}, {'M', 10'000}})->Statistics();
        Require(statistics.judgements[Perfect] == 1 && counted(statistics) == 1 && statistics.timingErrorCount == 2,
                "A MAX and a GREAT count the one grade of their average score; both hits stay timing samples.");
        statistics = play(BigDon, {{'C', -30'000}, {'M', 30'000}})->Statistics();
        Require(statistics.judgements[Good] == 1 && counted(statistics) == 1 &&
                    statistics.timingErrorSum == RhythmDuration{},
                "The average is of the hits' scores, not of their signed errors, which cancel here.");
        statistics = play(BigDon, {{'C', -70'000}, {'C', 0}, {'M', 0}})->Statistics();
        Require(statistics.judgements[Bad] == 1 && counted(statistics) == 1,
                "An early BAD answered on both hands counts only its BAD.");
        statistics = play(BigDon, {{'C', -70'000}, {'C', 0}})->Statistics();
        Require(statistics.judgements[Bad] == 1 && counted(statistics) == 1,
                "A BAD with the other hand missing counts only its BAD.");
        statistics = play(BigDon, {{'D', -20'000}, {'C', 0}, {'M', 0}})->Statistics();
        Require(statistics.poorCount == 1 && counted(statistics) == 1, "A POOR answered on both hands counts only its POOR.");
        statistics = play(BigDon, {{'C', -70'000}, {'D', -60'000}, {'C', 0}, {'M', 0}})->Statistics();
        Require(statistics.poorCount == 1 && counted(statistics) == 1, "A POOR after an early BAD counts only the POOR.");

        const auto oneSide = play(BigDon, {{'C', 0}});
        statistics = oneSide->Statistics();
        Require(statistics.judgements[Miss] == 1 && counted(statistics) == 1 && statistics.combo == 0,
                "One clean hit with the other hand missing counts one MISS.");
        Require(oneSide->AccuracyRate() == 0.0 && oneSide->Score() == 0,
                "A big note missed on one side scores zero, not half (user decision).");

        statistics = play(Purple, {{'C', 0}, {'D', 10'000}})->Statistics();
        Require(statistics.judgements[Perfect] == 1 && counted(statistics) == 1,
                "A purple note counts one grade for its two hits, like a big note.");
        statistics = play(Purple, {{'C', 0}, {'C', 5'000}})->Statistics();
        Require(statistics.poorCount == 1 && counted(statistics) == 1,
                "A purple note with a POOR after a clean hit counts only the POOR.");
        const auto purpleOneSide = play(Purple, {{'C', 0}});
        Require(purpleOneSide->Statistics().judgements[Miss] == 1 && counted(purpleOneSide->Statistics()) == 1,
                "A purple note hit on one side only counts one MISS.");
        Require(purpleOneSide->AccuracyRate() == 0.0, "A purple note hit on one side only scores zero.");

        auto retired = load(BigDon);
        static_cast<void>(retired->ProcessInput('C', InputEdge::Pressed, {}));
        retired->RetireRemaining(RhythmTime{10'000});
        statistics = retired->Statistics();
        Require(statistics.judgements[Miss] == 1 && counted(statistics) == 1,
                "A big note retired after one hit counts one MISS.");
    }

    void TestAccuracySixDecimalPrecision()
    {

        Require(AccuracyScore::FromRate(1.0 / 3).Units() == 33'333'333,
                "Percentage accuracy must truncate after six decimal places.");
        Require(AccuracyScore::Average(199'999'999, 2).Units() == 99'999'999,
                "The mean must use integer sums and truncate, rather than round up.");
        Require(AccuracyScore::FromRate(-1).Units() == 0 && AccuracyScore::FromRate(2).Units() == 100'000'000 &&
                    AccuracyScore::FromRate(std::numeric_limits<double>::quiet_NaN()).Units() == 0,
                "Score conversion must remain bounded and handle non-finite input.");
        for (const std::uint32_t units : {0U, 1U, 22'222'222U, 58'000'000U, 99'999'999U, 100'000'000U})
            Require(AccuracyScore::FromRate(AccuracyScore::FromUnits(units).Rate()).Units() == units,
                    "Converting a recorded score must not lose another decimal digit.");
        NoteAccuracy ticks;
        ticks.target = {NoteAccuracyKind::Ticks, 0, 3};
        ticks.acceptedTicks = 1;
        Require(ticks.Score().Units() == 33'333'333, "A logical tick note must record a six-decimal score.");
        AccuracyRange range;
        const auto judgement = range.Evaluate({}, RhythmTime{10'001});
        Require(judgement.scoreRate == AccuracyScore::FromRate(judgement.scoreRate).Rate(),
                "Each judgement score must already have the recording precision.");
    }

    void TestPlayResultCollection()
    {
        const auto parsed =
            ChartParser{}.ParsePattern("[Metadata]\nName: Result regression\nBase BPM: 120\n"
                                              "[Difficulty]\nMode: Taiko\n[Time Signature]\n[Pattern]\n"
                                              "0/1,1,0\n1/4,3,0\n1/2,11,1,,HitCount=3\n3/4,11,2\n--\n0/1,1,0\n",
                                              "result.ymp");
        Require(parsed.Succeeded(), "Result test chart must parse.");
        auto loaded = TaikoMode{}.CreateSession(parsed.document);
        Require(loaded.Succeeded(), "Result test session must load.");
        auto &session = *loaded.session;
        session.SetResultInitialTime(RhythmTime{-2'000'000});
        const auto press = [&](char key, std::int64_t us)
        { return session.ProcessInput(key, InputEdge::Pressed, RhythmTime{us}); };
        static_cast<void>(press('C', -1'000'000)); // Empty input is retained, never POOR.
        static_cast<void>(press('D', 2'000));
        static_cast<void>(session.ProcessInput('D', InputEdge::Released, RhythmTime{3'000}));
        const auto normal = press('C', 10'000);
        static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{10'000}));
        static_cast<void>(press('C', 500'000));
        const auto big = press('M', 510'000);
        static_cast<void>(press('C', 1'000'000));
        const auto roll = session.Update(RhythmTime{1'500'000});
        const auto expectedSum = static_cast<std::uint64_t>(normal.finalizedAccuracies.front().Score().Units()) +
                                 big.finalizedAccuracies.front().Score().Units() +
                                 roll.finalizedAccuracies.front().Score().Units();
        auto result = session.TakeResult(PlayEndReason::Interrupted, RhythmTime{1'500'000});
        const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Taiko result type must be preserved.");
        Require(taiko->summary.totalNotes == 4 && taiko->summary.processedNotes == 3 &&
                taiko->summary.unprocessedNotes == 1,
                "A long tail is not another logical note; interrupted accuracy uses the whole chart.");
        Require(taiko->summary.accuracy.Units() == expectedSum / 4,
                "A long tail is not another logical note; interrupted accuracy uses the whole chart.");
        Require(taiko->judgements[0] == 0 && taiko->judgements[1] == 1 && taiko->judgements[2] == 0,
                "The big note's MAX and GREAT count one PERFECT, their average.");
        Require(taiko->judgements[5] == 0, "Releases and blanks are never POOR or MISS.");
        Require(taiko->judgements[static_cast<std::size_t>(JudgementGrade::Bad)] == 0,
                "The GREAT that answers the first note's POOR adds no count: that note is its one POOR.");
        Require(taiko->poorCount == 1 && taiko->okCount == 1,
                "A roll that took one of its three hits is an OK, not a MISS.");
        Require(taiko->timingErrors.size() == 3 && taiko->meanErrorMilliseconds &&
                std::abs(*taiko->meanErrorMilliseconds - 20.0 / 3) < 1e-9,
                "Population variance must exclude automatic misses and free roll hits.");
        Require(std::abs(*taiko->varianceMillisecondsSquared - 200.0 / 9) < 1e-9,
                "Population variance must exclude automatic misses and free roll hits.");
        Require(taiko->groups[0].total == 2 && taiko->groups[1].total == 1 && taiko->groups[2].total == 1,
                "A POOR normal note scores zero; groups retain partial roll accuracy and note weights.");
        Require(taiko->groups[2].succeeded == 1u,
                "The roll that took a hit counts toward its group's combo-counted notes.");
        Require(taiko->notes[0].accuracy.Units() == 0 && taiko->notes[2].accuracy.Units() == 33'333'333 &&
                taiko->notes[3].accuracy.Units() == 0,
                "A POOR normal note scores zero; groups retain partial roll accuracy and note weights.");
        const auto &inputs = taiko->summary.inputs;
        Require(inputs.size() == 8 && inputs[0].time.count() == -1'000'000 && inputs[3].time == inputs[4].time,
                "Input logs must preserve signed microseconds, physical keys, edges and tied event order.");
        Require(inputs[3].edge == InputEdge::Pressed && inputs[4].edge == InputEdge::Released &&
                inputs[4].physicalKey == 'C',
                "Input logs must preserve signed microseconds, physical keys, edges and tied event order.");
        Require(taiko->summary.inputMapping.at('C') == inputs[3].action &&
                taiko->summary.initialTime.count() == -2'000'000,
                "Input logs must preserve signed microseconds, physical keys, edges and tied event order.");
        loaded.session.reset();
        Require(result->Summary().inputs.size() == 8 && taiko->notes.size() == 4,
                "Result ownership must be independent of the destroyed play session.");
    }

    void TestLongResultCompletionAndReset()
    {
        for (const auto type : {TaikoNoteType::Roll, TaikoNoteType::BigRoll, TaikoNoteType::Balloon,
                                TaikoNoteType::DengDeng, TaikoNoteType::TickRoll,
                                TaikoNoteType::BigTickRoll, TaikoNoteType::Buzz})
        {
            const bool timed = type == TaikoNoteType::TickRoll || type == TaikoNoteType::BigTickRoll ||
                               type == TaikoNoteType::Buzz;
            auto options = timed ? std::vector<std::string>{"TickDivision=4"} : std::vector<std::string>{"HitCount=1"};
            if (type == TaikoNoteType::Buzz)
                options.push_back("Action=Don");
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(type, std::move(options)));
            Require(loaded.Succeeded(), "Long result session must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            session.Reset();
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            if (type == TaikoNoteType::TickRoll || type == TaikoNoteType::BigTickRoll)
                for (const std::int64_t us : {500'000, 1'000'000})
                    static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{us}));
            const std::array<PhysicalKey, 1> held{'C'};
            static_cast<void>(session.Update(RhythmTime{1'000'000}, held));
            static_cast<void>(session.Update(RhythmTime{1'100'000}, held));
            auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{1'100'000});
            const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
            if (!taiko)
                throw std::runtime_error("Long Taiko result type must be preserved.");
            Require(taiko->okCount == 1 && taiko->judgements[5] == 0 && taiko->summary.processedNotes == 1,
                    "Each successful long-note completion must produce exactly one OK.");
            Require(taiko->summary.accuracy.Units() == 100'000'000,
                    "Each successful long-note completion must produce exactly one OK.");
            Require(taiko->summary.maximumCombo == 1,
                    "Every long-note kind contributes exactly one combo on successful completion.");
            Require(taiko->timingErrors.size() == (type == TaikoNoteType::Buzz ? 1U : 0U),
                    "Buzz head and HoldStarted must not duplicate one timing error.");
            Require(taiko->summary.inputs.size() ==
                        (type == TaikoNoteType::TickRoll || type == TaikoNoteType::BigTickRoll ? 3U : 1U),
                    "Reset must remove the previous run's log.");
        }
        auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don"}));
        Require(loaded.Succeeded(), "Interrupted Buzz fixture must load.");
        static_cast<void>(loaded.session->ProcessInput('C', InputEdge::Pressed, {}));
        auto result = loaded.session->TakeResult(PlayEndReason::Interrupted, RhythmTime{10'000});
        Require(result->Summary().unprocessedNotes == 1 && result->Summary().accuracy.Units() == 0,
                "A partially active long note remains unprocessed with zero score on interruption.");
    }

    void TestPartialLongNotesCountAsOk()
    {
        for (const int hits : {0, 1})
        {
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::Roll, {"HitCount=3"}));
            Require(loaded.Succeeded(), "Partial roll fixture must load.");
            auto &session = *loaded.session;
            if (hits > 0)
                static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(session.Update(RhythmTime{1'100'000}));
            auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{1'100'000});
            const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
            if (!taiko)
                throw std::runtime_error("Partial roll result type must be preserved.");
            const bool progressed = hits > 0;
            Require(taiko->okCount == (progressed ? 1U : 0U) && taiko->judgements[5] == (progressed ? 0U : 1U),
                    "A long note with any hit is an OK; only an untouched one is a MISS.");
            Require(taiko->summary.maximumCombo == (progressed ? 1U : 0U),
                    "A partly processed long note keeps the combo; an untouched one breaks it.");
            Require(taiko->notes[0].accuracy.Units() == (progressed ? 33'333'333U : 0U),
                    "Counting it as OK does not change the note's accuracy.");
        }
    }

    void TestLongNoteCompletionGrade()
    {
        const auto grade = [](NoteAccuracyKind kind, std::size_t hits, std::size_t ticks,
                              std::size_t acceptedHits, std::size_t acceptedTicks, std::vector<double> rates = {})
        {
            NoteAccuracy accuracy;
            accuracy.target = {kind, hits, ticks};
            accuracy.acceptedHits = acceptedHits;
            accuracy.acceptedTicks = acceptedTicks;
            accuracy.hitScoreRates = std::move(rates);
            return accuracy.CompletionGrade();
        };
        Require(grade(NoteAccuracyKind::HitCount, 3, 0, 0, 0) == JudgementGrade::Miss,
                "A long note with no accepted hit or tick shows MISS.");
        Require(grade(NoteAccuracyKind::HitCount, 3, 0, 1, 0) == JudgementGrade::Good,
                "A third of the hits shows GOOD.");
        Require(grade(NoteAccuracyKind::HitCount, 3, 0, 5, 0) == JudgementGrade::Max,
                "Extra hits never exceed MAX.");
        Require(grade(NoteAccuracyKind::Ticks, 0, 100, 0, 99) == JudgementGrade::Perfect &&
                    grade(NoteAccuracyKind::Ticks, 0, 100, 0, 90) == JudgementGrade::Great &&
                    grade(NoteAccuracyKind::Ticks, 0, 100, 0, 89) == JudgementGrade::Good,
                "Tick ratios use the timing grades' score boundaries: 99% PERFECT, 90% GREAT.");
        Require(grade(NoteAccuracyKind::Hold, 1, 10, 1, 10, {1.0}) == JudgementGrade::Max &&
                    grade(NoteAccuracyKind::Hold, 1, 10, 1, 1, {1.0}) == JudgementGrade::Good,
                "A Buzz averages its head score and body ticks before choosing the grade.");
        Require(grade(NoteAccuracyKind::Hold, 1, 10, 1, 1, {0.0}) == JudgementGrade::Good,
                "A Buzz whose head was accepted at a zero score but took a tick is processed, so it is not a MISS.");
        Require(grade(NoteAccuracyKind::Hold, 1, 10, 1, 0, {1.0}) == JudgementGrade::Miss,
                "GAME-05: a Buzz that took no body tick ends as a MISS, even after a MAX head.");
    }
} // namespace tests
