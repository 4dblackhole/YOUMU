#include "Note/Submodules/RuleBasedNote.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace
{
[[nodiscard]] bool IsRejectedGoodInput(const NoteEvent &event) noexcept
{
    return event.type == NoteEventType::InputRejected &&
           IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good);
}
} // namespace

RuleBasedNote::RuleBasedNote(const NoteId id, const RhythmTime timing,
                             std::shared_ptr<const AccuracyRange> profile,
                             std::unique_ptr<INoteRule> rule,
                             std::shared_ptr<const INoteSoundPolicy> soundPolicy)
    : id_(id), timing_(timing), profile_(std::move(profile)), rule_(std::move(rule)),
      soundPolicy_(std::move(soundPolicy))
{
    if (profile_ == nullptr || rule_ == nullptr)
    {
        throw std::invalid_argument("A rule-based note requires a profile and input rule.");
    }
    accuracy_.noteId = id_;
    accuracy_.target = rule_->AccuracyTarget();
    if (accuracy_.target.kind == NoteAccuracyKind::TimingHits ||
        accuracy_.target.kind == NoteAccuracyKind::Hold)
    {
        accuracy_.hitScoreRates.resize(accuracy_.target.hits);
    }
}

RuleBasedNote::~RuleBasedNote() = default;

const NoteAccuracy &RuleBasedNote::Accuracy() const noexcept
{
    return accuracy_;
}

NoteId RuleBasedNote::Id() const noexcept
{
    return id_;
}

RhythmTime RuleBasedNote::Timing() const noexcept
{
    return timing_;
}

RhythmTime RuleBasedNote::ExpireTime() const noexcept
{
    return rule_->ExpireTime(Context());
}

NoteState RuleBasedNote::State() const noexcept
{
    return rule_->State();
}

std::optional<NoteProgress> RuleBasedNote::Progress() const noexcept
{
    return rule_->Progress();
}

std::size_t RuleBasedNote::ProcessedTickCount() const noexcept
{
    return rule_->ProcessedTickCount();
}

#if defined(_DEBUG)
std::wstring RuleBasedNote::DebugText() const
{
    std::wstring result =
        std::format(L"Note #{}  {}  timing={} us  expire={} us", id_, StateName(State()),
                    timing_.count(), ExpireTime().count());
    if (const std::optional<NoteProgress> progress = Progress())
    {
        result += std::format(L"  progress={}/{}", progress->accepted, progress->required);
    }
    return result + L"\n" + accuracy_.DebugText();
}
#endif

const AccuracyRange &RuleBasedNote::Profile() const noexcept
{
    return *profile_;
}

JudgementResult RuleBasedNote::Preview(const RhythmInputEvent &input) const noexcept
{
    // Count-only intervals and a hold's body have no head timing grade.
    // Once accepted, the head must never gate or grade subsequent body input.
    if (accuracy_.target.kind == NoteAccuracyKind::HitCount ||
        (accuracy_.target.kind == NoteAccuracyKind::Hold && accuracy_.acceptedHits != 0))
        return {JudgementGrade::Unjudged, {}, 1.0};
    return profile_->Evaluate(timing_, input.time);
}

bool RuleBasedNote::CanAccept(const RhythmInputEvent &input) const noexcept
{
    const JudgementResult judgement = Preview(input);
    return rule_->CanAccept(Context(), input, judgement);
}

NoteProcessResult RuleBasedNote::ProcessInput(const RhythmInputEvent &input)
{
    NoteProcessResult result;
    const JudgementResult judgement = Preview(input);
    const bool judgesTiming = JudgesTiming();
    const bool retrying = retryAfterEarlyBad_ || retryAfterPoor_;
    rule_->ProcessInput(Context(), input, judgement, result);
    if (judgesTiming && input.edge == InputEdge::Pressed && std::ranges::any_of(result.events, IsRejectedGoodInput))
    {
        poorOnTimingTarget_ = true;
        retryAfterPoor_ = true;
        retryAfterEarlyBad_ = false;
    }
    MarkTimingOutcome(result, judgesTiming, retrying);
    for (NoteEvent &event : result.events)
    {
        if (event.type == NoteEventType::HitAccepted ||
            event.type == NoteEventType::TickAccepted)
        {
            event.inputAction = input.action;
        }
    }
    AccumulateAccuracy(result);
    AppendAudioCues(result);
    return result;
}

NoteProcessResult RuleBasedNote::Update(const NoteUpdateContext &update)
{
    NoteProcessResult result;
    rule_->Update(Context(), update, result);
    AccumulateAccuracy(result);
    AppendAudioCues(result);
    return result;
}

NoteProcessResult RuleBasedNote::MarkMissed(const RhythmTime time)
{
    NoteProcessResult result;
    rule_->MarkMissed(Context(), time, result);
    AccumulateAccuracy(result);
    AppendAudioCues(result);
    return result;
}

bool RuleBasedNote::JudgesTiming() const noexcept
{
    // Timing notes judge every hit; a Buzz judges only its head.
    return accuracy_.target.kind == NoteAccuracyKind::TimingHits ||
           (accuracy_.target.kind == NoteAccuracyKind::Hold && accuracy_.acceptedHits == 0);
}

void RuleBasedNote::MarkTimingOutcome(NoteProcessResult &result, const bool judgedTiming, const bool retrying)
{
    if (!judgedTiming)
        return;
    for (NoteEvent &event : result.events)
    {
        const bool head = event.type == NoteEventType::HitAccepted || event.type == NoteEventType::HoldStarted;
        if (!head)
            continue;
        // The hit that answers an early BAD or a POOR is a BAD recovery; a late BAD is a BAD by itself.
        event.recovery = retrying;
        if (retrying || event.judgement.grade == JudgementGrade::Bad)
            badOnTimingTarget_ = true;
    }
    const bool answered = std::ranges::any_of(result.events, [](const NoteEvent &event) {
        return event.type == NoteEventType::HitAccepted;
    });
    if (answered)
        retryAfterEarlyBad_ = retryAfterPoor_ = false;
}

void RuleBasedNote::RecordEarlyBad() noexcept
{
    if (!JudgesTiming())
        return;
    badOnTimingTarget_ = true;
    retryAfterEarlyBad_ = true;
    retryAfterPoor_ = false;
}

void RuleBasedNote::Reset() noexcept
{
    rule_->Reset();
    std::ranges::fill(accuracy_.hitScoreRates, 0.0);
    accuracy_.acceptedHits = 0;
    accuracy_.acceptedTicks = 0;
    accuracy_.finalized = false;
    accuracy_.rejectedTiming = false;
    poorOnTimingTarget_ = badOnTimingTarget_ = false;
    retryAfterEarlyBad_ = retryAfterPoor_ = false;
}

void RuleBasedNote::AccumulateAccuracy(NoteProcessResult &result)
{
    if (accuracy_.finalized)
    {
        return;
    }
    for (NoteEvent &event : result.events)
    {
        const bool rejected = poorOnTimingTarget_ || badOnTimingTarget_;
        if (event.type == NoteEventType::HitAccepted)
        {
            if (event.hitIndex < accuracy_.hitScoreRates.size())
            {
                accuracy_.hitScoreRates[event.hitIndex] = rejected ? 0.0 : event.judgement.scoreRate;
            }
            ++accuracy_.acceptedHits;
        }
        else if (event.type == NoteEventType::TickAccepted)
        {
            if (accuracy_.target.kind == NoteAccuracyKind::HitCount)
            {
                ++accuracy_.acceptedHits;
            }
            else
            {
                ++accuracy_.acceptedTicks;
            }
        }
        else if (event.type == NoteEventType::Completed || event.type == NoteEventType::Missed)
        {
            // A BAD, early BAD or POOR settles the timing target (every hit, or only a Buzz head) at zero.
            if (rejected)
                std::ranges::fill(accuracy_.hitScoreRates, 0.0);
            accuracy_.rejectedTiming = rejected;
            event.leftAtEarlyBad = event.type == NoteEventType::Missed && retryAfterEarlyBad_;
            accuracy_.finalized = true;
            result.finalizedAccuracies.push_back(accuracy_);
            break;
        }
    }
}

NoteRuleContext RuleBasedNote::Context() const noexcept
{
    return {id_, timing_, *profile_};
}

void RuleBasedNote::AppendAudioCues(NoteProcessResult &result) const
{
    if (soundPolicy_ == nullptr)
    {
        return;
    }
    for (const NoteEvent &event : result.events)
    {
        soundPolicy_->AppendAudioCues(event, result.audioCues);
    }
}
