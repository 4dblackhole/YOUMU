#include "YOUMULogoScene.h"
#include "Submodules/LogoView.h"
#include "GameFlow/SongLibrary.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionText.h"
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;

YOUMULogoScene::YOUMULogoScene(ScreenVisual2DManager &visuals,
                                         TextCatalog &texts,
                                         OptionSettings &options,
                                         std::shared_ptr<SongLibrary> library)
    : library_(std::move(library)), view_(std::make_unique<LogoView>(visuals, texts, options))
{
    if (!library_)
        throw std::invalid_argument("The title requires a song library.");
}
YOUMULogoScene::~YOUMULogoScene() = default;
void YOUMULogoScene::Initialize(const EngineServices &services)
{
    // The first title entry starts scanning the Songs folder.
    library_->Initialize(services.audio);
    view_->Initialize(services);
}
void YOUMULogoScene::BeginScene()
{
    keepPlaybackOnExit_ = false;
    view_->BeginScene();
    ShowPlayer();
}
void YOUMULogoScene::EndScene() noexcept
{
    if (!keepPlaybackOnExit_)
        library_->ReleaseAudio();
    keepPlaybackOnExit_ = false;
    view_->EndScene();
}
void YOUMULogoScene::Update(const UpdateContext &context,
                                 SceneManager &scenes)
{
    if (library_->SongsRootChanged())
        library_->RefreshCatalog();
    library_->PollCatalog();
    library_->ChooseTitleTrack();
    ShowPlayer();
    const auto command = view_->Update(context.input, context.deltaSeconds);
    if (command.player)
        ApplyPlayerRequest(*command.player);
    library_->UpdatePlayback(context.deltaSeconds, SongLibrary::Screen::Title);
    ShowPlayer();
    if (command.menu)
        RunMenuItem(*command.menu, scenes);
}

void YOUMULogoScene::ApplyPlayerRequest(const TitlePlayerRequest &request)
{
    auto &player = library_->Player();
    switch (request.action)
    {
    case TitlePlayerAction::TogglePlay:
        player.TogglePlay();
        break;
    case TitlePlayerAction::Stop:
        player.Stop();
        break;
    case TitlePlayerAction::Next:
        library_->StepTrack(1);
        break;
    case TitlePlayerAction::Previous:
        library_->PreviousTrack();
        break;
    case TitlePlayerAction::Seek:
        if (const auto duration = player.State().DurationMicroseconds())
            player.Seek(static_cast<std::uint64_t>(std::llround(static_cast<double>(request.seekRatio) *
                                                                static_cast<double>(*duration))));
        break;
    }
}

void YOUMULogoScene::ShowPlayer()
{
    TitlePlayerDisplay display;
    const auto *song = library_->FocusedSong();
    if (song != nullptr)
    {
        const auto &state = library_->Player().State();
        display.hasSong = true;
        display.title = song_select::SongTitle(*song);
        display.artist = song_select::SongArtist(*song);
        display.artwork = library_->FocusedArtwork();
        display.playing = state.IsPlaying();
        display.positionMicroseconds = state.PositionMicroseconds();
        display.durationMicroseconds = state.DurationMicroseconds();
    }
    view_->SetPlayer(display);
}

void YOUMULogoScene::RunMenuItem(const std::size_t index, SceneManager &scenes)
{
    constexpr std::size_t GameStartIndex = 0, EditorIndex = 1, OptionIndex = 2, ExitIndex = 3;

    if (index == GameStartIndex)
    {
        // The song heard here stays selected and audible in song select.
        keepPlaybackOnExit_ = true;
        if (!scenes.ChangeScene(LobbySceneId))
        {
            throw std::runtime_error("Failed to enter the song select Scene.");
        }
        return;
    }
    if (index == EditorIndex)
    {
        if (!scenes.ChangeScene(EditorSongSelectSceneId))
        {
            throw std::runtime_error("Failed to enter the editor song-select Scene.");
        }
        return;
    }
    if (index == OptionIndex)
    {
        return;
    }
    if (index == ExitIndex)
    {
        scenes.Quit();
    }
}
void YOUMULogoScene::Render(const RenderContext &)
{
}
void YOUMULogoScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    view_->OnResize(width, height);
}
void YOUMULogoScene::Shutdown() noexcept
{
    library_->Shutdown();
    view_->Shutdown();
}
