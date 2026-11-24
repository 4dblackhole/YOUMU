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
    // BAD recovery: an accepted hit (or Buzz head) that came after an early BAD or a POOR on the same
    // timing target. It shows its own grade but settles as a BAD.
    bool recovery{};
    // A miss of a note last left at an early BAD: it settles as a BAD, not a MISS, with no new word shown.
    bool leftAtEarlyBad{};
    // A rejected press the note would refuse even on time (a wrong colour), within the BAD window before or
    // after the note: a POOR whatever its timing grade (user decision).
    bool wrongPress{};
};

// POOR: a rejected press within GOOD, or a wrong colour's press anywhere within BAD. The note keeps waiting.
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

    [[nodiscard]] AccuracyScore Score() const noexcept;
    [[nodiscard]] double ScoreRate() const noexcept { return Score().Rate(); }
    // True once any hit or tick of the note was accepted.
    [[nodiscard]] bool HasProgress() const noexcept { return acceptedHits > 0 || acceptedTicks > 0; }
    // The grade a finished long note shows: Miss without any progress, otherwise its score
    // rate placed on the timing grades' score boundaries (Max 100%, Perfect 99%, Great 90%).
    [[nodiscard]] JudgementGrade CompletionGrade() const noexcept;
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
