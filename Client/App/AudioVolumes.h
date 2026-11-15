#pragma once

// The player's volume settings in whole percent, saved in Option.ini. Each
// playback path multiplies its own gain by Music() or Effects(); Master scales
// both. The gain is linear in the percentage.
struct AudioVolumes
{
    static constexpr int Maximum = 100;
    int master{Maximum}, music{Maximum}, effects{Maximum};

    [[nodiscard]] static constexpr bool IsValidPercent(const int percent) noexcept
    {
        return percent >= 0 && percent <= Maximum;
    }
    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return IsValidPercent(master) && IsValidPercent(music) && IsValidPercent(effects);
    }
    // Song audio: gameplay music, song select preview, the title player and the editor's music.
    [[nodiscard]] constexpr float Music() const noexcept
    {
        return Gain(music);
    }
    // Every other sound: hit sounds, ticks, input feedback and the metronome.
    [[nodiscard]] constexpr float Effects() const noexcept
    {
        return Gain(effects);
    }
    friend constexpr bool operator==(const AudioVolumes &, const AudioVolumes &) = default;

  private:
    [[nodiscard]] constexpr float Gain(const int percent) const noexcept
    {
        return static_cast<float>(master) * static_cast<float>(percent) / static_cast<float>(Maximum * Maximum);
    }
};
