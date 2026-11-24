#pragma once

#include <cstddef>

// How many of a preparation's files are ready. The loading screen's sky-blue bar fills by this count
// (user decision 2026-11-25: the chart, each play screen image and each sound file count one each).
struct FileProgress
{
    std::size_t ready{};
    std::size_t total{};

    FileProgress &operator+=(const FileProgress &other) noexcept
    {
        ready += other.ready;
        total += other.total;
        return *this;
    }
    // 0 while nothing is known yet.
    [[nodiscard]] float Fraction() const noexcept
    {
        return total == 0 ? 0.0F : static_cast<float>(ready) / static_cast<float>(total);
    }
};
