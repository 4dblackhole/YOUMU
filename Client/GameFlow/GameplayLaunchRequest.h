#pragma once

#include "App/AudioVolumes.h"
#include "Catalog/SongCatalog.h"
#include "Catalog/SongLayout.h"
#include "Mode/PlayOptions.h"
#include "Common/RhythmTypes.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Value snapshot for one scene entry. Shared transfer is encapsulated by
// GameplayLaunchStore; an active session owns its copy.
struct GameplayLaunchRequest
{
    std::filesystem::path patternPath;
    std::optional<std::filesystem::path> effectPath;
    std::filesystem::path musicPath;
    std::string mode;
    // Display values decoded from the same catalog selection as the paths.
    // Result/retry uses this snapshot rather than reopening metadata files.
    std::wstring songTitle;
    std::wstring patternCreator;
    // Freeze settings for this attempt, including a Result retry.
    std::int32_t globalOffsetMs{};
    AudioVolumes volumes;
    std::map<std::string, std::vector<std::uint16_t>> inputKeys;
    std::wstring songArtist;
    std::filesystem::path songsRoot;
    std::filesystem::path backgroundPath;
    double backgroundOffsetX{}, backgroundOffsetY{};

    PlayOptions playOptions;
    std::wstring patternTitle;
    // Editor only: create a difficulty for this song; patternPath stays empty until Create.
    std::shared_ptr<const SongCatalogEntry> editorNewDifficulty;

    // The song folder this request works in (SongLayout): the pattern's own, or the song a
    // new difficulty will be created in. Empty when the files are outside a song folder.
    [[nodiscard]] std::filesystem::path SongFolder() const
    {
        return editorNewDifficulty ? SongLayout::SongFolderOfMusic(editorNewDifficulty->metadataPath)
                                   : SongLayout::SongFolderOfPattern(patternPath);
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !patternPath.empty() && !musicPath.empty();
    }

    void Clear() noexcept
    {
        patternPath.clear();
        effectPath.reset();
        musicPath.clear();
        mode.clear();
        songTitle.clear();
        patternCreator.clear();
        patternTitle.clear();
        editorNewDifficulty.reset();
        globalOffsetMs = 0;
        volumes = {};
        playOptions = {};
        inputKeys.clear();
        songArtist.clear();
        songsRoot.clear();
        backgroundPath.clear();
        backgroundOffsetX = backgroundOffsetY = 0;
    }
};
