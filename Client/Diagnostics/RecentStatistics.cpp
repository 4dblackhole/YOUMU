#include "Diagnostics/RecentStatistics.h"

#include "Diagnostics/GameLog.h"

#include <cstdio>
#include <cstring>

namespace
{
    constexpr int EnterAttempts = 4000;
}

bool RecentStatistics::Enter() const noexcept
{
    for (int attempt = 0; attempt < EnterAttempts; ++attempt)
    {
        if (!busy_.test_and_set(std::memory_order_acquire))
            return true;
        YieldProcessor();
    }
    return false;
}

void RecentStatistics::Add(const StatisticsSample &sample) noexcept
{
    const bool entered = Enter();
    samples_[next_] = sample;
    next_ = (next_ + 1) % Capacity;
    count_ = std::min(count_ + 1, Capacity);
    if (entered)
        busy_.clear(std::memory_order_release);
}

std::size_t RecentStatistics::Snapshot(std::array<StatisticsSample, Capacity> &samples) const noexcept
{
    const bool entered = Enter();
    const std::size_t count = std::min(count_, Capacity);
    const std::size_t oldest = (next_ + Capacity - count) % Capacity;
    for (std::size_t index = 0; index < count; ++index)
        samples[index] = samples_[(oldest + index) % Capacity];
    if (entered)
        busy_.clear(std::memory_order_release);
    return count;
}

void RecentStatistics::WriteToLog() const noexcept
{
    // Static: a handler that runs on a nearly exhausted stack still has room.
    // Two threads ending the run at once would only interleave their lines.
    static std::array<StatisticsSample, Capacity> samples;
    const std::size_t count = Snapshot(samples);
    char line[256];
    int length = std::snprintf(line, sizeof(line), "recent statistics, %u seconds, oldest first:\r\n",
                               static_cast<unsigned>(count));
    if (length > 0)
        game_log::WriteRaw(line, static_cast<std::size_t>(length));
    for (std::size_t index = 0; index < count; ++index)
    {
        const StatisticsSample &sample = samples[index];
        length = std::snprintf(
            line, sizeof(line),
            "  %5us fps %4u ups %6u draws %3u/%3u longest %7u us vram %4u/%4u MB memory %5u MB handles %5u %s\r\n",
            sample.second, sample.framesPerSecond, sample.updatesPerSecond, sample.drawCalls, sample.peakDrawCalls,
            sample.longestLoopMicroseconds, sample.graphicsMemoryMb, sample.graphicsBudgetMb,
            sample.processMemoryMb, sample.handles, sample.scene);
        if (length > 0)
            game_log::WriteRaw(line, std::min(static_cast<std::size_t>(length), sizeof(line) - 1));
    }
    game_log::Flush();
}

void SetScene(StatisticsSample &sample, const std::string_view name) noexcept
{
    const std::size_t length = std::min(name.size(), sizeof(sample.scene) - 1);
    std::memcpy(sample.scene, name.data(), length);
    sample.scene[length] = '\0';
}
