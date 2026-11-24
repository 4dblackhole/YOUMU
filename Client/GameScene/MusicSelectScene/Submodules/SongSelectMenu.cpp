#include "MusicSelectView.h"
#include "Catalog/SongLayout.h"
#include "SongSelectionText.h"

using namespace PhantomCore;
using namespace song_select;

namespace
{
    // The confirmation's buttons: Delete sits apart at the left; Cancel takes Enter and Esc.
    constexpr std::size_t DeleteButton = 0;
}

bool MusicSelectView::ProcessSongMenu(const InputState &input)
{
    const bool rightPressed = input.WasMouseButtonPressed(MouseButton::Right);
    const bool canOpen = rightPressed && !options_.IsVisible() && !selection_.catalogLoading_;
    if (canOpen)
    {
        // A record row has no menu: the right-click asks at once whether to delete that record (user request).
        if (const auto record = records_.RecordAt(inputRouter_.HoveredNode()))
        {
            AskToDeleteRecord(*record);
            static_cast<void>(canvas_->TakeActions());
            return true;
        }
        if (const auto target = songList_.ContextTargetAt(inputRouter_.HoveredNode()))
        {
            OpenSongMenu(*target);
            static_cast<void>(canvas_->TakeActions());
            return true;
        }
    }
    if (!songMenu_.IsOpen())
        return false;
    for (const Action &action : canvas_->TakeActions())
    {
        if (const auto item = songMenu_.ProcessAction(action))
        {
            RunSongMenuItem(*item);
            inputRouter_.Reset(*canvas_);
            return true;
        }
    }
    // A press anywhere but the menu, or Esc, closes it and does nothing else.
    const bool pressed = input.WasMouseButtonPressed(MouseButton::Left) || rightPressed;
    const bool elsewhere = pressed && !songMenu_.Contains(inputRouter_.HoveredNode());
    if (elsewhere || input.WasKeyPressed(VK_ESCAPE))
    {
        songMenu_.Close();
        inputRouter_.Reset(*canvas_);
    }
    return true;
}

void MusicSelectView::OpenSongMenu(const SongListView::ContextTarget &target)
{
    SetSearchFocus(false);
    tooltip_.Hide();
    // A right-click selects what it lands on, like a left click, without starting it.
    if (target.visiblePosition != selection_.focusedSongPosition_)
        SelectVisibleSong(target.visiblePosition);
    if (target.pattern && *target.pattern != selection_.selectedPatternIndex_)
        SelectPattern(*target.pattern);
    RefreshSearchPresentation();
    const bool onDifficulty = target.pattern.has_value();
    std::vector<SongMenuItem> items{SongMenuItem::NewDifficulty};
    // The editor's song select opens a difficulty with a click already.
    if (onDifficulty && purpose_ == SongSelectPurpose::Gameplay)
        items.push_back(SongMenuItem::Edit);
    if (onDifficulty)
        items.push_back(SongMenuItem::DeleteDifficulty);
    items.push_back(SongMenuItem::DeleteSong);
    items.push_back(SongMenuItem::OpenFolder);
    const auto *song = FocusedSong();
    if (song != nullptr && selection_.selectedPatternIndex_ < song->patterns.size())
        items.push_back(SongMenuItem::Collections);
    songMenu_.Open(canvasPointer_.value_or(Point{}), std::move(items));
}

const SongCatalogEntry *MusicSelectView::FocusedSong() const noexcept
{
    if (selection_.focusedSongPosition_ >= selection_.visibleSongIndices_.size())
        return nullptr;
    return &selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
}

void MusicSelectView::RunSongMenuItem(const SongMenuItem item)
{
    const auto *song = FocusedSong();
    if (song == nullptr)
        return;
    switch (item)
    {
    case SongMenuItem::NewDifficulty:
        command_ = SongSelectCommand::CreateDifficulty;
        return;
    case SongMenuItem::Edit:
        command_ = SongSelectCommand::EditPattern;
        return;
    case SongMenuItem::Collections:
        OpenFavoritesPanel();
        return;
    case SongMenuItem::OpenFolder:
        folderToOpen_ = SongLayout::SongFolderOfMusic(song->metadataPath);
        command_ = SongSelectCommand::OpenFolder;
        return;
    case SongMenuItem::DeleteDifficulty:
        AskToDelete(*song, selection_.selectedPatternIndex_);
        return;
    case SongMenuItem::DeleteSong:
        AskToDelete(*song, std::nullopt);
        return;
    }
}

void MusicSelectView::AskToDelete(const SongCatalogEntry &song, const std::optional<std::size_t> pattern)
{
    const auto text = [this](const char *key) { return std::wstring(texts_.Get(key)); };
    const auto root = settings_.SongsRoot();
    try
    {
        auto deletion = pattern ? PlanDifficultyDeletion(song, *pattern, root) : PlanSongDeletion(song, root);
        // The detail names what goes: the song (and difficulty), then the paths under Songs.
        std::wstring name = SongTitle(song, selection_.notation_);
        if (pattern)
            name += L" [" + PatternName(song.patterns[*pattern], selection_.notation_) + L"]";
        AlertContent content{AlertKind::Warning,
                             text(pattern ? "SongMenu.deleteDifficultyTitle" : "SongMenu.deleteSongTitle"),
                             text(pattern ? "SongMenu.deleteDifficultyMessage" : "SongMenu.deleteSongMessage"),
                             name + L"\n" + DeletionListing(deletion, root),
                             {{text("SongMenu.delete"), false, true, false}, {text("SongMenu.cancel"), true, false, true}}};
        ShowAlert(std::move(content));
        pendingDeletion_ = std::move(deletion);
    }
    catch (const std::exception &error)
    {
        ShowAlert({AlertKind::Error, text("SongMenu.deleteFailedTitle"), text("SongMenu.deleteFailedMessage"),
                   DecodeDisplayText(error.what()), {{text("Alert.ok"), true}}});
    }
}

void MusicSelectView::AskToDeleteRecord(const PlayRecord &record)
{
    const auto text = [this](const char *key) { return std::wstring(texts_.Get(key)); };
    // The detail names the chart, then what tells this play apart from the others.
    std::wstring chart;
    const auto *song = FocusedSong();
    if (song != nullptr && selection_.selectedPatternIndex_ < song->patterns.size())
        chart = SongTitle(*song, selection_.notation_) + L" [" +
                PatternName(song->patterns[selection_.selectedPatternIndex_], selection_.notation_) + L"]\n";
    ShowAlert({AlertKind::Warning, text("Records.deleteTitle"), text("Records.deleteMessage"),
               chart + records_.DescribeRecord(record),
               {{text("SongMenu.delete"), false, true, false}, {text("SongMenu.cancel"), true, false, true}}});
    pendingRecordDeletion_ = record.id;
}

void MusicSelectView::AnswerDeletion(const std::optional<std::size_t> chosen)
{
    if (!chosen)
        return;
    // Only Delete deletes; Cancel, Enter and Esc leave the record.
    if (*chosen == DeleteButton && pendingRecordDeletion_)
        records_.DeleteRecord(*pendingRecordDeletion_);
    pendingRecordDeletion_.reset();
    if (!pendingDeletion_)
        return;
    if (*chosen == DeleteButton)
    {
        approvedDeletion_ = std::move(pendingDeletion_);
        command_ = SongSelectCommand::DeleteFiles;
    }
    pendingDeletion_.reset();
}
