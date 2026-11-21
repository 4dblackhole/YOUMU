#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

// Per-pattern local offsets in milliseconds (osu!-style), added to the global offset when a
// play session is compiled. One UTF-8 line per pattern: "<ms>\t<pattern key>", where the key is
// the pattern's normalized absolute path in lowercase (the same identity as the play records).
// Zero offsets are not stored. A missing file means every offset is zero.
class LocalOffsetStore final
{
  public:
    explicit LocalOffsetStore(std::filesystem::path file) : file_(std::move(file)) {}
    // Zero for an unknown pattern or an unreadable file.
    [[nodiscard]] std::int32_t Get(const std::filesystem::path &pattern) const;
    // Rewrites the file atomically (temporary file, then replace). False with an error message on failure.
    [[nodiscard]] bool Set(const std::filesystem::path &pattern, std::int32_t milliseconds, std::string &error) const;
    // Values are clamped to this range in both directions.
    static constexpr std::int32_t Limit = 1000;

  private:
    std::filesystem::path file_;
};
