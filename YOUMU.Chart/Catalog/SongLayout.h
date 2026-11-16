#pragma once
#include <filesystem>
#include <string_view>

// The on-disk layout of one song inside a Songs root:
//
//   Songs/<song folder>/
//     Music/                 exactly one YMM and the audio it names
//     Chart/<chart set>/     YMP/YME files and the files they reference
//     BG/                    backgrounds named by a YMP's BG field
//
// A YMP belongs to the song folder two levels above its own folder, so its YMM,
// audio and backgrounds are found from the YMP path alone, without searching Songs.
struct SongLayout final
{
    static constexpr std::wstring_view MusicFolder = L"Music";
    static constexpr std::wstring_view ChartFolder = L"Chart";
    static constexpr std::wstring_view BackgroundFolder = L"BG";

    enum class MetadataStatus
    {
        Found,
        Missing,
        Multiple,
    };
    struct Metadata
    {
        MetadataStatus status{MetadataStatus::Missing};
        std::filesystem::path path; // Set only when Found.
    };

    // Empty unless the path is <song folder>/Chart/<chart set>/<file>.
    [[nodiscard]] static std::filesystem::path SongFolderOfPattern(const std::filesystem::path &patternPath);
    // Empty unless the path is <song folder>/Music/<file>.
    [[nodiscard]] static std::filesystem::path SongFolderOfMusic(const std::filesystem::path &metadataPath);
    // The single YMM directly inside <song folder>/Music. An unreadable folder counts as Missing.
    [[nodiscard]] static Metadata FindMusicMetadata(const std::filesystem::path &songFolder);
    [[nodiscard]] static Metadata FindMusicMetadataOfPattern(const std::filesystem::path &patternPath);
    // Empty when either argument is empty.
    [[nodiscard]] static std::filesystem::path BackgroundPath(const std::filesystem::path &songFolder,
                                                              const std::filesystem::path &backgroundFile);
    [[nodiscard]] static std::filesystem::path BackgroundPathOfPattern(const std::filesystem::path &patternPath,
                                                                       const std::filesystem::path &backgroundFile);
    // "<first> - <second>" as one filename component, from UTF-8 text. Song folders and YMM
    // files use the first artist and first title; chart sets use the first maker and the
    // pattern name. Characters a filename cannot hold become '_' and an empty part "Unknown".
    // The layout only needs the folder levels, so these names are a convention for people.
    [[nodiscard]] static std::filesystem::path FolderName(std::string_view first, std::string_view second);
    // One part of such a name, as UTF-8 text: the same replacements, without the outer spaces.
    [[nodiscard]] static std::string NamePart(std::string_view text);
};
