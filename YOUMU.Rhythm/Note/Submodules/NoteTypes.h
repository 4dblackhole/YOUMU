#pragma once

#include "Common/RhythmTypes.h"
#include "Judgement/AccuracyRange.h"
#include "Judgement/AccuracyScore.h"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

enum class NoteState : std::uint8_t
{
    Pending,
    Active,
    AwaitingAdditionalInput,
    Holding,
    Completed,
    Missed,
};

enum class InputEdge : std::uint8_t
{
    Pressed,
    Released,
};

struct RhythmInputEvent
{
    RhythmTime time{};
    NoteAction action{};
    PhysicalKey physicalKey{};
    InputEdge edge{InputEdge::Pressed};
    // Supplied by the input-state owner after applying this physical edge.
    // Direct rule users may instead let the rule track their local keys.
    std::optional<bool> actionHeldAfter;
};

enum class NoteEventType : std::uint8_t
{
    InputRejected,
    HitAccepted,
    StageAdvanced,
    HoldStarted,
    HoldReleased,
    TickAccepted,
    TickMissed,
    Completed,
    Missed,
    TickRejected,
};

struct NoteEvent
{
    NoteId noteId{};
    NoteEventType type{};
    NoteState stateBefore{NoteState::Pending};
    NoteState stateAfter{NoteState::Pending};
    JudgementResult judgement{};
    RhythmTime eventTime{};
    std::size_t hitIndex{};
    std::size_t tickIndex{};
    std::optional<NoteAction> inputAction;
    // Recovery: an accepted hit (or Buzz head) that came after an early BAD or a POOR on the same timing target.
    // It shows its own grade. After an early BAD it settles as a BAD; after a POOR the note's one POOR is its
    // whole result and the hit adds no count (user decision: POOR is a kind of BAD).
    bool recovery{};
    // A miss of a note last left at an early BAD: it settles as a BAD, not a MISS, with no new word shown.
    bool leftAtEarlyBad{};
    // A miss of a note that took a POOR: the POOR is its result, so no MISS is counted or shown (user decision).
    bool leftAtPoor{};
    // A rejected press the note would refuse even on time (a wrong colour), within the BAD window before or
    // after the note: a POOR whatever its timing grade (user decision).
    bool wrongPress{};
};

// POOR: a rejected press within GOOD, or a wrong colour's press anywhere within BAD. The note keeps waiting and
// takes only a hit of GOOD or better from then on. Every POOR press shows its word; a note counts one POOR.
[[nodiscard]] inline bool IsPoor(const NoteEvent &event) noexcept
{
    if (event.type != NoteEventType::InputRejected)
        return false;
    return event.wrongPress || IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good);
}

// The lane's early BAD: a press within BAD before the note, reported while the note keeps its focus.
// A wrong colour's press in that window is a POOR instead; the rule rejects it with a negative error.
[[nodiscard]] inline bool IsEarlyBad(const NoteEvent &event) noexcept
{
    if (event.type != NoteEventType::InputRejected || event.judgement.grade != JudgementGrade::Bad)
        return false;
    return !event.wrongPress && event.judgement.signedError < RhythmDuration::zero();
}

// A right-colour press within BAD after the note that the note did not take. Only a note that takes GOOD or
// better has one (big and purple notes, a Buzz head); a normal note takes that press as its BAD hit.
[[nodiscard]] inline bool IsLateBad(const NoteEvent &event) noexcept
{
    if (event.type != NoteEventType::InputRejected || event.judgement.grade != JudgementGrade::Bad)
        return false;
    return !event.wrongPress && event.judgement.signedError > RhythmDuration::zero();
}

// A BAD the note did not take as a hit, before or after it: the note is a BAD from then on.
[[nodiscard]] inline bool IsRejectedBad(const NoteEvent &event) noexcept
{
    return IsEarlyBad(event) || IsLateBad(event);
}

struct AudioCueRequest
{
    SoundId sound;
    AudioBusId bus{"HitSound"};
    RhythmTime timelineTime{};
    float volume{1.0F};
    float pitch{1.0F};
    float pan{};
    float reverbSend{};
    int priority{};
};

enum class NoteAccuracyKind : std::uint8_t
{
    TimingHits,
    HitCount,
    Ticks,
    Hold,
};

struct NoteAccuracyTarget
{
    NoteAccuracyKind kind{NoteAccuracyKind::TimingHits};
    std::size_t hits{1};
    std::size_t ticks{};
};

// A logical note contributes once, independently of its number of inputs.
struct NoteAccuracy
{
    NoteId noteId{};
    NoteAccuracyTarget target;
    std::vector<double> hitScoreRates;
    std::size_t acceptedHits{};
    std::size_t acceptedTicks{};
    bool finalized{};
    // The timing target (all hits, or a Buzz head) took a BAD, an early BAD or a POOR; its hit scores are zero.
    bool rejectedTiming{};
    // A settled timing note that took every hit with no BAD or POOR: the grade its average hit score reaches on
    // the note's profile. A big note counts this one grade (user decision). Unjudged otherwise.
    JudgementGrade averageGrade{JudgementGrade::Unjudged};

    [[nodiscard]] AccuracyScore Score() const noexcept;
    [[nodiscard]] double ScoreRate() const noexcept { return Score().Rate(); }
    // True once any hit or tick of the note was accepted.
    [[nodiscard]] bool HasProgress() const noexcept { return acceptedHits > 0 || acceptedTicks > 0; }
    // The grade a finished long note shows: Miss without any progress, otherwise its score
    // rate placed on the timing grades' score boundaries (Max 100%, Perfect 99%, Great 90%).
    [[nodiscard]] JudgementGrade CompletionGrade() const noexcept;
    // The note judges the timing of its next hit: every hit of a timing note, only the head of a Buzz. Rolls,
    // balloons and tick rolls never do, so a press before them is neither an early BAD nor a POOR.
    [[nodiscard]] bool JudgesTiming() const noexcept
    {
        if (target.kind == NoteAccuracyKind::TimingHits)
            return true;
        return target.kind == NoteAccuracyKind::Hold && acceptedHits == 0;
    }
#if defined(_DEBUG)
    [[nodiscard]] std::wstring DebugText() const;
#endif
};

struct NoteProcessResult
{
    std::vector<NoteEvent> events;
    std::vector<AudioCueRequest> audioCues;
    std::vector<NoteAccuracy> finalizedAccuracies;

    void Append(NoteProcessResult other);
};
