#pragma once
#include <filesystem>
#include <string>
#include <string_view>

// A chart's identity outside its path: the SHA-256 of the pattern file's bytes as 64 lowercase hex
// digits. Collections keep charts by it so a shared collection finds the same charts in another
// player's Songs folder. Editing the file gives it a new hash.
[[nodiscard]] std::string ChartBytesHash(std::string_view bytes);
// Throws std::runtime_error when the file cannot be read.
[[nodiscard]] std::string ChartFileHash(const std::filesystem::path &path);
// An audio file's identity, in the same form: the SHA-256 of its bytes, read in chunks. Nothing writes it
// into song files (FMT-01); the osu! import names the song folders it creates with it. Throws
// std::runtime_error when the file cannot be read.
[[nodiscard]] std::string AudioFileHash(const std::filesystem::path &path);
// 64 hex digits, either case.
[[nodiscard]] bool IsChartHash(std::string_view text) noexcept;
