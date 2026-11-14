#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// Transport of the one song the title player and the game song select share.
// It holds no audio: the preview controller mirrors it onto an engine voice and
// advances the position by elapsed playback time.
enum class MusicTransport : std::uint8_t
{
    Stopped,
    Playing,
    Paused,
};

class MusicPlayerState final
{
  public:
    // Past this point the previous-song key restarts the current song instead.
    static constexpr std::uint64_t PreviousRestartMicroseconds = 3'000'000;

    // A new song starts playing from its beginning; its length is unknown until opened.
    void BeginTrack() noexcept;
    void Play() noexcept;
    void Pause() noexcept;
    void TogglePlay() noexcept;
    // Stop pauses and rewinds, unlike Pause, which keeps the position.
    void Stop() noexcept;
    void Seek(std::uint64_t positionMicroseconds) noexcept;
    void Advance(double seconds) noexcept;
    void SetDuration(std::optional<std::uint64_t> durationMicroseconds) noexcept;

    [[nodiscard]] MusicTransport Transport() const noexcept { return transport_; }
    [[nodiscard]] bool IsPlaying() const noexcept { return transport_ == MusicTransport::Playing; }
    [[nodiscard]] std::uint64_t PositionMicroseconds() const noexcept { return position_; }
    [[nodiscard]] std::optional<std::uint64_t> DurationMicroseconds() const noexcept { return duration_; }
    [[nodiscard]] bool PreviousRestartsTrack() const noexcept { return position_ > PreviousRestartMicroseconds; }

  private:
    [[nodiscard]] std::uint64_t ClampToTrack(std::uint64_t position) const noexcept;

    MusicTransport transport_{MusicTransport::Stopped};
    std::uint64_t position_{};
    std::optional<std::uint64_t> duration_;
};

// The visible position after stepping through a playlist of count songs; the ends wrap around.
[[nodiscard]] std::optional<std::size_t> StepPlaylist(std::size_t count, std::size_t current, int delta) noexcept;

// m:ss for the player's elapsed time and length; minutes keep counting past an hour.
[[nodiscard]] std::wstring FormatPlayerTime(std::uint64_t microseconds);
