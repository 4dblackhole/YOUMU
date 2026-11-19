#include "MusicSelectScene.h"
#include "App/AssetPaths.h"
#include "Catalog/SongLayout.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "Mode/PlayGameModeFactory.h"
#include "Submodules/MusicSelectView.h"
#include "GameFlow/SongLibrary.h"
#include "Submodules/SongSelectionText.h"
#include <stdexcept>

using namespace PhantomCore;

using namespace song_select;

namespace
{
    [[nodiscard]] SongLibrary &RequireLibrary(const std::shared_ptr<SongLibrary> &library)
    {
        if (!library)
            throw std::invalid_argument("Song select requires a song library.");
        return *library;
    }
}

MusicSelectScene::MusicSelectScene(ScreenVisual2DManager &visuals,
                                   std::shared_ptr<SongLibrary> library,
                                   std::shared_ptr<GameplayLaunchStore> request, SongSelectPurpose purpose,
                                   TextCatalog &texts, OptionSettings &options)
    : launchRequest_(std::move(request)), purpose_(purpose), options_(options), texts_(texts),
      library_(std::move(library)), selection_(RequireLibrary(library_).Selection()),
      view_(std::make_unique<MusicSelectView>(visuals, selection_, purpose, texts, options))
{
}
MusicSelectScene::~MusicSelectScene() = default;

void MusicSelectScene::Initialize(const EngineServices &services)
{
    if (!launchRequest_)
        throw std::invalid_argument("Lobby requires a gameplay launch request.");
    // The title normally started the scan already; a direct entry (smoke routes) starts it here.
    library_->Initialize(services.audio);
    view_->Initialize(services);
}
void MusicSelectScene::BeginScene()
{
    library_->LoadFavorites();
    // Initialize started the first load. Re-entry still observes user edits.
    if (!firstEntry_ && !osuImport_.Busy())
    {
        RefreshCatalog();
    }
    firstEntry_ = false;
    view_->RebuildVisibleSongs();
    active_ = true;
    keepPlaybackOnExit_ = false;
    view_->SetVisible(true);
    // A song that failed to open is tried again on entry, not on every frame.
    if (library_->Player().TrackFailed())
        library_->Player().RestartTrack();
}
void MusicSelectScene::EndScene() noexcept
{
    active_ = false;
    pendingLaunch_.reset();
    patternLoader_.Cancel();
    if (!keepPlaybackOnExit_)
        library_->ReleaseAudio();
    keepPlaybackOnExit_ = false;
    view_->SetVisible(false);
}
void MusicSelectScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    if (library_->SongsRootChanged())
        RefreshCatalog();
    PollOsuImport();
    if (library_->PollCatalog())
        view_->RefreshCatalogPresentation();
    const auto command = view_->Update(context.input, context.deltaSeconds);
    library_->UpdatePlayback(context.deltaSeconds, SongLibrary::Screen::SongSelect);
    // Length appears once the focused song's audio has opened.
    std::optional<std::uint64_t> duration;
    const auto *focused = library_->FocusedSong();
    const bool focusedHeard = focused != nullptr && focused->audioPath == library_->Player().CurrentTrack();
    if (focusedHeard)
        duration = library_->Player().State().DurationMicroseconds();
    view_->SetNowPlaying(focusedHeard && library_->Player().State().IsPlaying());
    if (duration != selection_.selectedDurationMicroseconds_)
    {
        selection_.selectedDurationMicroseconds_ = duration;
        view_->RefreshSelectionPresentation();
    }
    if (command == SongSelectCommand::Back)
    {
        // The game song select hands its song back to the title; the editor's own library stops.
        keepPlaybackOnExit_ = purpose_ == SongSelectPurpose::Gameplay;
        if (!scenes.ChangeScene(LogoSceneId))
            throw std::runtime_error("Failed to return to the title Scene.");
    }
    else if (command == SongSelectCommand::Launch)
        StartSelectedPattern(scenes);
    else if (command == SongSelectCommand::CreateDifficulty)
        CreateDifficulty(scenes);
    else if (command == SongSelectCommand::ConvertOsu)
        ConvertOsu();
    if (command != SongSelectCommand::Back)
        PollPatternLoad(scenes);
}
void MusicSelectScene::ConvertOsu()
{
    if (purpose_ != SongSelectPurpose::Editor || selection_.catalogLoading_ || osuImport_.Busy()) return;
    library_->ReleaseAudio();
    try
    {
        if (auto folder = SelectOsuFolder(texts_))
        {
            osuImportFolder_ = folder->filename().native();
            // The scan converts into a temporary folder; Songs changes only after the user chose.
            osuImport_.StartScan(*folder, library_->CatalogRoot());
            selection_.catalogLoading_ = true;
            selection_.ClearLaunchError();
            view_->RebuildVisibleSongs();
        }
    }
    catch (const std::exception &error)
    {
        selection_.SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern, error.what());
        view_->RefreshSelectionPresentation();
    }
}
void MusicSelectScene::PollOsuImport()
{
    if (auto scan = osuImport_.PollScan())
        ShowOsuScan(std::move(*scan));
    if (const auto choice = view_->TakeOsuImportChoice())
        AnswerOsuImport(*choice);
    if (const auto result = osuImport_.PollPlacement())
        FinishOsuImport(*result);
}
void MusicSelectScene::ShowImportAlert(const char *messageKey, const std::string &detail)
{
    view_->ShowAlert({AlertKind::Error, std::wstring(texts_.Get("MusicSelect.importFailedTitle")),
                      std::wstring(texts_.Get(messageKey)), DecodeDisplayText(detail),
                      {{std::wstring(texts_.Get("Alert.ok")), true}}});
}
void MusicSelectScene::ShowOsuScan(OsuImportScan scan)
{
    // Nothing reached Songs yet, so the song cards only unlock again.
    selection_.catalogLoading_ = false;
    view_->RebuildVisibleSongs();
    if (scan.success)
    {
        view_->OpenOsuImportPanel(osuImportFolder_, std::move(scan.difficulties));
        return;
    }
    osuImport_.Discard();
    ShowImportAlert("MusicSelect.importFailedMessage", scan.message);
}
void MusicSelectScene::AnswerOsuImport(const OsuImportChoice &choice)
{
    if (!choice.import)
    {
        osuImport_.Discard();
        return;
    }
    osuImport_.StartPlacement(choice.indices);
    selection_.catalogLoading_ = true;
    view_->RebuildVisibleSongs();
}
void MusicSelectScene::FinishOsuImport(const EditorOsuImportResult &result)
{
    // Imported difficulties appear in the list without a completion message.
    if (result.changed)
        RefreshCatalog();
    else
    {
        selection_.catalogLoading_ = false;
        view_->RebuildVisibleSongs();
    }
    // The alert carries one line per difficulty that failed.
    if (!result.success)
        ShowImportAlert(result.changed ? "MusicSelect.importPartialMessage" : "MusicSelect.importFailedMessage",
                        result.message);
}
void MusicSelectScene::CreateDifficulty(SceneManager &scenes)
{
    if (purpose_ != SongSelectPurpose::Editor || selection_.catalogLoading_ ||
        selection_.focusedSongPosition_ >= selection_.visibleSongIndices_.size())
        return;
    const auto &song = selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
    library_->ReleaseAudio();
    // No file exists until the editor's Create; leaving before it leaves the song unchanged.
    GameplayLaunchRequest request{{}, {}, song.audioPath, "Taiko", SongTitle(song, selection_.notation_), L""};
    request.globalOffsetMs = options_.Values().globalOffsetMs;
    request.volumes = options_.Values().volumes;
    request.songsRoot = library_->CatalogRoot();
    request.songArtist = SongArtist(song, selection_.notation_);
    request.editorNewDifficulty = std::make_shared<const SongCatalogEntry>(song);
    selection_.ClearLaunchError();
    launchRequest_->Set(std::move(request));
    if (!scenes.ChangeScene(EditorSceneId))
        throw std::runtime_error("Failed to enter the editor scene.");
}
void MusicSelectScene::Render(const RenderContext &)
{
}
void MusicSelectScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void MusicSelectScene::Shutdown() noexcept
{
    patternLoader_.Cancel();
    library_->Shutdown();
    view_->Shutdown();
}

bool MusicSelectScene::StartSelectedPattern(SceneManager &scenes)
{
    if (osuImport_.Busy() || patternLoader_.Busy())
        return false;
    if (selection_.focusedSongPosition_ >= selection_.visibleSongIndices_.size())
    {
        return false;
    }
    const auto &song =
        selection_.catalog_
            .songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
    if (song.patterns.empty() || selection_.selectedPatternIndex_ >= song.patterns.size())
    {
        return false;
    }
    const auto &pattern = song.patterns[selection_.selectedPatternIndex_];

    // Validate both entry routes before a Scene factory can reject the request.
    if (!IsSupportedPlayGameMode(pattern.pattern.mode))
    {
        selection_.SetLaunchError(SongSelectionState::LaunchErrorKind::UnsupportedMode,
                                   pattern.pattern.mode);
        view_->RefreshSelectionPresentation();
        return false;
    }

    GameplayLaunchRequest request{
        pattern.patternPath, pattern.effectPath,
        song.audioPath,      pattern.pattern.mode,
        SongTitle(song, selection_.notation_), MakerName(pattern, selection_.notation_)};
    request.globalOffsetMs = options_.Values().globalOffsetMs;
    request.volumes = options_.Values().volumes;
    request.patternTitle = PatternName(pattern, selection_.notation_);
    request.restartKey = options_.Values().restartKey;
    request.playOptions = selection_.playOptions_;
    request.songsRoot = library_->CatalogRoot();
    request.backgroundPath =
        SongLayout::BackgroundPathOfPattern(pattern.patternPath, pattern.pattern.backgroundFile);
    request.backgroundOffsetX = pattern.pattern.backgroundOffsetX;
    request.backgroundOffsetY = pattern.pattern.backgroundOffsetY;
    request.songArtist = SongArtist(song, selection_.notation_);
    if (const auto keys = options_.Values().modeKeys.find(request.mode); keys != options_.Values().modeKeys.end())
        request.inputKeys = keys->second;
    if (purpose_ == SongSelectPurpose::Editor)
    {
        selection_.ClearLaunchError();
        launchRequest_->Set(request);
        if (!scenes.ChangeScene(EditorSceneId))
        {
            throw std::runtime_error("Failed to enter the editor scene.");
        }
        return true;
    }

    selection_.ClearLaunchError();
    pendingLaunch_ = request;
    patternLoader_.Start(std::move(request));
    return true;
}

void MusicSelectScene::PollPatternLoad(SceneManager &scenes)
{
    if (pendingLaunch_)
    {
        const auto position = selection_.focusedSongPosition_;
        const auto& indices = selection_.visibleSongIndices_;
        const auto* song = position < indices.size() ? &selection_.catalog_.songs[indices[position]] : nullptr;
        const auto pattern = selection_.selectedPatternIndex_;
        if (!song || pattern >= song->patterns.size() ||
            song->patterns[pattern].patternPath != pendingLaunch_->patternPath)
        {
            patternLoader_.Cancel();
            pendingLaunch_.reset();
            return;
        }
    }
    auto loaded = patternLoader_.Poll();
    if (!loaded || !pendingLaunch_)
        return;
    auto request = std::move(*pendingLaunch_);
    pendingLaunch_.reset();
    // Navigation remains responsive. A different selection cancels this launch.
    if (selection_.focusedSongPosition_ >= selection_.visibleSongIndices_.size())
        return;
    const auto &song = selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
    if (selection_.selectedPatternIndex_ >= song.patterns.size() ||
        song.patterns[selection_.selectedPatternIndex_].patternPath != request.patternPath)
        return;
    if (!loaded->Succeeded())
    {
        selection_.SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern,
            loaded->diagnostics.empty() ? std::string{} : loaded->diagnostics.front().message);
        view_->RefreshSelectionPresentation();
        return;
    }
    launchRequest_->SetValidated(std::move(request), std::move(loaded->session));
    if (!scenes.ChangeScene(RhythmTestSceneId))
    {
        throw std::runtime_error("Failed to enter transient gameplay.");
    }
}

void MusicSelectScene::RefreshCatalog()
{
    if (!library_->RefreshCatalog()) return;
    pendingLaunch_.reset();
    patternLoader_.Cancel();
    if (active_)
        view_->RebuildVisibleSongs();
}
