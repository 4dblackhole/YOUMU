#pragma once

#include "Model/Submodules/MusicDocument.h"
#include "Model/Submodules/PatternDocument.h"
#include "Model/Submodules/EffectDocument.h"

#include <filesystem>
#include <string_view>

// Parses one chart file or text at a time. A load that may be cancelled checks between files (see
// Parsing/LoadCancellation.h); parsing one file is short and is not interrupted.
class ChartParser final
{
public:
    [[nodiscard]] ParseResult<MusicDocument> ParseMusicFile(
        const std::filesystem::path& path) const;
    [[nodiscard]] ParseResult<PatternDocument> ParsePatternFile(
        const std::filesystem::path& path) const;
    [[nodiscard]] ParseResult<EffectDocument> ParseEffectFile(
        const std::filesystem::path& path) const;

    [[nodiscard]] ParseResult<MusicDocument> ParseMusic(
        std::string_view utf8,
        std::filesystem::path source = {}) const;
    [[nodiscard]] ParseResult<PatternDocument> ParsePattern(
        std::string_view utf8,
        std::filesystem::path source = {}) const;
    [[nodiscard]] ParseResult<EffectDocument> ParseEffect(
        std::string_view utf8,
        std::filesystem::path source = {}) const;
};

[[nodiscard]] bool TryParseMusicalPosition(
    std::string_view value,
    std::int64_t implicitMeasure,
    MusicalPosition& output) noexcept;
