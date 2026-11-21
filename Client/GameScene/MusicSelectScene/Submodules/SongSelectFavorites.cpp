#include "MusicSelectView.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

namespace
{
    // What a collection file says about a chart: the song's first name and artist and the pattern name.
    SongCollectionChart CollectionLabel(const SongCatalogEntry &song, const SongCatalogPattern &pattern)
    {
        SongCollectionChart label;
        if (!song.music.names.empty()) label.title = song_select::DecodeDisplayText(song.music.names.front());
        if (!song.music.artists.empty()) label.artist = song_select::DecodeDisplayText(song.music.artists.front());
        label.pattern = song_select::DecodeDisplayText(pattern.pattern.name);
        return label;
    }
}

void MusicSelectView::OpenFavoritesPanel()
{
    SetSearchFocus(false);
    std::string chartHash;
    SongCollectionChart label;
    std::wstring name;
    if (!selection_.catalogLoading_ && selection_.focusedSongPosition_ < selection_.visibleSongIndices_.size())
    {
        const auto &song = selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
        if (selection_.selectedPatternIndex_ < song.patterns.size())
        {
            const auto &entry = song.patterns[selection_.selectedPatternIndex_];
            chartHash = entry.chartHash;
            label = CollectionLabel(song, entry);
            name = song_select::SongTitle(song, selection_.notation_) + L" / " +
                   song_select::PatternName(entry, selection_.notation_);
        }
    }
    // Charts moved over from favorites.json have no label yet; the catalog names the ones it has.
    std::set<std::string> unlabeled;
    for (const auto &folder : selection_.favorites_.Values().folders)
        for (const auto &[hash, kept] : folder.charts)
            if (kept == SongCollectionChart{}) unlabeled.insert(hash);
    std::map<std::string, SongCollectionChart> known;
    for (const auto &song : selection_.catalog_.songs)
        for (const auto &entry : song.patterns)
            if (unlabeled.contains(entry.chartHash)) known.emplace(entry.chartHash, CollectionLabel(song, entry));
    favoritesPanel_.Open(selection_.favorites_, std::move(chartHash), std::move(label), std::move(name), known);
}
void MusicSelectView::SetCollectionFilePicker(CollectionFilePicker picker)
{
    favoritesPanel_.SetFilePicker(std::move(picker));
}
bool MusicSelectView::ProcessFavoritesPanel(const InputState &input)
{
    if (!favoritesPanel_.IsVisible()) return false;
    for (const auto &action : canvas_->TakeActions())
        if (favoritesPanel_.ProcessAction(action)) inputRouter_.Reset(*canvas_);
    favoritesPanel_.ProcessInput(input, canvasPointer_);
    if (auto candidate = favoritesPanel_.TakeApplied())
    {
        std::string error;
        if (selection_.favorites_.Save(std::move(*candidate), error))
        {
            favoritesPanel_.Close();
            RebuildVisibleSongs();
        }
        else favoritesPanel_.ShowError(std::move(error));
    }
    inputRouter_.InvalidateHitTest();
    if (!favoritesPanel_.IsVisible()) inputRouter_.Reset(*canvas_);
    return true;
}
