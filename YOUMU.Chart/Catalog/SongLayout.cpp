#include "Catalog/SongLayout.h"

#include <algorithm>
#include <cwctype>
#include <string>
#include <system_error>

namespace
{
    [[nodiscard]] bool SameName(const std::filesystem::path &name, const std::wstring_view expected)
    {
        const auto &text = name.native();
        return std::ranges::equal(text, expected, [](const auto left, const auto right) {
            return std::towlower(static_cast<wint_t>(left)) == std::towlower(static_cast<wint_t>(right));
        });
    }
} // namespace

std::string SongLayout::NamePart(const std::string_view text)
{
    std::string part(text);
    for (auto &character : part)
    {
        const bool control = static_cast<unsigned char>(character) < 32;
        if (control || std::string_view{"<>:\"/\\|?*"}.find(character) != std::string_view::npos)
            character = '_';
    }
    const auto first = part.find_first_not_of(' ');
    if (first == std::string::npos)
        return "Unknown";
    return part.substr(first, part.find_last_not_of(' ') - first + 1);
}

std::filesystem::path SongLayout::SongFolderOfPattern(const std::filesystem::path &patternPath)
{
    const auto chartFolder = patternPath.parent_path().parent_path();
    if (!chartFolder.has_parent_path() || !SameName(chartFolder.filename(), ChartFolder))
        return {};
    return chartFolder.parent_path();
}

std::filesystem::path SongLayout::SongFolderOfMusic(const std::filesystem::path &metadataPath)
{
    const auto musicFolder = metadataPath.parent_path();
    if (!musicFolder.has_parent_path() || !SameName(musicFolder.filename(), MusicFolder))
        return {};
    return musicFolder.parent_path();
}

SongLayout::Metadata SongLayout::FindMusicMetadata(const std::filesystem::path &songFolder)
{
    Metadata result;
    if (songFolder.empty())
        return result;
    std::error_code error;
    std::filesystem::directory_iterator iterator(songFolder / MusicFolder, error);
    for (const std::filesystem::directory_iterator end; !error && iterator != end; iterator.increment(error))
    {
        std::error_code typeError;
        if (!iterator->is_regular_file(typeError) || !SameName(iterator->path().extension(), L".ymm"))
            continue;
        if (result.status == MetadataStatus::Found)
            return {MetadataStatus::Multiple, {}};
        result = {MetadataStatus::Found, iterator->path()};
    }
    return error ? Metadata{} : result;
}

SongLayout::Metadata SongLayout::FindMusicMetadataOfPattern(const std::filesystem::path &patternPath)
{
    return FindMusicMetadata(SongFolderOfPattern(patternPath));
}

std::filesystem::path SongLayout::BackgroundPath(const std::filesystem::path &songFolder,
                                                 const std::filesystem::path &backgroundFile)
{
    if (songFolder.empty() || backgroundFile.empty())
        return {};
    return songFolder / BackgroundFolder / backgroundFile;
}

std::filesystem::path SongLayout::BackgroundPathOfPattern(const std::filesystem::path &patternPath,
                                                          const std::filesystem::path &backgroundFile)
{
    return BackgroundPath(SongFolderOfPattern(patternPath), backgroundFile);
}

std::filesystem::path SongLayout::FolderName(const std::string_view first, const std::string_view second)
{
    std::string name = NamePart(first) + " - " + NamePart(second);
    // Windows drops trailing dots, which would make the created folder differ from this name.
    // The first part never ends in a dot or space after " - ", so the name stays non-empty.
    while (name.back() == '.' || name.back() == ' ')
        name.pop_back();
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(name.data()), name.size()));
}
