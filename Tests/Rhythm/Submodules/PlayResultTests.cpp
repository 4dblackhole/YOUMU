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
        hit('F', 0);
        Require(session.Accuracy()->Units() == 100'000'000 && session.Score() == 16'666'666 &&
                    session.Statistics().combo == 1,
                "A first MAX keeps current accuracy at 100 percent and earns one sixth of the full score.");
        hit('F', 500'000);
        Require(session.Statistics().combo == 1, "The first half of a big note is not another combo.");
        hit('J', 500'000);
        Require(session.Statistics().combo == 2, "A successful big note contributes one logical combo.");
        hit('F', 1'000'000);
        hit('F', 1'050'000);
        Require(session.Statistics().combo == 2, "Roll hits do not each increment combo.");
        static_cast<void>(session.Update(RhythmTime{1'500'000}));
        auto statistics = session.Statistics();
        Require(statistics.combo == 3 && statistics.maximumCombo == 3 && statistics.okCount == 1 &&
                    statistics.judgements[0] == 3 && session.Accuracy()->Units() == 100'000'000 &&
                    session.Score() == 50'000'000,
                "A successful roll adds one OK/combo; live counts share the result collector.");
        hit('D', 2'070'000);
        Require(session.Statistics().combo == 0 && session.Statistics().judgements[4] == 1,
                "BAD breaks combo even though the note completes.");
        hit('F', 2'500'000);
        hit('D', 3'000'000);
        Require(session.Statistics().combo == 0 && session.Statistics().poorCount == 1,
                "A real POOR immediately breaks combo; key releases never add POOR.");
        hit('F', 3'000'000);
        Require(session.Statistics().combo == 1 && session.Statistics().maximumCombo == 3,
                "A later accepted completion starts a new combo and preserves the maximum.");
        session.RewindJudgements(RhythmTime{2'000'000});
        statistics = session.Statistics();
        Require(statistics.combo == 3 && statistics.maximumCombo == 3 && statistics.poorCount == 0 &&
                    statistics.judgements[4] == 0 && statistics.okCount == 1 &&
                    session.Accuracy()->Units() == 100'000'000 && session.Score() == 50'000'000,
                "Rewind rebuilds score, combo, maximum and all counts from retained judgement history.");
        session.RetireRemaining(RhythmTime{2'000'000});
        Require(session.Statistics().combo == 0 && session.Statistics().judgements[5] == 3,
                "Retirement applies remaining logical misses to live counts and combo.");
        auto result = session.TakeResult(PlayEndReason::Interrupted, RhythmTime{2'000'000});
        Require(result->Summary().maximumCombo == 3 && session.Score() == 50'000'000 &&
                    result->Summary().score == session.Score() && result->Summary().accuracy.Units() == 50'000'000,
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
        Require(collector.Statistics().combo == 0 && collector.Statistics().maximumCombo == 1,
                "A later rejection must break the combo after an earlier completion in the same batch.");
        collector.ResetJudgements();
        Require(collector.Statistics().combo == 0 && collector.Statistics().maximumCombo == 0 &&
                    collector.Statistics().poorCount == 0, "Reset removes all HUD statistics.");
        hit.judgement.grade = JudgementGrade::Bad;
        batch.events = {hit, completed};
        collector.Consume(batch, true);
        Require(collector.Statistics().combo == 0, "A terminal event cannot turn a BAD hit into a successful combo.");
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
        static_cast<void>(press('F', -1'000'000)); // Empty input is retained, never POOR.
        static_cast<void>(press('D', 2'000));
        static_cast<void>(session.ProcessInput('D', InputEdge::Released, RhythmTime{3'000}));
        const auto normal = press('F', 10'000);
        static_cast<void>(session.ProcessInput('F', InputEdge::Released, RhythmTime{10'000}));
        static_cast<void>(press('F', 500'000));
        const auto big = press('J', 510'000);
        static_cast<void>(press('F', 1'000'000));
        const auto roll = session.Update(RhythmTime{1'500'000});
        const auto expectedSum = static_cast<std::uint64_t>(normal.finalizedAccuracies.front().Score().Units()) +
                                 big.finalizedAccuracies.front().Score().Units() +
                                 roll.finalizedAccuracies.front().Score().Units();
        auto result = session.TakeResult(PlayEndReason::Interrupted, RhythmTime{1'500'000});
        const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Taiko result type must be preserved.");
        Require(taiko->summary.totalNotes == 4 && taiko->summary.processedNotes == 3 &&
                    taiko->summary.unprocessedNotes == 1 && taiko->summary.accuracy.Units() == expectedSum / 4,
                "A long tail is not another logical note; interrupted accuracy uses the whole chart.");
        Require(taiko->judgements[0] == 1 && taiko->judgements[2] == 2 && taiko->judgements[5] == 1 &&
                    taiko->poorCount == 1 && taiko->okCount == 0,
                "Count graded inputs and failed completions once; releases and blanks are never POOR.");
        Require(taiko->timingErrors.size() == 3 && taiko->meanErrorMilliseconds &&
                    std::abs(*taiko->meanErrorMilliseconds - 20.0 / 3) < 1e-9 &&
                    std::abs(*taiko->varianceMillisecondsSquared - 200.0 / 9) < 1e-9,
                "Population variance must exclude automatic misses and free roll hits.");
        Require(taiko->groups[0].total == 2 && taiko->groups[1].total == 1 && taiko->groups[2].total == 1 &&
                    taiko->notes[0].accuracy.Units() == 0 &&
                    taiko->notes[2].accuracy.Units() == 33'333'333 && taiko->notes[3].accuracy.Units() == 0,
                "A POOR normal note scores zero; groups retain partial roll accuracy and note weights.");
        const auto &inputs = taiko->summary.inputs;
        Require(inputs.size() == 8 && inputs[0].time.count() == -1'000'000 && inputs[3].time == inputs[4].time &&
                    inputs[3].edge == InputEdge::Pressed && inputs[4].edge == InputEdge::Released &&
                    inputs[4].physicalKey == 'F' && taiko->summary.inputMapping.at('F') == inputs[3].action &&
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
            static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, {}));
            session.Reset();
            static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, {}));
            if (type == TaikoNoteType::TickRoll || type == TaikoNoteType::BigTickRoll)
                for (const std::int64_t us : {500'000, 1'000'000})
                    static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, RhythmTime{us}));
            const std::array<PhysicalKey, 1> held{'F'};
            static_cast<void>(session.Update(RhythmTime{1'000'000}, held));
            static_cast<void>(session.Update(RhythmTime{1'100'000}, held));
            auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{1'100'000});
            const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
            if (!taiko)
                throw std::runtime_error("Long Taiko result type must be preserved.");
            Require(taiko->okCount == 1 && taiko->judgements[5] == 0 && taiko->summary.processedNotes == 1 &&
                        taiko->summary.accuracy.Units() == 100'000'000,
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
        static_cast<void>(loaded.session->ProcessInput('F', InputEdge::Pressed, {}));
        auto result = loaded.session->TakeResult(PlayEndReason::Interrupted, RhythmTime{10'000});
        Require(result->Summary().unprocessedNotes == 1 && result->Summary().accuracy.Units() == 0,
                "A partially active long note remains unprocessed with zero score on interruption.");
    }
} // namespace tests
