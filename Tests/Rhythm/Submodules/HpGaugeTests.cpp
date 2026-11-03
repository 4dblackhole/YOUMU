#include "Gauge/BasicHpGauge.h"
#include "TestSupport.h"
#include <limits>

namespace tests
{
    namespace
    {
        bool Near(double actual, double expected)
        {
            return std::abs(actual - expected) < 1e-8;
        }
        BasicHpGauge GaugeFor(const PlaySession &session)
        {
            std::vector<HpNoteDefinition> notes;
            for (const auto &lane : session.Gear().Lanes())
                for (const auto &note : lane->Notes())
                    notes.push_back({note->Id(), note->Accuracy().target,
                                     note->Profile().HalfWindow(JudgementGrade::Perfect),
                                     note->Profile().HalfWindow(JudgementGrade::Great)});
            return BasicHpGauge(session.HealthLevel(), notes);
        }
        void Bind(PlaySession &session, IHpGauge &gauge)
        {
            const std::array<IHpGauge *, 1> gauges{&gauge};
            session.SetHpGauges(gauges);
        }
        NoteEvent Event(NoteId id, NoteEventType type, JudgementGrade grade = JudgementGrade::Max,
                        std::int64_t error = 0, std::size_t hit = 0, std::size_t tick = 0)
        {
            return {.noteId = id,
                    .type = type,
                    .judgement = {grade, RhythmDuration{error}, .123},
                    .hitIndex = hit,
                    .tickIndex = tick};
        }
        void Feed(IHpGauge &gauge, std::initializer_list<NoteEvent> events, bool pressed = true)
        {
            NoteProcessResult result;
            result.events = events;
            gauge.Consume(result, pressed);
        }
        std::vector<HpNoteDefinition> Definitions(NoteAccuracyTarget target, std::size_t count = 2)
        {
            std::vector<HpNoteDefinition> result;
            for (std::size_t i = 0; i < count; ++i)
                result.push_back({static_cast<NoteId>(i + 1), target, RhythmDuration{9'500}, RhythmDuration{23'500}});
            return result;
        }
    } // namespace

    void TestHpFormat()
    {
        ChartParser parser;
        const std::string prefix = "[Difficulty]\nMode: Taiko\nJudgeLevel: 50\n";
        const auto legacy = parser.ParsePattern(prefix, "legacy.ymp");
        Require(legacy.Succeeded() && legacy.document.hpLevel == HpLevel{9}, "Missing HP defaults to 9.00.");
        for (const auto value : {"1", "9.00", "15.25", "100.00"})
        {
            const auto parsed = parser.ParsePattern(prefix + "HP: " + value + "\n", "hp.ymp");
            Require(parsed.Succeeded(), "HP accepts the JudgeLevel numeric range and precision.");
            const auto text = ChartEditor::WritePattern(parsed.document);
            Require(text.find("JudgeLevel: 50.00\nHP: " + parsed.document.hpLevel.Format() + "\n") != std::string::npos,
                    "HP is written immediately below JudgeLevel, with two decimal places.");
            const auto again = parser.ParsePattern(text, "again.ymp");
            Require(again.Succeeded() && again.document.hpLevel == parsed.document.hpLevel, "HP must round trip.");
        }
        for (const auto value : {"0", "100.01", "9.001", "NaN", "inf", "-1", "9x", ""})
            Require(!parser.ParsePattern(prefix + "HP: " + value + "\n", "invalid.ymp").Succeeded(),
                    "Invalid HP must diagnose the chart.");
        Require(!HpLevel::IsValid(std::numeric_limits<double>::infinity()), "Infinite HP is invalid.");
        ChartEditor editor(legacy.document);
        auto changed = editor.Pattern();
        changed.hpLevel = 17.25;
        editor.Replace(changed, {});
        editor.Undo();
        Require(editor.Pattern().hpLevel == HpLevel{9}, "Metadata undo must restore HP.");
        editor.Redo();
        Require(editor.Pattern().hpLevel == HpLevel{17.25}, "Metadata redo must restore HP.");
    }

    void TestHpTimingRecovery()
    {
        for (const std::size_t count : {14U, 22U, 1000U})
        {
            BasicHpGauge boundary(9, Definitions({NoteAccuracyKind::TimingHits, 1, 0}, count));
            for (NoteId id = 1; id <= count / 2; ++id)
                Feed(boundary, {Event(id, NoteEventType::HitAccepted), Event(id, NoteEventType::Completed)});
            Require(Near(boundary.State().percent, 60) && boundary.State().passed,
                    "Exact 60% must pass after fractional recovery accumulation.");
        }
        for (const auto error : {-16'500, 16'500})
        {
            BasicHpGauge gauge(9, Definitions({NoteAccuracyKind::TimingHits, 1, 0}));
            Feed(gauge, {Event(1, NoteEventType::HitAccepted, JudgementGrade::Great, error),
                         Event(1, NoteEventType::Completed)});
            Require(Near(gauge.State().percent, 30),
                    "GREAT uses actual boundaries, symmetrically, independent of scoreRate.");
        }
        for (const auto [grade, expected] :
             std::array<std::pair<JudgementGrade, double>, 4>{{{JudgementGrade::Max, 60},
                                                               {JudgementGrade::Perfect, 60},
                                                               {JudgementGrade::Good, 0},
                                                               {JudgementGrade::Bad, 0}}})
        {
            BasicHpGauge gauge(9, Definitions({NoteAccuracyKind::TimingHits, 1, 0}));
            Feed(gauge, {Event(1, NoteEventType::HitAccepted, grade), Event(1, NoteEventType::Completed)});
            Require(Near(gauge.State().percent, expected),
                    "MAX/PERFECT/GOOD/BAD recovery must follow the HP contract.");
        }
        BasicHpGauge gauge(1.25, Definitions({NoteAccuracyKind::TimingHits, 1, 0}, 4));
        for (NoteId id : {1U, 2U, 3U})
            Feed(gauge, {Event(id, NoteEventType::HitAccepted), Event(id, NoteEventType::Completed)});
        Feed(gauge, {Event(4, NoteEventType::Missed, JudgementGrade::Miss)});
        Require(Near(gauge.State().percent, 52.5) && !gauge.State().passed,
                "MISS uses the chart HP multiplier; ending below 60 fails.");
        gauge.Reset();
        Require(gauge.State().applicable && Near(gauge.State().percent, 0),
                "Retry starts at zero and remains eligible.");
    }

    void TestHpCompositeSettlements()
    {
        BasicHpGauge pair(1, Definitions({NoteAccuracyKind::TimingHits, 2, 0}));
        Feed(pair, {Event(1, NoteEventType::HitAccepted)});
        Require(Near(pair.State().percent, 0), "A big note waits until completion.");
        Feed(pair,
             {Event(1, NoteEventType::HitAccepted, JudgementGrade::Good, 0, 1), Event(1, NoteEventType::Completed)});
        Require(Near(pair.State().percent, 30), "Each timing hit has half the logical weight.");
        Feed(pair, {Event(2, NoteEventType::InputRejected, JudgementGrade::Perfect)});
        Require(Near(pair.State().percent, 30), "POOR cannot decrease HP before final settlement.");
        Feed(pair,
             {Event(2, NoteEventType::HitAccepted), Event(2, NoteEventType::HitAccepted, JudgementGrade::Max, 0, 1),
              Event(2, NoteEventType::Completed)});
        Require(Near(pair.State().percent, 0), "POOR overrides the whole timing pair with one MISS penalty.");

        BasicHpGauge count(9, Definitions({NoteAccuracyKind::HitCount, 4, 0}));
        Feed(count, {Event(1, NoteEventType::TickAccepted), Event(1, NoteEventType::TickAccepted)});
        Require(Near(count.State().percent, 0), "Count-only rolls defer HP until completion.");
        Feed(count, {Event(1, NoteEventType::Missed)});
        Require(Near(count.State().percent, 30),
                "An incomplete count roll recovers its accepted fraction without extra penalty.");
        for (int i = 0; i < 8; ++i)
            Feed(count, {Event(2, NoteEventType::TickAccepted)});
        Feed(count, {Event(2, NoteEventType::Completed), Event(2, NoteEventType::Completed)});
        Require(Near(count.State().percent, 90), "Excess counts and repeated completion cannot add weight.");

        BasicHpGauge ticks(9, Definitions({NoteAccuracyKind::Ticks, 0, 4}));
        Feed(ticks,
             {Event(1, NoteEventType::TickAccepted), Event(1, NoteEventType::TickAccepted),
              Event(1, NoteEventType::TickMissed, JudgementGrade::Miss, 0, 0, 1),
              Event(1, NoteEventType::TickRejected, JudgementGrade::Miss, 0, 0, 2),
              Event(1, NoteEventType::TickAccepted, JudgementGrade::Max, 0, 0, 3), Event(1, NoteEventType::Completed)});
        Require(Near(ticks.State().percent, 30),
                "Ticks settle individually exactly once; rejected/missed ticks do not drain.");

        BasicHpGauge hold(1, Definitions({NoteAccuracyKind::Hold, 1, 2}));
        Feed(hold, {Event(1, NoteEventType::HitAccepted, JudgementGrade::Great, 16'500)});
        Feed(hold, {Event(1, NoteEventType::InputRejected, JudgementGrade::Max), Event(1, NoteEventType::TickAccepted),
                    Event(1, NoteEventType::TickRejected, JudgementGrade::Miss, 0, 0, 1),
                    Event(1, NoteEventType::Completed)});
        Require(Near(hold.State().percent, 30), "Buzz body POOR must preserve the settled head and other ticks.");
        Feed(hold,
             {Event(2, NoteEventType::InputRejected), Event(2, NoteEventType::HitAccepted),
              Event(2, NoteEventType::TickAccepted),
              Event(2, NoteEventType::TickAccepted, JudgementGrade::Max, 0, 0, 1), Event(2, NoteEventType::Completed)});
        Require(Near(hold.State().percent, 30), "Buzz head POOR drains only head weight, retaining body recovery.");
        BasicHpGauge headOnly(1, Definitions({NoteAccuracyKind::Hold, 1, 0}));
        Feed(headOnly, {Event(1, NoteEventType::HitAccepted), Event(1, NoteEventType::Completed)});
        Require(Near(headOnly.State().percent, 60), "A tickless Buzz assigns all weight to its head.");
    }

    void TestHpSessionReplayAndClear()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.hpLevel = 1;
        for (std::int64_t i = 0; i < 4; ++i)
            pattern.notes.push_back({.position = {0, {i, 4}}, .keyType = 1});
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Require(loaded.Succeeded(), "HP replay session must load.");
        auto &session = *loaded.session;
        auto gauge = GaugeFor(session);
        Bind(session, gauge);
        for (const auto us : {0, 500'000, 1'000'000, 1'500'000})
            static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, RhythmTime{us}));
        Require(Near(gauge.State().percent, 100), "HP clamps to 100.");
        session.RewindJudgements(RhythmTime{1'500'000});
        Require(Near(gauge.State().percent, 90), "Rewind replays earlier changes, recovering discarded saturation.");
        session.RecordControl({PlayControlType::Resume, RhythmTime{1'500'000}, RhythmTime{1'000'000}});
        Require(Near(gauge.State().percent, 90), "Resume preserves HP.");
        static_cast<void>(session.Update(RhythmTime{2'000'000}));
        Require(Near(gauge.State().percent, 60) && gauge.State().passed, "MISS drains at final judgement; 60 passes.");
        auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{2'000'000});
        Require(result->Summary().IsCleared() && Near(result->Summary().hpGauges.front().percent, 60),
                "The result includes the final judgement before clear evaluation.");
        session.Reset();
        Require(Near(gauge.State().percent, 0), "Retry resets attached HP.");
    }

    void TestHpLongRuntime()
    {
        for (const auto type :
             {TaikoNoteType::Roll, TaikoNoteType::BigRoll, TaikoNoteType::Balloon, TaikoNoteType::DengDeng,
              TaikoNoteType::TickRoll, TaikoNoteType::BigTickRoll, TaikoNoteType::Buzz})
        {
            auto loaded =
                TaikoMode{}.CreateSession(MakeLongPattern(type, {"HitCount=4", "TickDivision=8", "Action=Don"}));
            Require(loaded.Succeeded(), "HP long note must load.");
            auto &session = *loaded.session;
            auto gauge = GaugeFor(session);
            Bind(session, gauge);
            Require(session.PlayableNoteCount() == 1, "A long note counts as one HP unit.");
            static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, {}));
            static_cast<void>(session.Update(RhythmTime{1'000'000}));
            Require(gauge.State().percent > 0 && gauge.State().percent <= 100,
                    "Partial count/tick/Buzz recovery survives completion without non-timing MISS drain.");
            session.RewindJudgements({});
            Require(Near(gauge.State().percent, 0), "Rewind before a long head restores zero HP.");
            session.SetHpGauges({});
        }
        auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::Buzz, {"TickDivision=8", "Action=Don"}));
        auto &session = *loaded.session;
        auto gauge = GaugeFor(session);
        Bind(session, gauge);
        static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, {}));
        const auto head = gauge.State().percent;
        session.RetireRemaining(RhythmTime{100'000});
        Require(Near(gauge.State().percent, head), "Retirement preserves a settled Buzz head.");
    }

    void TestEmptyHpEligibility()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.notes.push_back({.position = {}, .keyType = 11, .actionType = 1}); // Unmatched head is ignored.
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Require(loaded.Succeeded(), "Editor documents may contain no effective playable notes.");
        auto &session = *loaded.session;
        auto gauge = GaugeFor(session);
        Bind(session, gauge);
        Require(!session.CanEnterGameplay() && !session.CanUseHp() && !gauge.State().applicable &&
                    !gauge.State().passed,
                "An effectively empty chart is editor-only and excluded from HP.");
        static_cast<void>(session.Update(RhythmTime{1'000'000}));
        auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{1'000'000});
        Require(!result->Summary().CanUseHp() && !result->Summary().IsCleared(),
                "Empty tests cannot become HP clears.");
    }
} // namespace tests
