#include "Note/Submodules/HoldInputRule.h"
#include "Note/Submodules/TimedSequenceInputRule.h"
#include "TestSupport.h"

namespace tests
{
    void TestJudgementScalingAndInterpolation()
    {
        AccuracyRange profile("default", 50);
        Require(profile.HalfWindow(JudgementGrade::Max).count() == 4'500,
                "Level 50 Max window must be 4.5 ms.");
        profile.SetLevel(100);
        Require(profile.HalfWindow(JudgementGrade::Max).count() == 2'250 &&
                    profile.HalfWindow(JudgementGrade::Perfect).count() == 4'750,
                "Level 100 windows must be exactly half of level 50.");
        profile.SetLevel(50.25);
        Require(profile.Level() == 50.25 && profile.HalfWindow(JudgementGrade::Bad).count() == 89'552,
                "Fractional JudgeLevel must reach actual judgement window scaling without integer truncation.");

        const std::array<JudgementBand, AccuracyRange::BandCount> bands{{
            {JudgementGrade::Max, RhythmDuration{5'000}, 1.0},
            {JudgementGrade::Perfect, RhythmDuration{9'000}, 0.9},
            {JudgementGrade::Great, RhythmDuration{20'000}, 0.8},
            {JudgementGrade::Good, RhythmDuration{50'000}, 0.5},
            {JudgementGrade::Bad, RhythmDuration{90'000}, 0.0},
        }};
        AccuracyRange interpolation("linear", 50, bands);
        const JudgementResult result =
            interpolation.Evaluate(RhythmTime::zero(), RhythmTime{7'000});
        Require(result.grade == JudgementGrade::Perfect &&
                    std::abs(result.scoreRate - 0.95) < 1e-9,
                "A 7 ms hit between 5/9 ms anchors must score 95 percent.");
    }

    void TestRhythmTimerClockMapping()
    {
        RhythmTimer timer;
        timer.Start(1'000, 1'000, RhythmTime{250'000});
        Require(timer.Now(1'500) == RhythmTime{750'000},
                "QPC mapping must advance the single rhythm timeline.");
        timer.AnchorDspClock(RhythmTime{750'000}, 48'000, 48'000);
        Require(timer.ToDspClock(RhythmTime{1'750'000}) == 96'000,
                "One second of rhythm time must map to one mixer sample-rate span.");
    }

    void TestLaneFocusRules()
    {
        auto profile = std::make_shared<AccuracyRange>();
        Lane lane;
        lane.AddNote(MakeTap(1, RhythmTime::zero(), profile));
        lane.AddNote(MakeTap(2, RhythmTime{100'000}, profile));
        lane.Finalize();

        const NoteProcessResult early =
            lane.ProcessInput({RhythmTime{-70'000}, 1, 'F', InputEdge::Pressed});
        Require(lane.CurrentIndex() == 0 && HasEvent(early, NoteEventType::InputRejected),
                "Early Bad must keep focus on the first note.");

        const auto automatic = lane.Update(RhythmTime{60'000});
        Require(HasEvent(automatic, NoteEventType::Missed) && lane.CurrentIndex() == 1 &&
                    !HasEvent(automatic, NoteEventType::HitAccepted),
                "Without input, a Good candidate must immediately retire the late-Bad focus.");
        Require(!HasEvent(lane.Update(RhythmTime{60'000}), NoteEventType::Missed),
                "The automatic focus transition must emit MISS only once.");
        lane.Reset();
        const NoteProcessResult forwarded =
            lane.ProcessInput({RhythmTime{60'000}, 1, 'F', InputEdge::Pressed});
        Require(HasEvent(forwarded, NoteEventType::Missed) &&
                    std::ranges::count(forwarded.events, NoteEventType::HitAccepted, &NoteEvent::type) == 1 &&
                    lane.CurrentIndex() == 2,
                "An input must preserve late-Bad protection: miss the old note and hit only the Good next note.");

        Lane isolated;
        isolated.AddNote(MakeTap(1, {}, profile));
        isolated.Finalize();
        const auto expiry = isolated.CurrentNote()->ExpireTime();
        Require(!HasEvent(isolated.Update(expiry), NoteEventType::Missed) && isolated.CurrentIndex() == 0,
                "Without a Good candidate, the current focus must keep its full closed window.");
        Require(HasEvent(isolated.Update(expiry + RhythmDuration{1}), NoteEventType::Missed) &&
                    isolated.CurrentNote() == nullptr,
                "An isolated note must become MISS when its full window expires.");
        Require(!HasEvent(isolated.Update(expiry + RhythmDuration{2}), NoteEventType::Missed),
                "An already retired focus must not produce a repeated MISS.");

        // Focus selection is time-based and must precede the key's colour check.
        Lane colour;
        colour.AddNote(MakeTap(1, {}, profile));
        colour.AddNote(std::make_unique<RuleBasedNote>(2, RhythmTime{100'000}, profile,
                                                      std::make_unique<TapInputRule>(2)));
        colour.Finalize();
        const auto wrongColour = colour.ProcessInput({RhythmTime{60'000}, 1, 'F', InputEdge::Pressed});
        Require(HasEvent(wrongColour, NoteEventType::Missed) && colour.CurrentIndex() == 1 &&
                    HasEvent(wrongColour, NoteEventType::InputRejected) &&
                    !HasEvent(wrongColour, NoteEventType::HitAccepted),
                "The Good next note must receive even a wrong-colour edge after automatic focusing.");

        Lane dense;
        for (const auto timing : {0, 5'000, 100'000})
            dense.AddNote(MakeTap(static_cast<NoteId>(dense.Notes().size() + 1), RhythmTime{timing}, profile));
        dense.Finalize();
        const auto skipped = dense.Update(RhythmTime{89'000});
        Require(dense.CurrentIndex() == 2 &&
                    std::ranges::count(skipped.events, NoteEventType::Missed, &NoteEvent::type) == 2,
                "RPG's first Good candidate must skip every intervening late-Bad note.");

        Lane boundary;
        boundary.AddNote(MakeTap(1, {}, profile));
        boundary.AddNote(MakeTap(2, RhythmTime{120'000}, profile));
        boundary.Finalize();
        const auto opens = RhythmTime{120'000} - profile->HalfWindow(JudgementGrade::Good);
        Require(!HasEvent(boundary.Update(opens - RhythmDuration{1}), NoteEventType::Missed),
                "The focus must remain one microsecond before the next Good window opens.");
        Require(HasEvent(boundary.Update(opens), NoteEventType::Missed) && boundary.CurrentIndex() == 1,
                "The next note's exact Good boundary must trigger automatic focusing.");

        auto extended = std::make_shared<AccuracyRange>("sync", JudgeLevel{50},
            AccuracyRange::DefaultLevel50Bands(), RhythmDuration{10'000});
        Lane mixed;
        mixed.AddNote(MakeTap(1, {}, profile));
        mixed.AddNote(MakeTap(2, RhythmTime{120'000}, extended));
        mixed.Finalize();
        Require(HasEvent(mixed.Update(RhythmTime{55'500}), NoteEventType::Missed) && mixed.CurrentIndex() == 1,
                "Good candidate selection must include the candidate's own Syncopation extension.");

        for (const auto spacing : {0, 10'000, 50'000})
        {
            Lane overlap;
            overlap.AddNote(MakeTap(1, {}, profile));
            overlap.AddNote(MakeTap(2, RhythmTime{spacing}, profile));
            overlap.Finalize();
            const auto first = overlap.ProcessInput({{}, 1, 'F', InputEdge::Pressed});
            Require(std::ranges::count(first.events, NoteEventType::HitAccepted, &NoteEvent::type) == 1 &&
                        std::ranges::count(first.events, NoteEventType::Completed, &NoteEvent::type) == 1 &&
                        !HasEvent(first, NoteEventType::Missed) && overlap.CurrentIndex() == 1 &&
                        overlap.Notes()[1]->State() == NoteState::Pending,
                    "Overlapping judgement windows must accept only one note per pressed edge.");
            Require(!HasEvent(overlap.Update({}), NoteEventType::Missed),
                    "Advancing focus after a hit must not miss the still-open next note.");
            Require(HasEvent(overlap.ProcessInput({{}, 1, 'J', InputEdge::Pressed}), NoteEventType::HitAccepted) &&
                        overlap.CurrentIndex() == 2,
                    "The overlapping second note must wait for its own pressed edge.");
        }
    }

    void TestLargeNoteSoundState()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120.0;
        pattern.judgementLevel = 50.25;
        pattern.notes.push_back(
            PatternNote{.position = {0, Rational{0, 1}},
                               .keyType = static_cast<int>(TaikoNoteType::BigDon),
                               .actionType = static_cast<int>(TaikoPatternAction::Down)});

        TaikoMode taiko;
        ModeLoadResult loaded = taiko.CreateSession(pattern);
        Require(loaded.Succeeded(), "Taiko mode must create the large-note session.");

        const NoteProcessResult early = loaded.session->ProcessInput(
            'F', InputEdge::Pressed, RhythmTime{-60'000});
        Require(early.audioCues.size() == 1 &&
                    early.audioCues.front().sound == "Taiko.Don.FreeInput",
                "An out-of-Good large note must not play its dedicated sound.");

        const NoteProcessResult first = loaded.session->ProcessInput(
            'F', InputEdge::Pressed, RhythmTime::zero());
        Require(first.audioCues.size() == 1 &&
                    first.audioCues.front().sound == "Taiko.BigDon.FirstHit",
                "The first Good large-note hit must emit exactly one large-note cue.");

        const NoteProcessResult second = loaded.session->ProcessInput(
            'J', InputEdge::Pressed, RhythmTime{1'000});
        Require(second.audioCues.empty() && HasEvent(second, NoteEventType::Completed),
                "The second large-note hit must complete without replaying the first-hit cue.");
        const auto result = loaded.session->TakeResult(PlayEndReason::Completed, RhythmTime{1'000});
        Require(result && result->Summary().judgementLevel == 50.25,
                "Result snapshots must preserve fractional JudgeLevel.");
    }

    void TestHoldTicksAreExactlyOnce()
    {
        auto profile = std::make_shared<AccuracyRange>();
        auto sound = std::make_shared<MappedNoteSoundPolicy>();
        sound->Bind(SoundBinding{.eventType = NoteEventType::TickAccepted,
                                         .cue = AudioCueRequest{.sound = "tick"}});
        RuleBasedNote note(7, RhythmTime::zero(), profile,
                                   std::make_unique<HoldInputRule>(
                                       1, RhythmTime{30'000},
                                       std::vector<RhythmTime>{RhythmTime{10'000},
                                                                       RhythmTime{20'000}}),
                                   sound);
        static_cast<void>(
            note.ProcessInput({RhythmTime::zero(), 1, 'F', InputEdge::Pressed}));
        const std::array<NoteAction, 1> held{1};
        const NoteProcessResult first = note.Update({RhythmTime{15'000}, held});
        const NoteProcessResult repeated = note.Update({RhythmTime{15'000}, held});
        const NoteProcessResult second = note.Update({RhythmTime{25'000}, held});
        Require(first.audioCues.size() == 1 && repeated.audioCues.empty() &&
                    second.audioCues.size() == 1,
                "Each hold tick must emit one sound even across repeated updates.");
    }

    void TestLongNoteRemainsInScrollSnapshotUntilItsTail()
    {
        auto profile = std::make_shared<AccuracyRange>();
        ScrollGear gear;
        Lane &lane = gear.CreateLane();
        lane.AddNote(std::make_unique<RuleBasedNote>(
            1, RhythmTime::zero(), profile,
            std::make_unique<TimedSequenceInputRule>(std::vector<NoteAction>{1}, 3,
                                                             RhythmTime{1'000'000})));
        gear.Finalize();

        const ScrollGearSnapshot active =
            gear.BuildSnapshot(RhythmTime{500'000}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        Require(active.notes.size() == 1 &&
                    active.notes.front().expireTime == RhythmTime{1'000'000},
                "A long note head must remain visible after the ordinary past "
                "window until its tail expires.");

        static_cast<void>(gear.Update(RhythmTime{1'000'000}));
        const ScrollGearSnapshot missedTrail =
            gear.BuildSnapshot(RhythmTime{1'100'000}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        const ScrollGearSnapshot expiredTrail =
            gear.BuildSnapshot(RhythmTime{1'220'001}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        Require(missedTrail.notes.size() == 1 &&
                    missedTrail.notes.front().state == NoteState::Missed &&
                    expiredTrail.notes.empty(),
                "A missed timed note must remain in the snapshot for its explicit "
                "post-expiry travel window only.");
    }

    void TestOverlappingLongTrailIsNotHiddenByShorterNote()
    {
        auto profile = std::make_shared<AccuracyRange>();
        ScrollGear gear;
        Lane &lane = gear.CreateLane();
        lane.AddNote(std::make_unique<RuleBasedNote>(
            1, RhythmTime::zero(), profile,
            std::make_unique<TimedSequenceInputRule>(std::vector<NoteAction>{1}, 2,
                                                             RhythmTime{1'000'000})));
        lane.AddNote(std::make_unique<RuleBasedNote>(
            2, RhythmTime{100'000}, profile,
            std::make_unique<TimedSequenceInputRule>(std::vector<NoteAction>{1}, 2,
                                                             RhythmTime{100'000})));
        gear.Finalize();

        static_cast<void>(gear.Update(RhythmTime{1'000'000}));
        const ScrollGearSnapshot snapshot =
            gear.BuildSnapshot(RhythmTime{1'100'000}, RhythmDuration{2'000'000},
                               RhythmDuration{220'000});
        Require(snapshot.notes.size() == 1 && snapshot.notes.front().noteId == 1,
                "An older long-note trail must remain visible when an intervening "
                "short note has already left the past window.");
    }
} // namespace tests
