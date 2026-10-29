#include "Note/Submodules/CountedHitInputRule.h"
#include "Note/Submodules/HoldInputRule.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "TestSupport.h"

namespace tests
{
    void TestPoorLargeNoteAccuracy()
    {
        for (const int type : {3, 4, 5})
        {
            PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.notes.push_back(PatternNote{.keyType = type});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Large POOR fixture must load.");
            auto &session = *loaded.session;
            if (type == 5)
            {
                static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, {}));
                static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, RhythmTime{1}));
                static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{2}));
            }
            else
            {
                static_cast<void>(session.ProcessInput(type == 3 ? 'D' : 'F', InputEdge::Pressed, RhythmTime{-1'000}));
                static_cast<void>(session.ProcessInput(type == 3 ? 'F' : 'D', InputEdge::Pressed, {}));
                static_cast<void>(session.ProcessInput(type == 3 ? 'J' : 'K', InputEdge::Pressed, RhythmTime{1}));
            }
            Require(session.AccuracyRate() == 0.0, "A POOR poisons the entire large or purple note.");
            session.Reset();
            static_cast<void>(session.ProcessInput(type == 4 ? 'D' : 'F', InputEdge::Pressed, {}));
            static_cast<void>(session.ProcessInput(type == 4 ? 'K' : type == 5 ? 'D' : 'J',
                                                  InputEdge::Pressed, RhythmTime{1}));
            Require(session.AccuracyRate() == 1.0, "Retry must clear the large-note POOR state.");
        }

    }

    void TestBuzzTickColorFocus()
    {
        for (const bool kat : {false, true})
        {
            auto pattern = MakeLongPattern(TaikoNoteType::Buzz,
                {kat ? "Action=Kat" : "Action=Don", "TickDivision=16"});
            pattern.notes.back().position.fraction = Rational{9, 16};
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Buzz POOR fixture must load.");
            auto &session = *loaded.session;
            const PhysicalKey correct = kat ? 'D' : 'F', wrong = kat ? 'F' : 'D';
            const std::array held{correct};
            const std::array both{correct, wrong};
            NoteProcessResult log;
            log.Append(session.ProcessInput(correct, InputEdge::Pressed, {}));
            const auto afterHead = session.ProcessInput(wrong, InputEdge::Pressed, RhythmTime{40'000});
            Require(!HasEvent(afterHead, NoteEventType::InputRejected),
                    "An already accepted Buzz head must not reject or regrade later inputs.");
            log.Append(afterHead);
            log.Append(session.ProcessInput(wrong, InputEdge::Released, RhythmTime{41'000}));
            log.Append(session.Update(RhythmTime{375'000}, held));
            log.Append(session.ProcessInput(wrong, InputEdge::Pressed, RhythmTime{500'000}));
            log.Append(session.Update(RhythmTime{750'000}, both));
            log.Append(session.ProcessInput(wrong, InputEdge::Released, RhythmTime{750'001}));
            log.Append(session.Update(RhythmTime{1'125'000}, held));
            Require(log.finalizedAccuracies.size() == 1 &&
                        log.finalizedAccuracies.front().acceptedTicks == 6 &&
                        log.finalizedAccuracies.front().target.ticks == 9 &&
                        log.finalizedAccuracies.front().hitScoreRates.front() == 1.0 &&
                        std::ranges::count(log.events, NoteEventType::TickRejected, &NoteEvent::type) == 3 &&
                        session.AccuracyRate() == AccuracyScore::FromRate(0.5 + 0.5 * (6.0 / 9)).Rate(),
                    "Wrong-color holds across ticks 4..6 must preserve MAX head and score six of nine ticks.");
            session.RewindJudgements(RhythmTime{625'001});
            static_cast<void>(session.ProcessInput(wrong, InputEdge::Released, RhythmTime{750'001}));
            static_cast<void>(session.Update(RhythmTime{1'125'000}, held));
            Require(session.AccuracyRate() == AccuracyScore::FromRate(0.5 + 0.5 * (6.0 / 9)).Rate(),
                    "Rewind must reconstruct the wrong-color held state and per-tick POORs.");
            auto result = session.TakeResult(PlayEndReason::Completed, RhythmTime{1'125'000});
            const auto *taiko = dynamic_cast<const TaikoPlayResult *>(result.get());
            Require(taiko && taiko->poorCount == 3 && taiko->missedTicks == 3 &&
                        taiko->acceptedTicks == 6 && taiko->timingErrors.size() == 1,
                    "Tick POOR counts must not invent additional head timing samples.");

            auto headPoor = TaikoMode{}.CreateSession(pattern);
            Require(headPoor.Succeeded(), "Buzz head POOR fixture must load.");
            static_cast<void>(headPoor.session->ProcessInput(wrong, InputEdge::Pressed, RhythmTime{-1'000}));
            static_cast<void>(headPoor.session->ProcessInput(wrong, InputEdge::Released, RhythmTime{-500}));
            static_cast<void>(headPoor.session->ProcessInput(correct, InputEdge::Pressed, {}));
            static_cast<void>(headPoor.session->Update(RhythmTime{1'125'000}, held));
            Require(headPoor.session->AccuracyRate() == 0.5,
                    "A POOR head scores zero while successful body ticks keep their half weight.");
        }
    }

    void TestPoorSingleNoteAccuracy()
    {
        auto profile = std::make_shared<AccuracyRange>();
        PlaySession session;
        session.SetInputMapping({{'F', 1}, {'D', 2}});
        auto &lane = session.Gear().CreateLane();
        lane.AddNote(MakeTap(1, RhythmTime{0}, profile));
        session.Gear().Finalize();

        const auto rejected = session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-10'000});
        Require(HasEvent(rejected, NoteEventType::InputRejected) && rejected.finalizedAccuracies.empty(),
                "A POOR must leave the note available for a subsequent correct input.");
        const auto accepted = session.ProcessInput('F', InputEdge::Pressed, RhythmTime{0});
        Require(HasEvent(accepted, NoteEventType::HitAccepted) &&
                    accepted.finalizedAccuracies.size() == 1 &&
                    accepted.finalizedAccuracies.front().Score().Units() == 0 &&
                    session.AccuracyRate() == 0.0,
                "A POOR followed by a MAX must finalize the logical note with zero accuracy.");

        session.Reset();
        static_cast<void>(session.ProcessInput('D', InputEdge::Released, RhythmTime{-10'000}));
        static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-100'000}));
        const auto clean = session.ProcessInput('F', InputEdge::Pressed, RhythmTime{0});
        Require(clean.finalizedAccuracies.size() == 1 && clean.finalizedAccuracies.front().Score().Rate() == 1.0,
                "Reset, releases, and out-of-range inputs must not poison a clean note.");

        session.Reset();
        static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-10'000}));
        session.RewindJudgements(RhythmTime{-15'000});
        const auto rewound = session.ProcessInput('F', InputEdge::Pressed, RhythmTime{0});
        Require(rewound.finalizedAccuracies.size() == 1 && rewound.finalizedAccuracies.front().Score().Rate() == 1.0,
                "Rewind must remove the POOR penalty when it removes the rejected input.");
    }

    void TestTimingAccuracyAndSessionAggregation()
    {
        auto profile = std::make_shared<AccuracyRange>();
        PlaySession session;
        session.SetInputMapping({{'F', 1}, {'J', 1}});
        auto &lane = session.Gear().CreateLane();
        lane.AddNote(std::make_unique<RuleBasedNote>(
            1, RhythmTime{0}, profile,
            std::make_unique<CountedHitInputRule>(1, 2)));
        lane.AddNote(MakeTap(2, RhythmTime{200'000}, profile));
        session.Gear().Finalize();
        Require(!session.AccuracyRate(), "Accuracy must be unavailable before a final note.");
        const auto first =
            session.ProcessInput('F', InputEdge::Pressed, RhythmTime{0});
        Require(first.finalizedAccuracies.empty(), "Big first hit must not finalize accuracy.");
        const auto second =
            session.ProcessInput('J', InputEdge::Pressed, RhythmTime{16'500});
        const double hit2 = profile->Evaluate({}, RhythmTime{16'500}).scoreRate;
        const double bigRate = AccuracyScore::FromRate((1.0 + hit2) * 0.5).Rate();
        const auto bigAccuracyRate = session.AccuracyRate();
        const auto &bigAccuracy = session.LastNoteAccuracy();
        Require(second.finalizedAccuracies.size() == 1 &&
                    bigAccuracyRate && bigAccuracy && std::abs(*bigAccuracyRate - bigRate) < 1e-9 &&
                    bigAccuracy->hitScoreRates == std::vector<double>{1.0, hit2},
                "Big accuracy must preserve both interpolated hits and their mean.");
        static_cast<void>(session.Update(RhythmTime{400'000}));
        static_cast<void>(session.Update(RhythmTime{500'000}));
        const auto missedAccuracyRate = session.AccuracyRate();
        Require(session.FinalizedNoteCount() == 2 &&
                    missedAccuracyRate && std::abs(*missedAccuracyRate - AccuracyScore::FromRate(bigRate * 0.5).Rate()) < 1e-9,
                "Each logical note, including a miss, must contribute exactly once.");
        session.Reset();
        Require(!session.AccuracyRate() && !session.LastNoteAccuracy(),
                "Reset must clear totals and the last-note details.");
        static_cast<void>(session.ProcessInput('F', InputEdge::Pressed, {}));
        const auto partial = session.Update(RhythmTime{60'000});
        Require(partial.finalizedAccuracies.size() == 1 &&
                    partial.finalizedAccuracies.front().ScoreRate() == 0.5 &&
                    partial.finalizedAccuracies.front().hitScoreRates[1] == 0.0,
                "An unfilled second hit must count as zero, not discard the first hit.");
#if defined(_DEBUG)
        const auto &partialAccuracy = session.LastNoteAccuracy();
        Require(partialAccuracy && partialAccuracy->DebugText().find(L"Hit2=0.000000%") != std::wstring::npos,
                "Debug details must expose unfilled hit components.");
#endif
    }

    void TestPurpleNoteBothOrders()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        pattern.notes.push_back(
            PatternNote{.position = {0, Rational{0, 1}},
                               .keyType = 5,
                               .actionType = static_cast<int>(TaikoPatternAction::Down)});
        for (const bool katFirst : {false, true})
        {
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded() && loaded.session->FindNotePresentation(1)->visualKind ==
                                              NoteVisualKind::Purple,
                    "ID 5 must create a purple large-note presentation.");
            const auto firstKey = katFirst ? 'D' : 'F';
            const auto secondKey = katFirst ? 'F' : 'D';
            const auto first =
                loaded.session->ProcessInput(firstKey, InputEdge::Pressed, {});
            const auto repeat = loaded.session->ProcessInput(firstKey, InputEdge::Pressed,
                                                             RhythmTime{1'000});
            const auto second = loaded.session->ProcessInput(secondKey, InputEdge::Pressed,
                                                             RhythmTime{2'000});
            Require(first.audioCues.size() == 1 &&
                        first.audioCues.front().sound ==
                            (katFirst ? "Taiko.Kat.Hit" : "Taiko.Don.Hit") &&
                        !HasEvent(repeat, NoteEventType::HitAccepted) &&
                        HasEvent(second, NoteEventType::Completed) &&
                        loaded.session->AccuracyRate() == 0.0,
                    "Purple accepts either order but a rejected repeated color leaves zero final accuracy.");
        }
    }

    void TestCountedLongNoteAccuracyAndSyntax()
    {
        for (const auto type : {TaikoNoteType::Roll, TaikoNoteType::BigRoll,
                                TaikoNoteType::Balloon, TaikoNoteType::DengDeng})
        {
            for (const std::string count : {"10", "HitCount=10"})
            {
                const std::string pattern =
                    "[Metadata]\nBase BPM: 120\n"
                    "[Difficulty]\nMode: Taiko\n[Time Signature]\n[Pattern]\n"
                    "0/1," +
                    std::to_string(static_cast<int>(type)) + ",1,," + count + "\n1/2," +
                    std::to_string(static_cast<int>(type)) + ",2\n";
                const auto parsed = ChartParser{}.ParsePattern(pattern, "count.ymp");
                auto loaded = TaikoMode{}.CreateSession(parsed.document);
                Require(parsed.Succeeded() && loaded.Succeeded(),
                        "All counted long notes must parse bare and named ExtraData counts.");
                for (int hit = 0; hit < 8; ++hit)
                {
                    const auto key =
                        type == TaikoNoteType::DengDeng && hit % 2 != 0 ? 'D' : 'F';
                    static_cast<void>(loaded.session->ProcessInput(
                        key, InputEdge::Pressed, RhythmTime{hit * 10'000}));
                }
                const auto expired = loaded.session->Update(RhythmTime{1'000'000});
                const auto finalRate = loaded.session->AccuracyRate();
                const auto &finalAccuracy = loaded.session->LastNoteAccuracy();
                Require(HasEvent(expired, NoteEventType::Missed) &&
                            finalRate && finalAccuracy && std::abs(*finalRate - 0.8) < 1e-9 &&
                            finalAccuracy->acceptedHits == 8 && finalAccuracy->target.hits == 10,
                        "Eight of ten hits must finalize at 80 percent, even on timeout.");
            }
            auto pattern = MakeLongPattern(type);
            pattern.notes.back().position.fraction = Rational{1, 7};
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded() && loaded.session->Gear()
                                                  .Lanes()
                                                  .front()
                                                  ->Notes()
                                                  .front()
                                                  ->Accuracy()
                                                  .target.hits == 2,
                    "All counted defaults must use exact ceil(1/7 * 12) = 2.");
            const auto invalid = TaikoMode{}.CreateSession(MakeLongPattern(type, {"0"}));
            Require(!invalid.Succeeded(), "A nonpositive explicit target must be rejected.");
        }
        auto roll =
            TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::Roll, {"2"}));
        for (int hit = 0; hit < 5; ++hit)
        {
            static_cast<void>(roll.session->ProcessInput('F', InputEdge::Pressed,
                                                         RhythmTime{hit * 1'000}));
        }
        static_cast<void>(roll.session->Update(RhythmTime{1'000'000}));
        const auto &rollAccuracy = roll.session->LastNoteAccuracy();
        Require(roll.session->AccuracyRate() == 1.0 &&
                    rollAccuracy && rollAccuracy->acceptedHits == 5,
                "Roll may keep accepting hits, but its final accuracy must cap at 100 percent.");
    }

    void TestTickAndHoldAccuracy()
    {
        auto tickRoll =
            TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::TickRoll));
        const auto early = tickRoll.session->ProcessInput('F', InputEdge::Pressed,
                                                          RhythmTime{-70'000});
        Require(std::ranges::none_of(early.events,
                                     [](const auto &event) {
                                         return event.judgement.grade ==
                                                JudgementGrade::Bad;
                                     }),
                "TickRoll must not emit a head BAD judgement.");
        for (const int time : {-54'500, 70'500})
        {
            const auto hit = tickRoll.session->ProcessInput('F', InputEdge::Pressed,
                                                            RhythmTime{time});
            Require(std::ranges::any_of(hit.events,
                                        [](const auto &event) {
                                            return event.type ==
                                                       NoteEventType::TickAccepted &&
                                                   event.judgement.grade ==
                                                       JudgementGrade::Unjudged &&
                                                   event.judgement.scoreRate == 1.0;
                                        }),
                    "Each TickRoll tick, including head, is binary within inclusive Good.");
        }
        static_cast<void>(tickRoll.session->Update(RhythmTime{1'000'000}));
        Require(tickRoll.session->AccuracyRate() == AccuracyScore::FromUnits(22'222'222).Rate(),
                "Two accepted ticks of nine must score 2/9, including the missed tail.");

        auto profile = std::make_shared<AccuracyRange>();
        std::vector<RhythmTime> ticks;
        for (int tick = 1; tick <= 100; ++tick)
        {
            ticks.emplace_back(tick * 100'000);
        }
        RuleBasedNote hold(
            1, {}, profile,
            std::make_unique<HoldInputRule>(1, RhythmTime{10'100'000}, ticks));
        // Default interpolation: 14,166 us is approximately 96 percent.
        static_cast<void>(
            hold.ProcessInput({RhythmTime{14'166}, 1, 'F', InputEdge::Pressed}));
        static_cast<void>(hold.ProcessInput(
            {RhythmTime{9'850'000}, 1, 'F', InputEdge::Released}));
        const auto done = hold.Update({RhythmTime{10'100'000}, {}});
        const double head = profile->Evaluate({}, RhythmTime{14'166}).scoreRate;
        Require(done.finalizedAccuracies.size() == 1 && hold.Accuracy().acceptedTicks == 98 &&
                    std::abs(hold.Accuracy().ScoreRate() - AccuracyScore::FromRate((head + 0.98) * 0.5).Rate()) < 1e-9 &&
                    std::abs(hold.Accuracy().ScoreRate() - 0.97) < 0.00001,
                "Buzz must combine interpolated head and 98/100 held ticks equally.");
        Require(hold.Update({RhythmTime{11'000'000}, {}}).finalizedAccuracies.empty(),
                "An ended hold must not emit its accuracy twice.");
        hold.Reset();
        static_cast<void>(hold.ProcessInput({{}, 1, 'F', InputEdge::Pressed}));
        static_cast<void>(
            hold.ProcessInput({RhythmTime{1}, 1, 'J', InputEdge::Pressed}));
        static_cast<void>(
            hold.ProcessInput({RhythmTime{2}, 1, 'F', InputEdge::Released}));
        static_cast<void>(
            hold.ProcessInput({RhythmTime{150'000}, 1, 'J', InputEdge::Released}));
        Require(hold.Accuracy().acceptedTicks == 1,
                "Releasing one physical key must not cancel another held key of the same action.");

        auto buzz = TaikoMode{}.CreateSession(
            MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"}));
        Require(buzz.session->FindNotePresentation(1)->tickTimes.size() == 8 &&
                    buzz.session->FindNotePresentation(1)->tickTimes.front() == RhythmTime{125'000},
                "Buzz body ticks must exclude the independently judged head.");
    }
} // namespace tests
