#include "Parsing/Submodules/ParserSupport.h"

namespace
{
    template <typename Document, typename Parse>
    ParseResult<Document> ParseFile(const std::filesystem::path &path, std::stop_token stop, Parse parse)
    {
        std::string text;
        try
        {
            text = ReadUtf8File(path, stop);
        }
        catch (const std::runtime_error &error)
        {
            ParseResult<Document> result;
            result.document.sourcePath = path;
            AddDiagnostic(result.diagnostics, path, 0, error.what());
            return result;
        }
        return parse(text, path);
    }
}

ParseResult<MusicDocument> ChartParser::ParseMusicFile(const std::filesystem::path &path) const
{
    return ParseFile<MusicDocument>(path, stop_, [this](const auto &text, const auto &source) { return ParseMusic(text, source); });
}

ParseResult<PatternDocument> ChartParser::ParsePatternFile(
    const std::filesystem::path &path) const
{
    return ParseFile<PatternDocument>(path, stop_, [this](const auto &text, const auto &source) { return ParsePattern(text, source); });
}

ParseResult<EffectDocument> ChartParser::ParseEffectFile(
    const std::filesystem::path &path) const
{
    return ParseFile<EffectDocument>(path, stop_, [this](const auto &text, const auto &source) { return ParseEffect(text, source); });
}
