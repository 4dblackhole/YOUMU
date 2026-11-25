#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

// One second of the run, as the engine's performance sample and the process
// itself report it.
struct StatisticsSample
{
    std::uint32_t second{}; // since the engine started
    std::uint32_t framesPerSecond{};
    std::uint32_t updatesPerSecond{};
    std::uint32_t drawCalls{};
    std::uint32_t peakDrawCalls{};
    // The longest single pass of the main loop in that second, in microseconds.
    std::uint32_t longestLoopMicroseconds{};
    std::uint32_t graphicsMemoryMb{};
    std::uint32_t graphicsBudgetMb{};
    std::uint32_t processMemoryMb{};
    std::uint32_t handles{};
    // The Scene on screen, cut to fit and always terminated.
    char scene[24]{};
};

// The last two minutes of samples, kept in memory. A normal run never writes
// them anywhere; when the run ends badly they go to the log, so the file shows
// what led up to the end without costing a disk write per second.
//
// The game loop adds; a crash handler or the stall watch may read from another
// thread at any moment. Nothing here allocates.
class RecentStatistics final
{
  public:
    static constexpr std::size_t Capacity = 120;

    void Add(const StatisticsSample &sample) noexcept;
    // Copies the kept samples, oldest first, and returns how many there are.
    [[nodiscard]] std::size_t Snapshot(std::array<StatisticsSample, Capacity> &samples) const noexcept;
    // One log line per sample, oldest first, under a heading. Safe in a
    // handler that must not allocate.
    void WriteToLog() const noexcept;

  private:
    // Held for a few nanoseconds. A reader that cannot get it (the adding
    // thread died inside Add) reads anyway rather than wait forever.
    [[nodiscard]] bool Enter() const noexcept;

    mutable std::atomic_flag busy_;
    std::array<StatisticsSample, Capacity> samples_{};
    std::size_t next_{};
    std::size_t count_{};
};

// The log line of one sample, ending in "\r\n", written into line without
// allocating. Returns its length, 0 when it does not fit.
[[nodiscard]] std::size_t FormatSample(const StatisticsSample &sample, char *line, std::size_t size) noexcept;

// Copies name into sample.scene, cut to fit.
void SetScene(StatisticsSample &sample, std::string_view name) noexcept;
