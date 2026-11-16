#pragma once
#include "Note/Submodules/NoteTypes.h"
#include "PhantomCore.h"
#include <span>

// Read-only observer for timing measurements (Tests/Presentation/GameplayTimingProbe.cpp).
// The game never sets one. Every call only reports values the controller already
// computed; an observer must not change gameplay, audio or presentation state.
class IGameplayTimingProbe
{
  public:
    virtual ~IGameplayTimingProbe() = default;
    // QPC and the DSP clock were anchored to this same future instant, which plays timeline.
    virtual void OnClockAnchored(const PhantomCore::AudioClockSnapshot &anchor, RhythmTime timeline,
                                 double playbackRate) noexcept = 0;
    // One running gameplay update: its clock read and the chart time computed from that QPC.
    virtual void OnGameplayClock(const PhantomCore::AudioClockSnapshot &clock, RhythmTime time) noexcept = 0;
    // Input-driven cues, just before and just after the audio router starts them.
    virtual void BeforeImmediateCues(std::span<const AudioCueRequest> cues) noexcept = 0;
    virtual void AfterImmediateCues(std::span<const AudioCueRequest> cues) noexcept = 0;
};
