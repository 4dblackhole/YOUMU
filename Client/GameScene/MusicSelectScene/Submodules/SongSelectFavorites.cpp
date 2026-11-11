#include "MusicSelectView.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

void MusicSelectView::OpenFavoritesPanel()
{
    SetSearchFocus(false);
    CloseAdditionMenu();
    std::filesystem::path pattern;
    std::wstring name;
    if (!selection_.catalogLoading_ && selection_.focusedSongPosition_ < selection_.visibleSongIndices_.size())
    {
        const auto &song = selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
        if (selection_.selectedPatternIndex_ < song.patterns.size())
        {
            const auto &entry = song.patterns[selection_.selectedPatternIndex_];
            pattern = entry.patternPath;
            name = song_select::SongTitle(song) + L" / " + song_select::PatternName(entry);
        }
    }
    favoritesPanel_.Open(selection_.favorites_, pattern, std::move(name));
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
