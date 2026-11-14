#include "Note/Submodules/NoteSoundPolicy.h"
#include "Note/Submodules/RuleHelpers.h"
#include <algorithm>
#include <format>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace
{
// An unset binding field matches every event.
template <typename Expected, typename Actual>
[[nodiscard]] bool MatchesIfSet(const std::optional<Expected> &expected, const Actual &actual) noexcept
{
    return !expected.has_value() || expected == actual;
}
} // namespace

MappedNoteSoundPolicy &MappedNoteSoundPolicy::Bind(SoundBinding binding)
{
    bindings_.push_back(std::move(binding));
    return *this;
}

void MappedNoteSoundPolicy::AppendAudioCues(const NoteEvent &event,
                                            std::vector<AudioCueRequest> &output) const
{
    for (const SoundBinding &binding : bindings_)
    {
        if (!Matches(binding, event))
        {
            continue;
        }
        AudioCueRequest cue = binding.cue;
        cue.timelineTime = event.eventTime;
        output.push_back(std::move(cue));
        if (binding.stopAfterMatch)
        {
            break;
        }
    }
}

bool MappedNoteSoundPolicy::Matches(const SoundBinding &binding,
                                    const NoteEvent &event) noexcept
{
    if (binding.eventType != event.type || !MatchesIfSet(binding.inputAction, event.inputAction))
    {
        return false;
    }
    if (!MatchesIfSet(binding.stateBefore, event.stateBefore) ||
        !MatchesIfSet(binding.stateAfter, event.stateAfter))
    {
        return false;
    }
    if (!MatchesIfSet(binding.hitIndex, event.hitIndex) || !MatchesIfSet(binding.tickIndex, event.tickIndex))
    {
        return false;
    }
    return !binding.maximumGrade.has_value() ||
           IsAtLeastAsAccurateAs(event.judgement.grade, *binding.maximumGrade);
}
