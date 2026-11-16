#include "SongLibrary.h"
#include "Catalog/SongLayout.h"

using namespace PhantomCore;

SongLibrary::SongLibrary(AudioPlaybackManager &playback, OptionSettings &options)
    : options_(options), player_(playback)
{
    selection_.favorites_ = SongFavorites{options.Path().parent_path() / L"favorites.json"};
}

void SongLibrary::Initialize(AudioSystem &audio)
{
    if (initialized_)
        return;
    initialized_ = true;
    player_.Initialize(audio);
    LoadFavorites();
    RefreshCatalog();
}

void SongLibrary::LoadFavorites()
{
    std::string error;
    static_cast<void>(selection_.favorites_.Load(error));
}

bool SongLibrary::RefreshCatalog()
{
    const auto root = options_.SongsRoot();
    if (!catalogLoader_.Refresh(root))
        return false;
    selection_.catalogLoading_ = true;
    if (root == catalogRoot_)
        return true;
    catalogRoot_ = root;
    player_.StopPreviewAudio();
    trackChosen_ = false;
    selection_.catalog_ = {};
    selection_.RebuildVisibleSongs();
    return true;
}

bool SongLibrary::SongsRootChanged() const
{
    return catalogRoot_ != options_.SongsRoot();
}

bool SongLibrary::PollCatalog()
{
    bool changed = false;
    if (auto entries = catalogLoader_.PollProgress(); !entries.empty())
    {
        selection_.MergeCatalogProgress(std::move(entries));
        changed = true;
    }
    if (auto catalog = catalogLoader_.Poll())
    {
        // The song keeps playing: the player follows its audio path, not a catalog index.
        selection_.FinishCatalog(std::move(*catalog));
        selection_.ClearLaunchError();
        const bool noSongs = selection_.catalog_.songs.empty();
        if (noSongs && !selection_.catalog_.diagnostics.empty())
            selection_.SetLaunchError(SongSelectionState::LaunchErrorKind::Catalog,
                                      selection_.catalog_.diagnostics.front().message);
        changed = true;
    }
    return changed;
}

void SongLibrary::ChooseTitleTrack()
{
    const auto count = selection_.visibleSongIndices_.size();
    if (trackChosen_ || count == 0)
        return;
    trackChosen_ = true;
    std::uniform_int_distribution<std::size_t> position(0, count - 1);
    selection_.SelectVisibleSong(position(random_));
}

void SongLibrary::StepTrack(const int delta)
{
    const auto next = StepPlaylist(selection_.visibleSongIndices_.size(), selection_.focusedSongPosition_, delta);
    if (!next)
        return;
    selection_.SelectVisibleSong(*next);
    const auto *song = FocusedSong();
    if (song == nullptr)
        return;
    // A one-song playlist, or two songs sharing a file, start the same audio over.
    if (song->audioPath == player_.CurrentTrack())
        player_.RestartTrack();
    else
        player_.SyncTrack(song->audioPath, true);
}

void SongLibrary::PreviousTrack()
{
    if (player_.State().PreviousRestartsTrack())
    {
        player_.RestartTrack();
        return;
    }
    StepTrack(-1);
}

void SongLibrary::UpdatePlayback(const double deltaSeconds, const Screen screen)
{
    const auto *song = FocusedSong();
    const bool songSelect = screen == Screen::SongSelect;
    if (songSelect && song != nullptr)
        trackChosen_ = true;
    player_.SetVolumeScale(options_.Values().volumes.Music());
    player_.SyncTrack(song != nullptr ? song->audioPath : std::filesystem::path{}, songSelect);
    const auto endBehavior = songSelect ? SongPreviewController::EndBehavior::Repeat
                                        : SongPreviewController::EndBehavior::Report;
    if (player_.UpdatePreviewAudio(deltaSeconds, endBehavior))
        StepTrack(1);
}

void SongLibrary::ReleaseAudio() noexcept
{
    player_.ReleaseAudio();
}

void SongLibrary::Shutdown() noexcept
{
    catalogLoader_.Cancel();
    player_.Shutdown();
    selection_.catalog_ = {};
    selection_.visibleSongIndices_.clear();
    selection_.focusedSongPosition_ = 0;
}

const SongCatalogEntry *SongLibrary::FocusedSong() const noexcept
{
    const auto position = selection_.focusedSongPosition_;
    if (position >= selection_.visibleSongIndices_.size())
        return nullptr;
    const auto index = selection_.visibleSongIndices_[position];
    return index < selection_.catalog_.songs.size() ? &selection_.catalog_.songs[index] : nullptr;
}

std::filesystem::path SongLibrary::FocusedArtwork() const
{
    const auto *song = FocusedSong();
    if (song == nullptr || song->patterns.empty())
        return {};
    const auto index = selection_.selectedPatternIndex_ < song->patterns.size() ? selection_.selectedPatternIndex_ : 0;
    const auto &pattern = song->patterns[index];
    return SongLayout::BackgroundPathOfPattern(pattern.patternPath, pattern.pattern.backgroundFile);
}
