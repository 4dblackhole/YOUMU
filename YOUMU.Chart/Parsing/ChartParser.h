#pragma once

#include "Parsing/LoadCancellation.h"
#include "Model/Submodules/MusicDocument.h"
#include "Model/Submodules/PatternDocument.h"
#include "Model/Submodules/EffectDocument.h"

#include <filesystem>
#include <string_view>

class ChartParser final
{
public:
    explicit ChartParser(std::stop_token stop = {}) : stop_(stop) {}

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
private:
    std::stop_token stop_;
};

[[nodiscard]] bool TryParseMusicalPosition(
    std::string_view value,
    std::int64_t implicitMeasure,
    MusicalPosition& output) noexcept;
