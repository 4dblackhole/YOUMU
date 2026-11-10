#include "Note/Submodules/HoldInputRule.h"
#include "TestSupport.h"

namespace tests
{
    void TestMappedLaneInput()
    {
        PlaySession session;
        auto &left = session.Gear().CreateLane();
        auto &right = session.Gear().CreateLane();
        const auto profile = std::make_shared<AccuracyRange>();
        const RhythmTime start{100'000}, end{600'000};
        const std::vector<RhythmTime> ticks{RhythmTime{200'000}, RhythmTime{300'000}, RhythmTime{400'000}};
        left.AddNote(std::make_unique<RuleBasedNote>(1, start, profile,
            std::make_unique<HoldInputRule>(1, end, ticks)));
        right.AddNote(std::make_unique<RuleBasedNote>(2, start, profile,
            std::make_unique<HoldInputRule>(2, end, ticks)));
        session.Gear().Finalize();
        session.SetInputLayout({"TwoLane", {{"left", "Left", 1, {'F'}, 0}, {"right", "Right", 2, {'J'}, 1}}});
        session.SetInputMapping({{'F', 1}, {'J', 2}});

        Require(HasEvent(session.ProcessInput('J', InputEdge::Pressed, start), NoteEventType::HitAccepted) &&
                    left.Notes().front()->State() == NoteState::Pending,
                "Pressing the right key must reach only the right lane.");
        Require(HasEvent(session.ProcessInput('F', InputEdge::Pressed, start), NoteEventType::HitAccepted),
                "Both lanes must accept simultaneous heads.");
        static_cast<void>(session.Update(ticks.front(), std::array<PhysicalKey, 2>{'F', 'J'}));
        static_cast<void>(session.ProcessInput('J', InputEdge::Released, RhythmTime{250'000}));
        static_cast<void>(session.Update(ticks.back(), std::array<PhysicalKey, 1>{'F'}));
        static_cast<void>(session.ProcessInput('F', InputEdge::Released, RhythmTime{450'000}));
        static_cast<void>(session.Update(end));

        const auto checkAccuracy = [&] {
            const auto &leftAccuracy = left.Notes().front()->Accuracy();
            const auto &rightAccuracy = right.Notes().front()->Accuracy();
            Require(leftAccuracy.finalized && rightAccuracy.finalized &&
                        leftAccuracy.acceptedHits == 1 && rightAccuracy.acceptedHits == 1 &&
                        leftAccuracy.acceptedTicks == 3 && rightAccuracy.acceptedTicks == 1,
                    "The right release must stop its ticks while the left hold continues.");
        };
        checkAccuracy();
        const auto score = session.Score();
        session.RewindJudgements(end + RhythmDuration{1});
        checkAccuracy();
        Require(session.Score() == score && session.FinalizedNoteCount() == 2,
                "Rewind must route the two lanes identically to live input.");
    }

    void TestInvalidLaneMapping()
    {
        PlaySession session;
        session.Gear().CreateLane();
        session.Gear().CreateLane();
        for (const ModeInputLayout &layout : {
            ModeInputLayout{"TwoLane", {{"bad", "Bad", 1, {'F'}, 2}}},
            ModeInputLayout{"TwoLane", {{"left", "Left", 1, {'F'}, 0}, {"right", "Right", 1, {'J'}, 1}}}})
        {
            bool rejected = false;
            try { session.SetInputLayout(layout); }
            catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected, "Reject missing lanes and an action split across lanes during setup.");
        }
    }
}
