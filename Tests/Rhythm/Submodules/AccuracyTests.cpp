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

    // BAD recovery (user decision): a BAD, an early BAD or a POOR settles the note as a BAD with zero accuracy and
    // no combo; the GOOD..MAX that answers an early BAD or a POOR is marked as a recovery and counts as a BAD.
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
            Require(session.Statistics().poorCount == 1 && session.Statistics().judgements[Miss] == 1 && bad(session.Statistics()) == 2,
                    "A POOR left without a retry is a MISS.");
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
            Require(session.Statistics().judgements[0] == 1 && bad(session.Statistics()) == 1,
                    "Large-note inputs still count one by one.");
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
            Require(hit != answered.events.end() && hit->recovery && session.Statistics().judgements[Bad] == 1,
                    "The right colour after that POOR is a BAD recovery (POOR 1 + BAD 1).");
            Require(session.Statistics().combo == 0 && session.Statistics().poorCount == 1,
                    "The POOR note adds no combo.");
        }
        {
            // A Kat press 70 ms after a Don note (late BAD window) is a POOR; leaving the note is a MISS.
            auto loaded = TaikoMode{}.CreateSession(pattern({1}));
            Require(loaded.Succeeded(), "Late wrong-colour fixture must load.");
            auto &session = *loaded.session;
            const auto wrong = rejected(session.ProcessInput('K', InputEdge::Pressed, RhythmTime{70'000}));
            Require(wrong.wrongPress && IsPoor(wrong) && session.Statistics().poorCount == 1,
                    "A wrong colour inside the late BAD window is a POOR.");
            static_cast<void>(session.Update(RhythmTime{500'000}));
            Require(session.Statistics().judgements[Miss] == 1 && session.Statistics().judgements[Bad] == 0,
                    "A note left after a late POOR is a MISS.");
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
            Require(session.Statistics().judgements[Miss] == 1 && session.Statistics().poorCount == 1 &&
                        session.Statistics().combo == 1,
                    "The late POOR note is a MISS; the next note starts a new combo.");
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
            // Every tick taken on time: the roll still ends at its tail, as before.
            auto loaded = TaikoMode{}.CreateSession(MakeLongPattern(TaikoNoteType::TickRoll, {"TickDivision=16"}));
            Require(loaded.Succeeded(), "Clean tick-roll fixture must load.");
            for (std::int64_t i = 0; i <= 8; ++i)
                static_cast<void>(loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{i * Interval}));
            const auto ended = loaded.session->Update(RhythmTime{Tail});
            Require(HasEvent(ended, NoteEventType::Completed) && ticks(ended) == 9 &&
                        loaded.session->AccuracyRate() == 1.0,
                    "A roll with every tick taken ends at its tail with full accuracy.");
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
