#pragma once

#include "App/AudioVolumes.h"
#include "App/ControlKeys.h"
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

class IGameplayTimingProbe;

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
    // This pattern's local offset when the attempt was entered (LocalOffsetStore); added to the global one.
    std::int32_t localOffsetMs{};
    AudioVolumes volumes;
    std::map<std::string, std::vector<std::uint16_t>> inputKeys;
    std::uint16_t restartKey{DefaultRestartKey}; // Held for half a second to restart the attempt.
    int keyBeamOpacity{100}; // Percent applied to the lane key beam of every mode.
    int backgroundBrightness{100}; // Percent of the background image's brightness (0 = black).
    std::wstring songArtist;
    std::filesystem::path songsRoot;
    std::filesystem::path backgroundPath;
    double backgroundOffsetX{}, backgroundOffsetY{};

    PlayOptions playOptions;
    std::wstring patternTitle;
    // Editor only: create a difficulty for this song; patternPath stays empty until Create.
    std::shared_ptr<const SongCatalogEntry> editorNewDifficulty;
    // Measurement only (Tests/Presentation/GameplayTimingProbe.cpp): a non-owning observer of
    // the clock anchors, clock reads and immediate cues. The game never sets it.
    IGameplayTimingProbe *timingProbe{};

    // The song folder this request works in (SongLayout): the pattern's own, or the song a
    // new difficulty will be created in. Empty when the files are outside a song folder.
    [[nodiscard]] std::filesystem::path SongFolder() const
    {
        return editorNewDifficulty ? SongLayout::SongFolderOfMusic(editorNewDifficulty->metadataPath)
                                   : SongLayout::SongFolderOfPattern(patternPath);
    }

    // The offset compiled into the session's note and effect times.
    [[nodiscard]] RhythmDuration CompiledOffset() const noexcept
    {
        return RhythmDuration{(static_cast<std::int64_t>(globalOffsetMs) + localOffsetMs) * 1000};
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
        timingProbe = nullptr;
        globalOffsetMs = 0;
        localOffsetMs = 0;
        volumes = {};
        playOptions = {};
        inputKeys.clear();
        restartKey = DefaultRestartKey;
        keyBeamOpacity = 100;
        backgroundBrightness = 100;
        songArtist.clear();
        songsRoot.clear();
        backgroundPath.clear();
        backgroundOffsetX = backgroundOffsetY = 0;
    }
};
