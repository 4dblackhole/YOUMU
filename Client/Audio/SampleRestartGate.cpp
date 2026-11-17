#include "Audio/SampleRestartGate.h"

namespace
{
    [[nodiscard]] bool SameSettings(const SampleRestartGate::Request &left,
                                    const SampleRestartGate::Request &right) noexcept
    {
        return left.bus == right.bus && left.volume == right.volume && left.pitch == right.pitch;
    }

    // Both requests fall in one mixer period only when the clock is known and unchanged.
    [[nodiscard]] bool SameMixerPeriod(const SampleRestartGate::Request &previous,
                                       const SampleRestartGate::Request &request) noexcept
    {
        return request.dspClock != 0 && previous.dspClock == request.dspClock;
    }
}

bool SampleRestartGate::Admit(const Request &request)
{
    const auto found = applied_.find(request.voice);
    const bool repeated = found != applied_.end() && SameMixerPeriod(found->second, request);
    if (repeated && SameSettings(found->second, request))
        return false;
    Remember(request);
    return true;
}

void SampleRestartGate::Remember(const Request &request)
{
    applied_.insert_or_assign(request.voice, request);
}

void SampleRestartGate::Forget(const void *const voice) noexcept
{
    applied_.erase(voice);
}

void SampleRestartGate::Clear() noexcept
{
    applied_.clear();
}
