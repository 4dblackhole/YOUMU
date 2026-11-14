#include "MusicPlayerState.h"
#include <algorithm>
#include <cmath>

namespace
{
    // A seek never lands on the final frame: the audio backend rejects positions at or past the end.
    constexpr std::uint64_t EndMarginMicroseconds = 1'000;
}

void MusicPlayerState::BeginTrack() noexcept
{
    transport_ = MusicTransport::Playing;
    position_ = 0;
    duration_.reset();
}

void MusicPlayerState::Play() noexcept
{
    transport_ = MusicTransport::Playing;
}

void MusicPlayerState::Pause() noexcept
{
    if (transport_ == MusicTransport::Playing)
        transport_ = MusicTransport::Paused;
}

void MusicPlayerState::TogglePlay() noexcept
{
    if (IsPlaying())
        Pause();
    else
        Play();
}

void MusicPlayerState::Stop() noexcept
{
    transport_ = MusicTransport::Stopped;
    position_ = 0;
}

void MusicPlayerState::Seek(const std::uint64_t positionMicroseconds) noexcept
{
    position_ = ClampToTrack(positionMicroseconds);
    // A stopped song seeked away from its start is simply paused there.
    if (transport_ == MusicTransport::Stopped && position_ > 0)
        transport_ = MusicTransport::Paused;
}

void MusicPlayerState::Advance(const double seconds) noexcept
{
    if (!IsPlaying() || !(seconds > 0.0))
        return;
    const auto elapsed = static_cast<std::uint64_t>(std::llround(seconds * 1'000'000.0));
    position_ = duration_ ? std::min(position_ + elapsed, *duration_) : position_ + elapsed;
}

void MusicPlayerState::SetDuration(const std::optional<std::uint64_t> durationMicroseconds) noexcept
{
    duration_ = durationMicroseconds;
    if (duration_)
        position_ = std::min(position_, *duration_);
}

std::uint64_t MusicPlayerState::ClampToTrack(const std::uint64_t position) const noexcept
{
    if (!duration_)
        return position;
    const auto last = *duration_ > EndMarginMicroseconds ? *duration_ - EndMarginMicroseconds : 0;
    return std::min(position, last);
}

std::optional<std::size_t> StepPlaylist(const std::size_t count, const std::size_t current, const int delta) noexcept
{
    if (count == 0)
        return std::nullopt;
    const auto size = static_cast<long long>(count);
    const auto start = static_cast<long long>(std::min(current, count - 1));
    const auto shifted = (start + static_cast<long long>(delta) % size + size) % size;
    return static_cast<std::size_t>(shifted);
}

std::wstring FormatPlayerTime(const std::uint64_t microseconds)
{
    const auto totalSeconds = microseconds / 1'000'000;
    const auto seconds = totalSeconds % 60;
    return std::to_wstring(totalSeconds / 60) + (seconds < 10 ? L":0" : L":") + std::to_wstring(seconds);
}
