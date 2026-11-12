#include "MusicSelectView.h"

using namespace PhantomCore;

void MusicSelectView::OpenRecordFilter()
{
    SetSearchFocus(false);
    CloseAdditionMenu();
    records_.OpenFilter();
}

bool MusicSelectView::ProcessRecordFilterPanel(const InputState& input)
{
    if (!records_.FilterVisible()) return false;
    const auto actions = canvas_->TakeActions();
    records_.UpdateFilter(actions, input);
    if (!records_.FilterVisible()) inputRouter_.Reset(*canvas_);
    else inputRouter_.InvalidateHitTest();
    return true;
}

void MusicSelectView::UpdateRecords()
{
    const SongCatalogPattern* selected = nullptr;
    if (selection_.focusedSongPosition_ < selection_.visibleSongIndices_.size())
    {
        const auto& song = selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
        if (selection_.selectedPatternIndex_ < song.patterns.size())
            selected = &song.patterns[selection_.selectedPatternIndex_];
    }
    records_.Update(selected);
}
