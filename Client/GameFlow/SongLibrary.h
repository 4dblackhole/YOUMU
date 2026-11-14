#pragma once
#include "PhantomCore.h"
#include "App/OptionSettings.h"
#include "GameScene/MusicSelectScene/Submodules/SongCatalogLoader.h"
#include "GameScene/MusicSelectScene/Submodules/SongPreviewController.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include <filesystem>
#include <random>

// The song list, its selection (search, filter, sort, folder, song and pattern) and the song
// being heard. The game owns one that the title player and the game song select share, so a
// song keeps playing and stays selected between them; editor song select keeps its own.
// Whichever screen is active polls it, so the catalog and the audio advance on either.
class SongLibrary final
{
  public:
    // The title moves on to the next song when one ends; song select repeats it.
    enum class Screen : std::uint8_t
    {
        Title,
        SongSelect,
    };

    SongLibrary(PhantomCore::AudioPlaybackManager &playback, OptionSettings &options);
    SongLibrary(const SongLibrary &) = delete;
    SongLibrary &operator=(const SongLibrary &) = delete;

    // The first call, from whichever screen opens first, starts scanning the Songs folder.
    void Initialize(PhantomCore::AudioSystem &audio);
    void LoadFavorites();
    // Scans the Songs folder again. The same folder keeps the current list and selection while
    // fresh entries replace them; a different folder starts from an empty list and forgets the song.
    // Returns false when the same folder is already being scanned.
    bool RefreshCatalog();
    [[nodiscard]] bool SongsRootChanged() const;
    // Publishes scanned songs. Returns true when the list changed.
    bool PollCatalog();
    // The title starts on a random song of the playlist the first time one is available.
    void ChooseTitleTrack();
    // Next or previous song of the playlist (song select's visible list), wrapping at either end.
    void StepTrack(int delta);
    // After three seconds the previous-song key restarts the current song instead.
    void PreviousTrack();
    // Keeps the focused song audible and advances it; at its end the title steps to the next song.
    void UpdatePlayback(double deltaSeconds, Screen screen);
    // Leaving for loading, play, results or the editor silences the song but keeps it selected.
    void ReleaseAudio() noexcept;
    void Shutdown() noexcept;

    [[nodiscard]] const SongCatalogEntry *FocusedSong() const noexcept;
    // The selected (or else first) pattern's background, which the title shows as album art.
    [[nodiscard]] std::filesystem::path FocusedArtwork() const;
    [[nodiscard]] SongSelectionState &Selection() noexcept { return selection_; }
    [[nodiscard]] const SongSelectionState &Selection() const noexcept { return selection_; }
    [[nodiscard]] SongPreviewController &Player() noexcept { return player_; }
    [[nodiscard]] const SongPreviewController &Player() const noexcept { return player_; }
    [[nodiscard]] const std::filesystem::path &CatalogRoot() const noexcept { return catalogRoot_; }

  private:
    OptionSettings &options_;
    SongSelectionState selection_;
    SongPreviewController player_;
    SongCatalogLoader catalogLoader_;
    std::filesystem::path catalogRoot_;
    std::mt19937 random_{std::random_device{}()};
    bool initialized_{};
    // Set once a song was picked for the title or heard in song select; a new Songs folder clears it.
    bool trackChosen_{};
};
