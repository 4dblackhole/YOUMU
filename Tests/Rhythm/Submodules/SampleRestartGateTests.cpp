#include "Audio/SampleRestartGate.h"
#include "TestSupport.h"

#include <cstdint>
#include <set>

namespace tests
{
    namespace
    {
        // Stand-ins for voices and buses; the gate compares addresses only. Not const, so the
        // linker cannot fold them into one address.
        int VoiceA{}, VoiceB{}, BusTick{}, BusHit{};

        SampleRestartGate::Request At(std::uint64_t dspClock, const void *voice = &VoiceA, const void *bus = &BusTick,
                                      float volume = 1.0F, float pitch = 1.0F)
        {
            return {voice, bus, volume, pitch, dspClock};
        }
    }

    void TestSampleRestartGateMixerPeriods()
    {
        SampleRestartGate gate;
        Require(gate.Admit(At(480)), "The first restart of a voice always reaches it.");
        Require(!gate.Admit(At(480)) && !gate.Admit(At(480)),
                "A repeated restart with the same settings before the mixer runs changes nothing and is dropped.");
        Require(gate.Admit(At(960)), "After the mixer clock advances the restart reaches the voice again.");

        Require(gate.Admit(At(960, &VoiceA, &BusTick, 0.5F)),
                "A different volume in the same mixer period is applied: the last request wins.");
        Require(!gate.Admit(At(960, &VoiceA, &BusTick, 0.5F)), "Repeating the new volume is dropped again.");
        Require(gate.Admit(At(960, &VoiceA, &BusTick, 0.5F, 1.5F)), "A different pitch is applied.");
        Require(gate.Admit(At(960, &VoiceA, &BusHit, 0.5F, 1.5F)), "A different bus is applied.");
        Require(gate.Admit(At(960, &VoiceB)), "Another voice is judged on its own.");

        Require(gate.Admit(At(0)) && gate.Admit(At(0)), "An unknown mixer clock never drops a restart.");

        gate.Remember(At(1440));
        Require(!gate.Admit(At(1440)), "A new voice started in this period already plays the request.");
        gate.Forget(&VoiceA);
        Require(gate.Admit(At(1440)), "A voice changed outside the gate is restarted again.");
        gate.Clear();
        Require(gate.Admit(At(1440)), "Clearing forgets every voice.");
    }

    // A 1024-division Buzz at 180 BPM asks for a tick restart every 1.302 ms while a WASAPI
    // mixer wakes every 10 ms (480 samples at 48 kHz). Only one restart per mix can be heard.
    void TestSampleRestartGateDenseTicks()
    {
        SampleRestartGate gate;
        std::set<std::uint64_t> periods;
        std::size_t admitted{};
        for (std::uint64_t tick = 0; tick < 512; ++tick)
        {
            const std::uint64_t microseconds = 1'000'000 + tick * 1'302;
            const std::uint64_t dspClock = microseconds / 10'000 * 480;
            periods.insert(dspClock);
            admitted += gate.Admit(At(dspClock)) ? 1U : 0U;
        }
        Require(admitted == periods.size(), "Each mixer period receives exactly one restart of the tick voice.");
        Require(admitted < 512 / 7, "Dense ticks reach the voice about once per mix instead of once per tick.");
    }
}
