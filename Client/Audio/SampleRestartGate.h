#pragma once

#include <cstdint>
#include <unordered_map>

// Drops an immediate restart that cannot change what the mixer plays.
// A restart reaches a voice only when the mixer next mixes it, so until the DSP
// clock advances, restarting the same playing voice again with the same bus,
// volume and pitch leaves it exactly where the previous request put it. A
// request with any other setting goes through as before: the last request wins.
// Voices and buses are identified by address only; the gate never knows which
// note, mode or rule asked for the sound.
class SampleRestartGate final
{
  public:
    struct Request
    {
        const void *voice{};
        const void *bus{};
        float volume{};
        float pitch{};
        // The mixer clock read while routing this request; 0 means unknown.
        std::uint64_t dspClock{};
    };

    // True when the request must reach the voice; the gate then remembers it.
    [[nodiscard]] bool Admit(const Request &request);
    // Records a request that already reached its voice, such as a new voice.
    void Remember(const Request &request);
    // The voice was changed outside the gate, for example by a scheduled start.
    void Forget(const void *voice) noexcept;
    void Clear() noexcept;

  private:
    std::unordered_map<const void *, Request> applied_;
};
