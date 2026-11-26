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
                static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
                static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{1}));
                static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{2}));
            }
            else
            {
                static_cast<void>(session.ProcessInput(type == 3 ? 'D' : 'C', InputEdge::Pressed, RhythmTime{-1'000}));
                static_cast<void>(session.ProcessInput(type == 3 ? 'C' : 'D', InputEdge::Pressed, {}));
                static_cast<void>(session.ProcessInput(type == 3 ? 'M' : 'K', InputEdge::Pressed, RhythmTime{1}));
            }
            Require(session.AccuracyRate() == 0.0, "A POOR poisons the entire large or purple note.");
            session.Reset();
            static_cast<void>(session.ProcessInput(type == 4 ? 'D' : 'C', InputEdge::Pressed, {}));
            static_cast<void>(session.ProcessInput(type == 4 ? 'K' : type == 5 ? 'D' : 'M',
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
            const PhysicalKey correct = kat ? 'D' : 'C', wrong = kat ? 'C' : 'D';
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
            Require(log.finalizedAccuracies.size() == 1 && log.finalizedAccuracies.front().acceptedTicks == 6 &&
                    log.finalizedAccuracies.front().target.ticks == 9,
                    "Wrong-color holds across ticks 4..6 must preserve MAX head and score six of nine ticks.");
            Require(log.finalizedAccuracies.front().hitScoreRates.front() == 1.0 &&
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
            if (!taiko)
                throw std::runtime_error("Tick POOR result must be a Taiko result.");
            Require(taiko->poorCount == 0 && taiko->missedTicks == 3 && taiko->acceptedTicks == 6,
                    "A wrong colour held over a tick only leaves it unprocessed: no POOR (user decision).");
            Require(taiko->okCount == 1 && taiko->summary.maximumCombo == 1,
                    "A clean Buzz head with some ticks is an OK that keeps the combo.");
            Require(taiko->timingErrors.size() == 1,
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

    // BAD recovery (user decision): a BAD or an early BAD settles the note as a BAD with zero accuracy and no
    // combo; the GOOD..MAX that answers an early BAD is marked as a recovery and counts as a BAD. A POOR settles
    // the note as that one POOR: the hit that answers it is a recovery too but adds no count.
    void TestBadRecovery()
    {
        constexpr std::size_t Bad = 4, Miss = 5;
        const auto bad = [](const PlayStatistics &statistics) { return statistics.judgements[Bad]; };
        {
            PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.baseBpm = 120.0;
            for (std::int64_t i = 0; i < 4; ++i)
                pattern.notes.push_back({.position = {0, {i, 4}}, .keyType = 1, .sourceOrder = static_cast<std::size_t>(i)});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "BAD recovery fixture must load.");
            auto &session = *loaded.session;
            const auto early = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000});
            Require(HasEvent(early, NoteEventType::InputRejected) && early.finalizedAccuracies.empty() &&
                        bad(session.Statistics()) == 0,
                    "An early BAD keeps the note and counts nothing until it settles.");
            const auto &lane = *session.Gear().Lanes().front();
            Require(lane.CurrentIndex() == 0, "An early BAD keeps the focus on its note.");
            // More right-colour presses inside the same early BAD window keep the focus too (user decision: "early bad
            // 일때 올바른색으로 다시 쳤을 경우에 포커스를 그대로 유지해야합니다").
            for (const std::int64_t at : {-65'000, -60'000, -56'000})
            {
                static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{at - 2'000}));
                static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{at}));
                Require(lane.CurrentIndex() == 0 && bad(session.Statistics()) == 0,
                        "Pressing again inside the early BAD window keeps the focus and counts nothing.");
            }
            static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{-50'000}));
            const auto recovered = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0});
            const auto hit = std::ranges::find(recovered.events, NoteEventType::HitAccepted, &NoteEvent::type);
            Require(hit != recovered.events.end() && hit->recovery && hit->judgement.grade == JudgementGrade::Max,
                    "The MAX that answers an early BAD is a recovery and keeps its own grade for the word.");
            Require(recovered.finalizedAccuracies.size() == 1 && recovered.finalizedAccuracies.front().Score().Units() == 0 &&
                        recovered.finalizedAccuracies.front().rejectedTiming,
                    "A BAD recovery settles the note at zero accuracy.");
            Require(bad(session.Statistics()) == 1 && session.Statistics().judgements[0] == 0 &&
                        session.Statistics().combo == 0,
                    "A BAD recovery counts as a BAD and adds no combo.");
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{500'000}));
            Require(session.Statistics().combo == 1, "The next clean note starts a new combo.");
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{930'000}));
            const auto left = session.Update(RhythmTime{1'200'000});
            const auto missed = std::ranges::find(left.events, NoteEventType::Missed, &NoteEvent::type);
            Require(missed != left.events.end() && missed->leftAtEarlyBad,
                    "A note left at an early BAD settles through a miss marked as left at that BAD.");
            Require(bad(session.Statistics()) == 2 && session.Statistics().judgements[Miss] == 0 &&
                        session.Statistics().combo == 0,
                    "A note left at an early BAD is a BAD, not a MISS, and breaks the combo.");
            static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{1'500'000}));
            static_cast<void>(session.Update(RhythmTime{1'700'000}));
            Require(session.Statistics().poorCount == 1 && session.Statistics().judgements[Miss] == 0 && bad(session.Statistics()) == 2,
                    "A POOR left without a retry stays that POOR, not a MISS.");
        }
        {
            PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.notes.push_back(PatternNote{.keyType = 3});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Large BAD fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(session.ProcessInput('M', InputEdge::Pressed, {}));
            Require(session.AccuracyRate() == 0.0 && session.Statistics().combo == 0,
                    "An early BAD answered on one hand settles the whole large note as a BAD with no combo.");
            Require(session.Statistics().judgements[0] == 0 && bad(session.Statistics()) == 1,
                    "A large note counts one BAD, not each of its hits (user decision).");
        }
        {
            auto pattern = MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"});
            pattern.notes.back().position.fraction = Rational{9, 16};
            const std::array held{static_cast<PhysicalKey>('C')};
            auto badHead = TaikoMode{}.CreateSession(pattern);
            Require(badHead.Succeeded(), "Buzz BAD fixture must load.");
            // The head only takes GOOD or better, so its BAD is an early BAD answered by the head press (GAME-05).
            static_cast<void>(badHead.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(badHead.session->ProcessInput('C', InputEdge::Released, RhythmTime{-60'000}));
            static_cast<void>(badHead.session->ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(badHead.session->Update(RhythmTime{1'125'000}, held));
            const auto rate = badHead.session->AccuracyRate();
            Require(rate && *rate > 0.0 && *rate <= 0.5, "A BAD Buzz head scores zero while its ticks keep their half.");
            Require(badHead.session->Statistics().okCount == 0 && bad(badHead.session->Statistics()) == 1 &&
                        badHead.session->Statistics().maximumCombo == 0,
                    "A BAD Buzz head counts one BAD, no OK, and no combo.");

            auto emptyBody = TaikoMode{}.CreateSession(pattern);
            Require(emptyBody.Succeeded(), "Buzz empty-body fixture must load.");
            static_cast<void>(emptyBody.session->ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(emptyBody.session->ProcessInput('C', InputEdge::Released, RhythmTime{1'000}));
            const auto finished = emptyBody.session->Update(RhythmTime{1'125'000});
            Require(emptyBody.session->AccuracyRate() == 0.5 && emptyBody.session->Statistics().okCount == 0,
                    "A clean Buzz head with no tick keeps its head score but is not an OK.");
            Require(emptyBody.session->Statistics().judgements[Miss] == 1 && emptyBody.session->Statistics().maximumCombo == 0,
                    "A Buzz that took no tick is a MISS with no combo.");
            Require(finished.finalizedAccuracies.size() == 1 &&
                        finished.finalizedAccuracies.front().CompletionGrade() == JudgementGrade::Miss,
                    "A Buzz that took no tick shows MISS when it ends.");
        }
    }

    // Only a press can be an early BAD (Rules: a key release makes no BAD processing). A release, a second key
    // on a finished note, alternating primary and secondary keys and a wrong colour's release inside the next
    // note's early BAD window must leave that note clean.
    void TestReleaseIsNotEarlyBad()
    {
        constexpr std::size_t Max = 0, Bad = 4;
        const auto cleanHit = [](const NoteProcessResult &result) {
            const auto hit = std::ranges::find(result.events, NoteEventType::HitAccepted, &NoteEvent::type);
            return hit != result.events.end() && !hit->recovery && hit->judgement.grade == JudgementGrade::Max;
        };
        const auto pattern = [](std::initializer_list<int> keyTypes) {
            PatternDocument document;
            document.mode = "Taiko";
            document.baseBpm = 120.0;
            std::int64_t i = 0;
            for (const int keyType : keyTypes)
            {
                document.notes.push_back(
                    {.position = {0, {i, 8}}, .keyType = keyType, .sourceOrder = static_cast<std::size_t>(i)});
                ++i;
            }
            return document;
        };
        {
            // Don notes every 250 ms; each release lands 60..70 ms before the next note (its early BAD window).
            auto loaded = TaikoMode{}.CreateSession(pattern({1, 1, 1, 1}));
            Require(loaded.Succeeded(), "Release early BAD fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            const auto released = session.ProcessInput('C', InputEdge::Released, RhythmTime{180'000});
            Require(released.finalizedAccuracies.empty() && session.Statistics().judgements[Bad] == 0,
                    "Releasing a key inside the next note's early BAD window counts nothing.");
            Require(cleanHit(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{250'000})),
                    "The note after a release inside its early BAD window takes its own grade, not a BAD recovery.");
            // A second Don key a few ms after a finished normal note is far outside the next note's window.
            static_cast<void>(session.ProcessInput('V', InputEdge::Pressed, RhythmTime{255'000}));
            static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{430'000}));
            static_cast<void>(session.ProcessInput('V', InputEdge::Released, RhythmTime{440'000}));
            Require(cleanHit(session.ProcessInput('V', InputEdge::Pressed, RhythmTime{500'000})),
                    "Alternating the primary and secondary Don keys keeps the next note clean.");
            // A wrong colour pressed outside every window and released inside the next note's early BAD window.
            static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{600'000}));
            static_cast<void>(session.ProcessInput('V', InputEdge::Released, RhythmTime{670'000}));
            static_cast<void>(session.ProcessInput('D', InputEdge::Released, RhythmTime{680'000}));
            Require(cleanHit(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{750'000})),
                    "A wrong colour's release inside the early BAD window keeps the note clean.");
            const auto &statistics = session.Statistics();
            Require(statistics.judgements[Max] == 4 && statistics.judgements[Bad] == 0 && statistics.poorCount == 0,
                    "Four clean presses count four MAX and no BAD or POOR.");
            Require(statistics.combo == 4 && session.AccuracyRate() == 1.0,
                    "Releases never break the combo or the accuracy.");
        }
        {
            // A large Don hit with both hands almost together, both released inside the next note's early window.
            auto loaded = TaikoMode{}.CreateSession(pattern({3, 1}));
            Require(loaded.Succeeded(), "Large-note release fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            static_cast<void>(session.ProcessInput('M', InputEdge::Pressed, RhythmTime{8'000}));
            static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{180'000}));
            static_cast<void>(session.ProcessInput('M', InputEdge::Released, RhythmTime{185'000}));
            Require(cleanHit(session.ProcessInput('N', InputEdge::Pressed, RhythmTime{250'000})),
                    "Releasing both hands of a large note inside the next note's early window keeps it clean.");
            // The second hand 8 ms late scores a little under 100%; a BAD would settle the large note at zero.
            const auto rate = session.AccuracyRate();
            Require(session.Statistics().judgements[Bad] == 0 && session.Statistics().combo == 2,
                    "A large note and the note after its releases both stay clean.");
            Require(rate && *rate > 0.99, "A large note and the note after its releases both stay clean.");
        }
        {
            // The user's rule stays: a press inside the early BAD window still makes the next press a BAD recovery.
            auto loaded = TaikoMode{}.CreateSession(pattern({1, 1}));
            Require(loaded.Succeeded(), "Press early BAD fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            static_cast<void>(session.ProcessInput('M', InputEdge::Pressed, RhythmTime{180'000}));
            const auto answered = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{250'000});
            const auto hit = std::ranges::find(answered.events, NoteEventType::HitAccepted, &NoteEvent::type);
            Require(hit != answered.events.end() && hit->recovery && session.Statistics().judgements[Bad] == 1,
                    "A press inside the early BAD window is still an early BAD answered by a BAD recovery.");
        }
    }

    // A wrong colour's press is a POOR, never an early BAD, anywhere within the BAD window before or after the
    // note (user decision: "다른색 키를 쳤으면 POOR입니다 early BAD가 아닙니다"). The note keeps waiting.
    void TestWrongColourPoor()
    {
        constexpr std::size_t Bad = 4, Miss = 5;
        const auto pattern = [](std::initializer_list<int> keyTypes) {
            PatternDocument document;
            document.mode = "Taiko";
            document.baseBpm = 120.0;
            std::int64_t i = 0;
            for (const int keyType : keyTypes)
            {
                document.notes.push_back(
                    {.position = {0, {i, 8}}, .keyType = keyType, .sourceOrder = static_cast<std::size_t>(i)});
                ++i;
            }
            return document;
        };
        const auto rejected = [](const NoteProcessResult &result) {
            const auto event = std::ranges::find(result.events, NoteEventType::InputRejected, &NoteEvent::type);
            return event == result.events.end() ? NoteEvent{} : *event;
        };
        {
            // Don notes at 0 and 250 ms; a Kat press 70 ms before the second is a POOR on that note.
            auto loaded = TaikoMode{}.CreateSession(pattern({1, 1}));
            Require(loaded.Succeeded(), "Early wrong-colour fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            const auto wrong = rejected(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{180'000}));
            Require(wrong.wrongPress && IsPoor(wrong) && !IsEarlyBad(wrong),
                    "A wrong colour inside the early BAD window is a POOR, not an early BAD.");
            Require(session.Statistics().poorCount == 1 && session.Statistics().judgements[Bad] == 0,
                    "The early wrong colour counts one POOR at once and no BAD.");
            const auto answered = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{250'000});
            const auto hit = std::ranges::find(answered.events, NoteEventType::HitAccepted, &NoteEvent::type);
            Require(hit != answered.events.end() && hit->recovery && session.Statistics().judgements[Bad] == 0,
                    "The right colour after that POOR is a recovery that adds no count (POOR 1, BAD 0).");
            Require(session.Statistics().combo == 0 && session.Statistics().poorCount == 1,
                    "The POOR note adds no combo.");
        }
        {
            // A Kat press 70 ms after a Don note (late BAD window) is a POOR; leaving the note keeps that POOR.
            auto loaded = TaikoMode{}.CreateSession(pattern({1}));
            Require(loaded.Succeeded(), "Late wrong-colour fixture must load.");
            auto &session = *loaded.session;
            const auto wrong = rejected(session.ProcessInput('K', InputEdge::Pressed, RhythmTime{70'000}));
            Require(wrong.wrongPress && IsPoor(wrong) && session.Statistics().poorCount == 1,
                    "A wrong colour inside the late BAD window is a POOR.");
            static_cast<void>(session.Update(RhythmTime{500'000}));
            Require(session.Statistics().judgements[Miss] == 0 && session.Statistics().judgements[Bad] == 0 &&
                        session.Statistics().poorCount == 1,
                    "A note left after a late POOR is that POOR, not a MISS.");
        }
        {
            // The right colour in the late BAD window stays a BAD hit.
            auto loaded = TaikoMode{}.CreateSession(pattern({1}));
            Require(loaded.Succeeded(), "Late right-colour fixture must load.");
            auto &session = *loaded.session;
            const auto late = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{70'000});
            Require(HasEvent(late, NoteEventType::HitAccepted) && session.Statistics().judgements[Bad] == 1 &&
                        session.Statistics().poorCount == 0,
                    "The right colour inside the late BAD window is still a BAD hit, not a POOR.");
        }
        {
            // A large Don: a Kat press 70 ms early is a POOR, and the two hands after it settle the note at zero.
            auto loaded = TaikoMode{}.CreateSession(pattern({3}));
            Require(loaded.Succeeded(), "Large wrong-colour fixture must load.");
            auto &session = *loaded.session;
            const auto wrong = rejected(session.ProcessInput('K', InputEdge::Pressed, RhythmTime{-70'000}));
            Require(IsPoor(wrong) && !IsEarlyBad(wrong) && session.Statistics().poorCount == 1,
                    "A wrong colour before a large note is a POOR.");
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(session.ProcessInput('M', InputEdge::Pressed, {}));
            Require(session.AccuracyRate() == 0.0 && session.Statistics().combo == 0,
                    "A large note answered after a POOR settles at zero with no combo.");
        }
        {
            // After a late POOR the focus still moves on: the note expires and the next one takes its own grade.
            auto loaded = TaikoMode{}.CreateSession(pattern({1, 1}));
            Require(loaded.Succeeded(), "Late POOR focus fixture must load.");
            auto &session = *loaded.session;
            static_cast<void>(session.ProcessInput('K', InputEdge::Pressed, RhythmTime{70'000}));
            static_cast<void>(session.Update(RhythmTime{200'000}));
            const auto next = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{250'000});
            const auto hit = std::ranges::find(next.events, NoteEventType::HitAccepted, &NoteEvent::type);
            Require(hit != next.events.end() && !hit->recovery && hit->judgement.grade == JudgementGrade::Max,
                    "The note after a late POOR keeps its own grade.");
            Require(session.Statistics().judgements[Miss] == 0 && session.Statistics().poorCount == 1 &&
                        session.Statistics().combo == 1,
                    "The late POOR note stays a POOR; the next note starts a new combo.");
        }
        {
            // A Buzz head: a wrong colour 70 ms early is a head POOR; the body ticks keep their own rule.
            auto buzz = MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"});
            buzz.notes.back().position.fraction = Rational{9, 16};
            const std::array held{static_cast<PhysicalKey>('C')};
            auto loaded = TaikoMode{}.CreateSession(buzz);
            Require(loaded.Succeeded(), "Buzz wrong-colour fixture must load.");
            auto &session = *loaded.session;
            const auto wrong = rejected(session.ProcessInput('K', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(session.ProcessInput('K', InputEdge::Released, RhythmTime{-60'000}));
            Require(IsPoor(wrong) && !IsEarlyBad(wrong) && session.Statistics().poorCount == 1,
                    "A wrong colour before a Buzz head is a POOR on the head.");
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(session.Update(RhythmTime{1'125'000}, held));
            const auto rate = session.AccuracyRate();
            Require(rate && *rate > 0.0 && *rate <= 0.5,
                    "A POOR Buzz head scores zero while its ticks keep their half.");
            Require(session.Statistics().poorCount == 1, "Buzz ticks add no POOR.");
        }
        {
            // A wrong colour outside the BAD window stays ignored.
            auto loaded = TaikoMode{}.CreateSession(pattern({1}));
            Require(loaded.Succeeded(), "Out-of-window wrong-colour fixture must load.");
            auto &session = *loaded.session;
            const auto outside = rejected(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-150'000}));
            Require(!IsPoor(outside) && session.Statistics().poorCount == 0,
                    "A wrong colour outside the BAD window is not judged.");
            static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
            Require(session.Statistics().combo == 1 && session.AccuracyRate() == 1.0,
                    "The note after an out-of-window wrong colour stays clean.");
        }
    }

    // A POOR is a kind of BAD and settles its note (user decisions: "POOR는 BAD의 아종인것으로 해주세요",
    // "다른 색상을 마구 연타하더라도 POOR카운트는 1개만 오르도록 해주세요", "POOR처리된 노트는 BAD범위로 누르더라도
    // BAD카운트가 올라가지않습니다. POOR복귀는 GOOD 이상의 정확도로 쳐야하는것으로 하겠습니다", "POOR를 치고 그대로
    // 놓칠경우 MISS가 아니라 POOR로 하겠습니다"). Don notes every 1/8 (250 ms) unless noted; JudgeLevel 50.
    void TestPoorSettlesNote()
    {
        constexpr std::size_t Good = 3, Bad = 4, Miss = 5;
        const auto pattern = [](std::initializer_list<int> keyTypes, std::int64_t division) {
            PatternDocument document;
            document.mode = "Taiko";
            document.baseBpm = 120.0;
            std::int64_t i = 0;
            for (const int keyType : keyTypes)
            {
                document.notes.push_back(
                    {.position = {0, {i, division}}, .keyType = keyType, .sourceOrder = static_cast<std::size_t>(i)});
                ++i;
            }
            return document;
        };
        const auto load = [&](std::initializer_list<int> keyTypes, std::int64_t division = 8) {
            auto loaded = TaikoMode{}.CreateSession(pattern(keyTypes, division));
            Require(loaded.Succeeded(), "POOR fixture must load.");
            return std::move(loaded.session);
        };
        const auto firstHit = [](const NoteProcessResult &result) -> const NoteEvent * {
            const auto hit = std::ranges::find(result.events, NoteEventType::HitAccepted, &NoteEvent::type);
            return hit == result.events.end() ? nullptr : &*hit;
        };
        const auto counts = [](const PlayStatistics &statistics, std::size_t poor, std::size_t bad, std::size_t miss) {
            const bool counted = statistics.poorCount == poor && statistics.judgements[Bad] == bad;
            return counted && statistics.judgements[Miss] == miss;
        };
        {
            // Mashing the wrong colour: every press is a POOR to show (red beam, POOR word), the count rises once.
            auto session = load({1});
            for (const std::int64_t at : {-80'000, -40'000, 0, 30'000, 60'000})
            {
                const auto wrong = session->ProcessInput('K', InputEdge::Pressed, RhythmTime{at});
                Require(std::ranges::any_of(wrong.events, IsPoor), "Each wrong-colour press within BAD shows a POOR.");
                static_cast<void>(session->ProcessInput('K', InputEdge::Released, RhythmTime{at + 5'000}));
            }
            Require(counts(session->Statistics(), 1, 0, 0), "Mashing the wrong colour counts one POOR.");
            const auto left = session->Update(RhythmTime{500'000});
            const auto missed = std::ranges::find(left.events, NoteEventType::Missed, &NoteEvent::type);
            Require(missed != left.events.end() && missed->leftAtPoor && !missed->leftAtEarlyBad,
                    "The note left after its POOR misses as left at that POOR (no MISS word).");
            Require(counts(session->Statistics(), 1, 0, 0) && session->Statistics().combo == 0,
                    "A POOR left alone stays that POOR: no MISS, no combo.");
        }
        {
            // After a POOR, the right colour within BAD before the note counts no BAD and keeps the note waiting.
            auto session = load({1, 1});
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            static_cast<void>(session->ProcessInput('D', InputEdge::Pressed, RhythmTime{180'000}));
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{185'000}));
            Require(counts(session->Statistics(), 1, 0, 0), "A BAD-range press after a POOR counts no BAD.");
            Require(session->Gear().Lanes().front()->CurrentIndex() == 1,
                    "That press keeps the focus on the POOR note.");
            const auto answered = session->ProcessInput('C', InputEdge::Pressed, RhythmTime{250'000});
            const NoteEvent *hit = firstHit(answered);
            Require(hit != nullptr && hit->recovery && hit->judgement.grade == JudgementGrade::Max,
                    "The on-time hit after it answers the POOR and shows its own grade.");
            Require(counts(session->Statistics(), 1, 0, 0) && session->Statistics().judgements[0] == 1,
                    "That recovery adds no count; only the first note's MAX and the POOR are counted.");
        }
        {
            // After a late POOR, the right colour still in the late BAD window is not taken; the note then expires as
            // its POOR.
            auto session = load({1});
            static_cast<void>(session->ProcessInput('K', InputEdge::Pressed, RhythmTime{60'000}));
            const auto late = session->ProcessInput('C', InputEdge::Pressed, RhythmTime{70'000});
            Require(firstHit(late) == nullptr && HasEvent(late, NoteEventType::InputRejected),
                    "A note with a POOR does not take a BAD hit.");
            static_cast<void>(session->Update(RhythmTime{500'000}));
            Require(counts(session->Statistics(), 1, 0, 0), "It ends as its POOR: no BAD and no MISS.");
        }
        {
            // A POOR within GOOD (wrong colour on time), then GOOD or better answers it; releases add nothing.
            auto session = load({1});
            static_cast<void>(session->ProcessInput('K', InputEdge::Pressed, RhythmTime{-10'000}));
            static_cast<void>(session->ProcessInput('K', InputEdge::Released, RhythmTime{-5'000}));
            static_cast<void>(session->ProcessInput('C', InputEdge::Released, RhythmTime{0}));
            Require(counts(session->Statistics(), 1, 0, 0), "Releases after a POOR add nothing.");
            const auto answered = session->ProcessInput('C', InputEdge::Pressed, RhythmTime{40'000});
            const NoteEvent *hit = firstHit(answered);
            Require(hit != nullptr && hit->recovery && hit->judgement.grade == JudgementGrade::Good,
                    "A GOOD answers the POOR.");
            Require(counts(session->Statistics(), 1, 0, 0) && session->Statistics().judgements[Good] == 0,
                    "The GOOD that answers a POOR adds no count.");
            Require(session->AccuracyRate() == 0.0, "A note with a POOR keeps zero accuracy, like a BAD.");
        }
        {
            // An early BAD, then a POOR on the same note: the one judged first, the BAD, is the note's result (user
            // decision). The POOR still shows its word but counts nothing; left alone, answered or retired alike.
            auto session = load({1});
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            const auto poor = session->ProcessInput('K', InputEdge::Pressed, RhythmTime{-60'000});
            Require(std::ranges::any_of(poor.events, IsPoor) && counts(session->Statistics(), 0, 0, 0),
                    "A POOR after an early BAD shows but counts nothing yet.");
            static_cast<void>(session->Update(RhythmTime{500'000}));
            Require(counts(session->Statistics(), 0, 1, 0) && session->Statistics().combo == 0,
                    "An early BAD followed by a POOR settles as one BAD.");
            auto answered = load({1});
            static_cast<void>(answered->ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(answered->ProcessInput('K', InputEdge::Pressed, RhythmTime{-60'000}));
            static_cast<void>(answered->ProcessInput('C', InputEdge::Released, RhythmTime{-50'000}));
            static_cast<void>(answered->ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            Require(counts(answered->Statistics(), 0, 1, 0) && answered->Statistics().judgements[0] == 0,
                    "Answering that note on time still counts only its BAD.");
            auto retired = load({1});
            static_cast<void>(retired->ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(retired->ProcessInput('K', InputEdge::Pressed, RhythmTime{-60'000}));
            retired->RetireRemaining(RhythmTime{-50'000});
            Require(counts(retired->Statistics(), 0, 1, 0), "Retired after an early BAD and a POOR, it counts one BAD.");
            auto earlyOnly = load({1});
            static_cast<void>(earlyOnly->ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            earlyOnly->RetireRemaining(RhythmTime{-50'000});
            Require(counts(earlyOnly->Statistics(), 0, 0, 1), "Retired with only an early BAD, it stays a MISS.");
        }
        {
            // A note right behind (1/16, 125 ms): after a late POOR the focus moves on as usual and the next note
            // takes its own grade.
            auto session = load({1, 1}, 16);
            static_cast<void>(session->ProcessInput('K', InputEdge::Pressed, RhythmTime{70'000}));
            const auto next = session->ProcessInput('C', InputEdge::Pressed, RhythmTime{95'000});
            const NoteEvent *hit = firstHit(next);
            Require(hit != nullptr && !hit->recovery && hit->noteId == session->Gear().Lanes().front()->Notes()[1]->Id(),
                    "The press inside the next note's GOOD goes to that note, not a BAD on the POOR note.");
            Require(counts(session->Statistics(), 1, 0, 0) && session->Statistics().combo == 1,
                    "The POOR note ends as its POOR and the next note starts a combo.");
        }
        {
            // A purple note hit twice with the same colour: the second is a POOR, counted once, and the other colour
            // within GOOD completes it with no further count.
            auto session = load({5});
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            const auto twice = session->ProcessInput('V', InputEdge::Pressed, RhythmTime{5'000});
            static_cast<void>(session->ProcessInput('M', InputEdge::Pressed, RhythmTime{8'000}));
            Require(std::ranges::any_of(twice.events, IsPoor) && counts(session->Statistics(), 1, 0, 0),
                    "The same colour twice on a purple note is one POOR however often it repeats.");
            static_cast<void>(session->ProcessInput('K', InputEdge::Pressed, RhythmTime{10'000}));
            Require(counts(session->Statistics(), 1, 0, 0), "The purple note completed after its POOR adds no count.");
        }
        {
            // A Buzz head follows the same rule (user decision): an early BAD, then a POOR, is one BAD and no POOR,
            // whether the head is then answered, left alone or retired. A POOR that comes first stays the one POOR.
            auto buzz = MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"});
            buzz.notes.back().position.fraction = Rational{9, 16};
            const std::array held{static_cast<PhysicalKey>('C')};
            const auto start = [&] {
                auto loaded = TaikoMode{}.CreateSession(buzz);
                Require(loaded.Succeeded(), "Buzz first-fault fixture must load.");
                return std::move(loaded.session);
            };
            const auto earlyBadThenPoor = [&](auto &session) {
                const auto early = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000});
                Require(std::ranges::any_of(early.events, IsEarlyBad), "The right colour 70 ms early is an early BAD.");
                static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{-65'000}));
                const auto poor = session.ProcessInput('K', InputEdge::Pressed, RhythmTime{-60'000});
                Require(std::ranges::any_of(poor.events, IsPoor) && counts(session.Statistics(), 0, 0, 0),
                        "A POOR after a Buzz head's early BAD shows but counts nothing yet.");
                static_cast<void>(session.ProcessInput('K', InputEdge::Released, RhythmTime{-55'000}));
            };
            auto answered = start();
            earlyBadThenPoor(*answered);
            static_cast<void>(answered->ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(answered->Update(RhythmTime{1'125'000}, held));
            Require(counts(answered->Statistics(), 0, 1, 0) && answered->Statistics().okCount == 0,
                    "A Buzz head answered after an early BAD and a POOR counts one BAD.");
            auto left = start();
            earlyBadThenPoor(*left);
            static_cast<void>(left->Update(RhythmTime{1'125'000}));
            Require(counts(left->Statistics(), 0, 1, 0), "Left alone after an early BAD and a POOR, the Buzz counts one BAD.");
            auto retired = start();
            earlyBadThenPoor(*retired);
            retired->RetireRemaining(RhythmTime{-50'000});
            Require(counts(retired->Statistics(), 0, 1, 0),
                    "Retired after an early BAD and a POOR, the Buzz counts one BAD.");
            auto poorFirst = start();
            static_cast<void>(poorFirst->ProcessInput('K', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(poorFirst->ProcessInput('K', InputEdge::Released, RhythmTime{-65'000}));
            static_cast<void>(poorFirst->ProcessInput('C', InputEdge::Pressed, RhythmTime{-60'000}));
            static_cast<void>(poorFirst->ProcessInput('C', InputEdge::Released, RhythmTime{-55'000}));
            static_cast<void>(poorFirst->ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(poorFirst->Update(RhythmTime{1'125'000}, held));
            Require(counts(poorFirst->Statistics(), 1, 0, 0), "A Buzz head whose POOR came first stays that one POOR.");
            auto badOnly = start();
            static_cast<void>(badOnly->ProcessInput('C', InputEdge::Pressed, RhythmTime{-70'000}));
            static_cast<void>(badOnly->ProcessInput('C', InputEdge::Released, RhythmTime{-60'000}));
            static_cast<void>(badOnly->ProcessInput('C', InputEdge::Pressed, {}));
            static_cast<void>(badOnly->Update(RhythmTime{1'125'000}, held));
            Require(counts(badOnly->Statistics(), 0, 1, 0), "A Buzz head answered after an early BAD alone is one BAD.");
        }
    }

    // A note that only takes GOOD or better (big and purple notes, a Buzz head) does not take a right colour pressed
    // within BAD after it. That press is still a timing mistake: the note is a BAD from then on and ends as one BAD
    // when it is missed, like an early BAD (user decision on the late BAD of these notes: "1").
    void TestLateBadSettlesNote()
    {
        constexpr std::size_t Bad = 4, Miss = 5;
        const auto load = [](const int keyType) {
            PatternDocument document;
            document.mode = "Taiko";
            document.baseBpm = 120.0;
            document.notes.push_back({.position = {0, {0, 8}}, .keyType = keyType, .sourceOrder = 0});
            auto loaded = TaikoMode{}.CreateSession(document);
            Require(loaded.Succeeded(), "Late BAD fixture must load.");
            return std::move(loaded.session);
        };
        const auto counts = [](const PlayStatistics &statistics, std::size_t poor, std::size_t bad, std::size_t miss) {
            const bool counted = statistics.poorCount == poor && statistics.judgements[Bad] == bad;
            return counted && statistics.judgements[Miss] == miss;
        };
        for (const int keyType : {static_cast<int>(TaikoNoteType::BigDon), static_cast<int>(TaikoNoteType::Purple)})
        {
            // Nothing but a late press: shown as a BAD, counted when the note is missed, and not a MISS.
            auto session = load(keyType);
            const auto late = session->ProcessInput('C', InputEdge::Pressed, RhythmTime{70'000});
            Require(std::ranges::any_of(late.events, IsLateBad) && counts(session->Statistics(), 0, 0, 0),
                    "A right colour 70 ms late on a big note is a late BAD that counts nothing yet.");
            static_cast<void>(session->Update(RhythmTime{500'000}));
            Require(counts(session->Statistics(), 0, 1, 0) && session->Statistics().combo == 0,
                    "A big note left after its late BAD ends as one BAD, not a MISS.");
            Require(session->AccuracyRate() == 0.0, "That note scores zero, like any BAD.");
        }
        {
            // One side on time, the other too late: one BAD for the note.
            auto session = load(static_cast<int>(TaikoNoteType::BigDon));
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{0}));
            const auto late = session->ProcessInput('V', InputEdge::Pressed, RhythmTime{70'000});
            Require(std::ranges::any_of(late.events, IsLateBad), "The second side 70 ms late is a late BAD.");
            static_cast<void>(session->Update(RhythmTime{500'000}));
            Require(counts(session->Statistics(), 0, 1, 0), "A big note whose second side came too late is one BAD.");
        }
        {
            // The late BAD came first, so a wrong colour after it is shown but not counted (the first of the two).
            auto session = load(static_cast<int>(TaikoNoteType::BigDon));
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{60'000}));
            const auto poor = session->ProcessInput('K', InputEdge::Pressed, RhythmTime{70'000});
            Require(std::ranges::any_of(poor.events, IsPoor), "A wrong colour after the late BAD still shows a POOR.");
            static_cast<void>(session->Update(RhythmTime{500'000}));
            Require(counts(session->Statistics(), 0, 1, 0), "A late BAD followed by a POOR is one BAD.");
        }
        {
            // A POOR first stays the POOR: the late right colour after it counts nothing.
            auto session = load(static_cast<int>(TaikoNoteType::BigDon));
            static_cast<void>(session->ProcessInput('K', InputEdge::Pressed, RhythmTime{60'000}));
            static_cast<void>(session->ProcessInput('C', InputEdge::Pressed, RhythmTime{70'000}));
            static_cast<void>(session->Update(RhythmTime{500'000}));
            Require(counts(session->Statistics(), 1, 0, 0), "A POOR followed by a late BAD stays one POOR.");
        }
        {
            // A Buzz head pressed too late: the Buzz ends as one BAD.
            auto buzz = MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"});
            buzz.notes.back().position.fraction = Rational{9, 16};
            auto loaded = TaikoMode{}.CreateSession(buzz);
            Require(loaded.Succeeded(), "Buzz late BAD fixture must load.");
            auto &session = *loaded.session;
            const auto late = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{70'000});
            Require(std::ranges::any_of(late.events, IsLateBad), "A Buzz head pressed 70 ms late is a late BAD.");
            static_cast<void>(session.Update(RhythmTime{1'125'000}));
            Require(counts(session.Statistics(), 0, 1, 0) && session.Statistics().okCount == 0,
                    "A Buzz whose head was pressed too late ends as one BAD.");
        }
        {
            // A normal note is unchanged: its late BAD is a hit and one BAD at once.
            auto session = load(static_cast<int>(TaikoNoteType::Don));
            const auto late = session->ProcessInput('C', InputEdge::Pressed, RhythmTime{70'000});
            Require(!std::ranges::any_of(late.events, IsLateBad) && counts(session->Statistics(), 0, 1, 0),
                    "A normal note takes its late BAD as a hit.");
        }
    }

    // BAD is a timing mistake and POOR a wrong action (user decision: "BAD는 타이밍이 맞지않은 처리, POOR은 동작이
    // 맞지않은 처리를 의미하도록 해주세요"). Rolls and balloons judge no timing, so a press just before them is
    // neither: no early BAD, no POOR, no word and no red beam.
    void TestRollHeadPressIsNotJudged()
    {
        const auto judged = [](const NoteProcessResult &result) {
            return std::ranges::any_of(result.events, [](const NoteEvent &event) {
                return IsPoor(event) || IsEarlyBad(event) || event.judgement.grade == JudgementGrade::Bad;
            });
        };
        for (const auto type : {TaikoNoteType::Roll, TaikoNoteType::BigRoll, TaikoNoteType::Balloon})
        {
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(type));
            Require(loaded.Succeeded(), "Roll head fixture must load.");
            auto &session = *loaded.session;
            for (const std::int64_t at : {-70'000, -20'000})
            {
                const auto early = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{at});
                static_cast<void>(session.ProcessInput('C', InputEdge::Released, RhythmTime{at + 5'000}));
                Require(HasEvent(early, NoteEventType::InputRejected) && !judged(early),
                        "A press before a roll or balloon head is not graded: no early BAD and no POOR.");
            }
            const auto &statistics = session.Statistics();
            Require(statistics.poorCount == 0 && statistics.judgements[static_cast<std::size_t>(JudgementGrade::Bad)] == 0,
                    "Presses before a roll or balloon count no POOR and no BAD.");
            const auto head = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0});
            Require(HasEvent(head, NoteEventType::TickAccepted) || HasEvent(head, NoteEventType::HitAccepted),
                    "The roll or balloon still takes the press on its head.");
        }
    }

    // The last tick of a tick roll keeps its whole GOOD window, even past the tail (user decision: "맨마지막 틱의
    // 포커스가 끝나기 전 혹은 처리되기 전에 틱롤노트의 처리가 종료되면 안됩니다"). The window comes from the note's
    // judgement profile, never a fixed number, so it follows the judge level.
    void TestTickRollLastTickWindow()
    {
        // 120 BPM, half a measure, 16th ticks: nine ticks every 125 ms and the ninth on the tail at 1000 ms.
        constexpr std::int64_t Interval = 125'000, Tail = 1'000'000;
        const auto play = [](TaikoNoteType type, double level, std::int64_t lastPress, std::int64_t settleAt) {
            auto pattern = MakeLongPattern(type, {"TickDivision=16"});
            pattern.judgementLevel = JudgeLevel{level};
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Last-tick fixture must load.");
            NoteProcessResult log;
            for (std::int64_t i = 0; i < 8; ++i)
                log.Append(loaded.session->ProcessInput(i % 2 ? 'M' : 'C', InputEdge::Pressed, RhythmTime{i * Interval}));
            log.Append(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{lastPress}));
            log.Append(loaded.session->Update(RhythmTime{settleAt}));
            return log;
        };
        const auto ticks = [](const NoteProcessResult &log) {
            return log.finalizedAccuracies.empty() ? std::size_t{} : log.finalizedAccuracies.front().acceptedTicks;
        };
        for (const double level : {50.0, 25.0, 80.0})
        {
            const auto window = AccuracyRange("Taiko.Default", JudgeLevel{level}).HalfWindow(JudgementGrade::Good).count();
            const std::int64_t edge = Tail + window;
            for (const auto type : {TaikoNoteType::TickRoll, TaikoNoteType::BigTickRoll})
            {
                const auto inside = play(type, level, edge, edge + 1);
                Require(ticks(inside) == 9 && inside.finalizedAccuracies.front().target.ticks == 9,
                        "A press at the late edge of the last tick's window is taken, even after the tail.");
                const auto outside = play(type, level, edge + 1, edge + 2);
                Require(ticks(outside) == 8, "A press just past the last tick's window is not taken.");
            }
            // The roll stays open while its last tick waits, and settles at the late edge of that window.
            auto pattern = MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=16"});
            pattern.judgementLevel = JudgeLevel{level};
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Last-tick settle fixture must load.");
            for (std::int64_t i = 0; i < 8; ++i)
                static_cast<void>(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{i * Interval}));
            Require(!HasEvent(loaded.session->Update(RhythmTime{edge - 1}), NoteEventType::Completed),
                    "A roll whose last tick still waits does not end at its tail.");
            const auto settled = loaded.session->Update(RhythmTime{edge});
            Require(HasEvent(settled, NoteEventType::Completed) && HasEvent(settled, NoteEventType::TickMissed) &&
                        ticks(settled) == 8,
                    "At the late edge of the last tick's window the roll ends and that tick is missed.");
        }
        {
            // A key released after the tail while the last tick waits takes nothing; the press after it still takes
            // the last tick.
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=16"}));
            Require(loaded.Succeeded(), "Release past the tail fixture must load.");
            NoteProcessResult log;
            for (std::int64_t i = 0; i < 8; ++i)
            {
                log.Append(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{i * Interval}));
                log.Append(loaded.session->ProcessInput('C', InputEdge::Released, RhythmTime{i * Interval + 5'000}));
            }
            log.Append(loaded.session->ProcessInput('M', InputEdge::Released, RhythmTime{Tail + 10'000}));
            Require(log.finalizedAccuracies.empty() && loaded.session->Gear().Lanes().front()->CurrentIndex() == 0,
                    "A release after the tail neither takes the last tick nor ends the roll.");
            log.Append(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{Tail + 20'000}));
            log.Append(loaded.session->Update(RhythmTime{Tail + 30'000}));
            Require(ticks(log) == 9, "The press after that release still takes the last tick.");
        }
        {
            // Every tick taken on time: the press that takes the last tick ends the roll with full accuracy.
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=16"}));
            Require(loaded.Succeeded(), "Clean tick-roll fixture must load.");
            for (std::int64_t i = 0; i < 8; ++i)
                static_cast<void>(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{i * Interval}));
            const auto ended = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{Tail});
            Require(HasEvent(ended, NoteEventType::Completed) && ticks(ended) == 9 &&
                        loaded.session->AccuracyRate() == 1.0,
                    "A roll with every tick taken ends on its last tick with full accuracy.");
            Require(loaded.session->Update(RhythmTime{Tail + 1}).events.empty(),
                    "A roll that already ended emits nothing at its tail.");
        }
        {
            // A single-tick roll (the tail is off the tick grid) keeps the head tick's late window.
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=1"}));
            Require(loaded.Succeeded(), "Single-tick roll fixture must load.");
            const auto window = AccuracyRange("Taiko.Default", JudgeLevel{50}).HalfWindow(JudgementGrade::Good).count();
            auto taken = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{window});
            taken.Append(loaded.session->Update(RhythmTime{Tail}));
            Require(ticks(taken) == 1 && taken.finalizedAccuracies.front().target.ticks == 1,
                    "A single-tick roll takes its tick through the window's late edge.");
        }
        {
            // A Don right after the tail (1/16 later) while the last tick waits, at a wide judge level where the
            // windows overlap: a press inside the last tick's window goes to the roll, and the Don then takes its
            // own on-time press.
            auto pattern = MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=16"});
            pattern.judgementLevel = JudgeLevel{25};
            pattern.notes.push_back(
                {.position = {0, Rational{9, 16}}, .keyType = 1, .sourceOrder = 2});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Roll-then-Don fixture must load.");
            auto &session = *loaded.session;
            for (std::int64_t i = 0; i < 8; ++i)
                static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{i * Interval}));
            auto log = session.ProcessInput('M', InputEdge::Pressed, RhythmTime{Tail + 60'000});
            log.Append(session.ProcessInput('C', InputEdge::Pressed, RhythmTime{Tail + Interval}));
            Require(ticks(log) == 9, "A press inside the last tick's window goes to the roll's last tick.");
            const auto hit = std::ranges::find(log.events, NoteEventType::HitAccepted, &NoteEvent::type);
            Require(hit != log.events.end() && hit->judgement.grade == JudgementGrade::Max && !hit->recovery,
                    "The Don after the roll takes its own on-time press.");
        }
    }

    // A note right behind a tick roll (user decision: the roll keeps the focus while a tick can still be hit, and
    // gives it up as soon as that is no longer so). Judge level 25 so the windows overlap; the next note is a
    // 1/32 after the tail (62.5 ms) or a 1/16 (125 ms).
    void TestTickRollFocusAgainstNextNote()
    {
        constexpr std::int64_t Interval = 125'000, Tail = 1'000'000;
        const auto session = [](TaikoNoteType next, Rational at) {
            auto pattern = MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=16"});
            pattern.judgementLevel = JudgeLevel{25};
            pattern.notes.push_back({.position = {0, at}, .keyType = static_cast<int>(next), .sourceOrder = 2});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Require(loaded.Succeeded(), "Roll-then-note fixture must load.");
            for (std::int64_t i = 0; i < 8; ++i)
                static_cast<void>(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{i * Interval}));
            return std::move(loaded.session);
        };
        const auto noteId = [](const auto &play, std::size_t index) {
            return play->Gear().Lanes().front()->Notes()[index]->Id();
        };
        const auto rollTicks = [&](const auto &play, const NoteProcessResult &log) {
            const auto found = std::ranges::find(log.finalizedAccuracies, noteId(play, 0), &NoteAccuracy::noteId);
            return found == log.finalizedAccuracies.end() ? std::size_t{} : found->acceptedTicks;
        };
        // The next note's first accepted hit, if any.
        const auto nextHit = [&](const auto &play, const NoteProcessResult &log) -> const NoteEvent * {
            const auto id = noteId(play, 1);
            const auto found = std::ranges::find_if(log.events, [&](const NoteEvent &event) {
                return event.type == NoteEventType::HitAccepted && event.noteId == id;
            });
            return found == log.events.end() ? nullptr : &*found;
        };
        // The last tick still waits: a press of either colour inside its window goes to the roll, even within the
        // next note's GOOD window, and the next note then takes its own on-time press.
        struct Waiting
        {
            TaikoNoteType next;
            PhysicalKey press;
            PhysicalKey onTime;
        };
        for (const Waiting waiting : {Waiting{TaikoNoteType::Kat, 'K', 'K'}, Waiting{TaikoNoteType::BigKat, 'D', 'K'},
                                      Waiting{TaikoNoteType::BigDon, 'K', 'C'}})
        {
            auto play = session(waiting.next, Rational{17, 32});
            auto log = play->ProcessInput(waiting.press, InputEdge::Pressed, RhythmTime{Tail + 30'000});
            Require(rollTicks(play, log) == 9 && nextHit(play, log) == nullptr,
                    "A press of either colour in the last tick's window takes that tick, not the next note.");
            log.Append(play->ProcessInput(waiting.onTime, InputEdge::Pressed, RhythmTime{Tail + 62'500}));
            const NoteEvent *hit = nextHit(play, log);
            Require(hit != nullptr && hit->judgement.grade == JudgementGrade::Max && !hit->recovery,
                    "The note after the roll takes its own on-time press.");
            Require(play->Statistics().poorCount == 0, "The roll's last tick leaves no POOR on the next note.");
        }
        // Every tick taken before the tail: the roll ends on that press and the next input goes to the next note,
        // still before the tail.
        struct Settled
        {
            TaikoNoteType next;
            PhysicalKey press;
        };
        for (const Settled settled : {Settled{TaikoNoteType::Don, 'C'}, Settled{TaikoNoteType::Kat, 'K'},
                                      Settled{TaikoNoteType::BigDon, 'M'}})
        {
            auto play = session(settled.next, Rational{17, 32});
            auto log = play->ProcessInput('C', InputEdge::Pressed, RhythmTime{Tail - 30'000});
            Require(rollTicks(play, log) == 9 && HasEvent(log, NoteEventType::Completed),
                    "Taking the last tick before the tail ends the roll at once.");
            Require(play->Gear().Lanes().front()->CurrentIndex() == 1, "The next note takes the focus.");
            log.Append(play->ProcessInput(settled.press, InputEdge::Pressed, RhythmTime{Tail - 10'000}));
            const NoteEvent *hit = nextHit(play, log);
            Require(hit != nullptr && IsAtLeastAsAccurateAs(hit->judgement.grade, JudgementGrade::Good),
                    "A press right after the roll ended is the next note's hit, not a rejected roll input.");
            Require(!hit->recovery, "That hit is not a BAD recovery.");
        }
        {
            // The last tick's window passes with the tick missed: the roll settles and the next press goes to the
            // next note.
            const auto window = AccuracyRange("Taiko.Default", JudgeLevel{25}).HalfWindow(JudgementGrade::Good).count();
            auto play = session(TaikoNoteType::Kat, Rational{9, 16});
            auto log = play->ProcessInput('K', InputEdge::Pressed, RhythmTime{Tail + window + 1});
            Require(rollTicks(play, log) == 8, "A press past the last tick's window leaves that tick missed.");
            Require(nextHit(play, log) != nullptr, "That press goes to the next note once the roll can no longer take it.");
        }
    }

    void TestPoorSingleNoteAccuracy()
    {
        auto profile = std::make_shared<AccuracyRange>();
        PlaySession session;
        session.SetInputMapping({{'C', 1}, {'D', 2}});
        auto &lane = session.Gear().CreateLane();
        lane.AddNote(MakeTap(1, RhythmTime{0}, profile));
        session.Gear().Finalize();

        const auto rejected = session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-10'000});
        Require(HasEvent(rejected, NoteEventType::InputRejected) && rejected.finalizedAccuracies.empty(),
                "A POOR must leave the note available for a subsequent correct input.");
        const auto accepted = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0});
        Require(HasEvent(accepted, NoteEventType::HitAccepted) && accepted.finalizedAccuracies.size() == 1 &&
                accepted.finalizedAccuracies.front().Score().Units() == 0,
                "A POOR followed by a MAX must finalize the logical note with zero accuracy.");
        Require(session.AccuracyRate() == 0.0,
                "A POOR followed by a MAX must finalize the logical note with zero accuracy.");

        session.Reset();
        static_cast<void>(session.ProcessInput('D', InputEdge::Released, RhythmTime{-10'000}));
        static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-100'000}));
        const auto clean = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0});
        Require(clean.finalizedAccuracies.size() == 1 && clean.finalizedAccuracies.front().Score().Rate() == 1.0,
                "Reset, releases, and out-of-range inputs must not poison a clean note.");

        session.Reset();
        static_cast<void>(session.ProcessInput('D', InputEdge::Pressed, RhythmTime{-10'000}));
        session.RewindJudgements(RhythmTime{-15'000});
        const auto rewound = session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0});
        Require(rewound.finalizedAccuracies.size() == 1 && rewound.finalizedAccuracies.front().Score().Rate() == 1.0,
                "Rewind must remove the POOR penalty when it removes the rejected input.");
    }

    void TestTimingAccuracyAndSessionAggregation()
    {
        auto profile = std::make_shared<AccuracyRange>();
        PlaySession session;
        session.SetInputMapping({{'C', 1}, {'M', 1}});
        auto &lane = session.Gear().CreateLane();
        lane.AddNote(std::make_unique<RuleBasedNote>(
            1, RhythmTime{0}, profile,
            std::make_unique<CountedHitInputRule>(1, 2)));
        lane.AddNote(MakeTap(2, RhythmTime{200'000}, profile));
        session.Gear().Finalize();
        Require(!session.AccuracyRate(), "Accuracy must be unavailable before a final note.");
        const auto first =
            session.ProcessInput('C', InputEdge::Pressed, RhythmTime{0});
        Require(first.finalizedAccuracies.empty(), "Big first hit must not finalize accuracy.");
        const auto second =
            session.ProcessInput('M', InputEdge::Pressed, RhythmTime{16'500});
        const double hit2 = profile->Evaluate({}, RhythmTime{16'500}).scoreRate;
        const double bigRate = AccuracyScore::FromRate((1.0 + hit2) * 0.5).Rate();
        const auto bigAccuracyRate = session.AccuracyRate();
        const auto &bigAccuracy = session.LastNoteAccuracy();
        Require(second.finalizedAccuracies.size() == 1 && bigAccuracyRate && bigAccuracy,
                "Big accuracy must preserve both interpolated hits and their mean.");
        Require(std::abs(*bigAccuracyRate - bigRate) < 1e-9 &&
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
        static_cast<void>(session.ProcessInput('C', InputEdge::Pressed, {}));
        // A big note waits through the BAD window before it is missed, like a normal note (user decision).
        const auto partial = session.Update(RhythmTime{profile->HalfWindow(JudgementGrade::Bad).count() + 1});
        Require(partial.finalizedAccuracies.size() == 1 &&
                    partial.finalizedAccuracies.front().ScoreRate() == 0.0 &&
                    partial.finalizedAccuracies.front().hitScoreRates[0] == 0.0,
                "A note missed with its second hit unfilled is a MISS: zero, not half (user decision).");
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
            const auto firstKey = katFirst ? 'D' : 'C';
            const auto secondKey = katFirst ? 'C' : 'D';
            const auto first =
                loaded.session->ProcessInput(firstKey, InputEdge::Pressed, {});
            const auto repeat = loaded.session->ProcessInput(firstKey, InputEdge::Pressed,
                                                             RhythmTime{1'000});
            const auto second = loaded.session->ProcessInput(secondKey, InputEdge::Pressed,
                                                             RhythmTime{2'000});
            Require(first.audioCues.size() == 1 &&
                    first.audioCues.front().sound == (katFirst ? "Taiko.Kat.Hit" : "Taiko.Don.Hit") &&
                    !HasEvent(repeat, NoteEventType::HitAccepted),
                    "Purple accepts either order but a rejected repeated color leaves zero final accuracy.");
            Require(HasEvent(second, NoteEventType::Completed) && loaded.session->AccuracyRate() == 0.0,
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
                        type == TaikoNoteType::DengDeng && hit % 2 != 0 ? 'D' : 'C';
                    static_cast<void>(loaded.session->ProcessInput(
                        key, InputEdge::Pressed, RhythmTime{hit * 10'000}));
                }
                const auto expired = loaded.session->Update(RhythmTime{1'000'000});
                const auto finalRate = loaded.session->AccuracyRate();
                const auto &finalAccuracy = loaded.session->LastNoteAccuracy();
                Require(HasEvent(expired, NoteEventType::Missed) && finalRate && finalAccuracy,
                        "Eight of ten hits must finalize at 80 percent, even on timeout.");
                Require(std::abs(*finalRate - 0.8) < 1e-9 && finalAccuracy->acceptedHits == 8 &&
                        finalAccuracy->target.hits == 10,
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
            static_cast<void>(roll.session->ProcessInput('C', InputEdge::Pressed,
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
        const auto early = tickRoll.session->ProcessInput('C', InputEdge::Pressed,
                                                          RhythmTime{-70'000});
        Require(std::ranges::none_of(early.events,
                                     [](const auto &event) {
                                         return event.judgement.grade ==
                                                JudgementGrade::Bad;
                                     }),
                "TickRoll must not emit a head BAD judgement.");
        for (const int time : {-54'500, 70'500})
        {
            const auto hit = tickRoll.session->ProcessInput('C', InputEdge::Pressed,
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
        // The missed tail tick keeps its window past the tail, so the roll settles at that window's late edge.
        const auto &rollNote = *tickRoll.session->Gear().Lanes().front()->Notes().front();
        static_cast<void>(tickRoll.session->Update(
            RhythmTime{1'000'000} + rollNote.Profile().HalfWindow(JudgementGrade::Good)));
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
            hold.ProcessInput({RhythmTime{14'166}, 1, 'C', InputEdge::Pressed}));
        static_cast<void>(hold.ProcessInput(
            {RhythmTime{9'850'000}, 1, 'C', InputEdge::Released}));
        const auto done = hold.Update({RhythmTime{10'100'000}, {}});
        const double head = profile->Evaluate({}, RhythmTime{14'166}).scoreRate;
        Require(done.finalizedAccuracies.size() == 1 && hold.Accuracy().acceptedTicks == 98 &&
                std::abs(hold.Accuracy().ScoreRate() - AccuracyScore::FromRate((head + 0.98) * 0.5).Rate()) < 1e-9,
                "Buzz must combine interpolated head and 98/100 held ticks equally.");
        Require(std::abs(hold.Accuracy().ScoreRate() - 0.97) < 0.00001,
                "Buzz must combine interpolated head and 98/100 held ticks equally.");
        Require(hold.Update({RhythmTime{11'000'000}, {}}).finalizedAccuracies.empty(),
                "An ended hold must not emit its accuracy twice.");
        hold.Reset();
        static_cast<void>(hold.ProcessInput({{}, 1, 'C', InputEdge::Pressed}));
        static_cast<void>(
            hold.ProcessInput({RhythmTime{1}, 1, 'M', InputEdge::Pressed}));
        static_cast<void>(
            hold.ProcessInput({RhythmTime{2}, 1, 'C', InputEdge::Released}));
        static_cast<void>(
            hold.ProcessInput({RhythmTime{150'000}, 1, 'M', InputEdge::Released}));
        Require(hold.Accuracy().acceptedTicks == 1,
                "Releasing one physical key must not cancel another held key of the same action.");

        auto buzz = TaikoMode{}.CreateSession(
            MakeLongPattern(TaikoNoteType::Buzz, {"Action=Don", "TickDivision=16"}));
        Require(buzz.session->FindNotePresentation(1)->tickTimes.size() == 8 &&
                    buzz.session->FindNotePresentation(1)->tickTimes.front() == RhythmTime{125'000},
                "Buzz body ticks must exclude the independently judged head.");
    }
} // namespace tests
