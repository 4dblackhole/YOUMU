#include "Parsing/Submodules/ParserSupport.h"

namespace
{
    template <typename Document, typename Parse>
    ParseResult<Document> ParseFile(const std::filesystem::path &path, Parse parse)
    {
        std::string text, error;
        if (!ReadUtf8File(path, text, error))
        {
            ParseResult<Document> result;
            result.document.sourcePath = path;
            AddDiagnostic(result.diagnostics, path, 0, error);
            return result;
        }
        return parse(text, path);
    }
}

ParseResult<MusicDocument> ChartParser::ParseMusicFile(const std::filesystem::path &path) const
{
    return ParseFile<MusicDocument>(path, [this](const auto &text, const auto &source) { return ParseMusic(text, source); });
}

ParseResult<PatternDocument> ChartParser::ParsePatternFile(
    const std::filesystem::path &path) const
{
    return ParseFile<PatternDocument>(path, [this](const auto &text, const auto &source) { return ParsePattern(text, source); });
}

ParseResult<EffectDocument> ChartParser::ParseEffectFile(
    const std::filesystem::path &path) const
{
    return ParseFile<EffectDocument>(path, [this](const auto &text, const auto &source) { return ParseEffect(text, source); });
}
