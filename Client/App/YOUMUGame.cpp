#include "YOUMUGame.h"

#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "EditorScene/EditorScene.h"
#include "GameFlow/SongLibrary.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameScene/YOUMULogoScene/YOUMULogoScene.h"
#include "GameScene/MusicSelectScene/MusicSelectScene.h"
#include "GameScene/ResultScene/ResultScene.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Presentation/UiColors.h"
#include "Texts/GameIdentity.h"
#include "App/GameDataPaths.h"
#include "Records/PlayRecordStore.h"

#include <Windows.h>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

using namespace PhantomCore;

YOUMUGame::YOUMUGame(const bool smokeTest, std::string initialSceneId,
                               const bool rhythmDebugMode)
    : smokeTest_(smokeTest), showPerformanceOverlay_(smokeTest), rhythmDebugMode_(rhythmDebugMode),
      initialSceneId_(std::move(initialSceneId)),
      launchRequest_(std::make_shared<GameplayLaunchStore>()),
      optionSettings_(OptionSettings::NextToExecutable())
{
    std::string error;
    // Invalid/unwritable settings must not prevent startup. The options panel
    // retains the load warning until the user successfully saves new settings.
    if (!optionSettings_.Load(error))
        OutputDebugStringA(error.c_str());
    GameplayLaunchRequest initialRequest;
    initialRequest.songsRoot = optionSettings_.SongsRoot();
    launchRequest_->Set(std::move(initialRequest));
    texts_.SetLanguage(optionSettings_.Values().language);
    for (const auto &diagnostic : texts_.Diagnostics())
        OutputDebugStringA((diagnostic + "\n").c_str());
    SkinSetSelection::Instance().Initialize(optionSettings_.Values().skinSet);
}

EngineConfig YOUMUGame::GetEngineConfig() const
{
    EngineConfig config;
    config.windowTitle = GameName;
    config.windowWidth = optionSettings_.Values().windowWidth;
    config.windowHeight = optionSettings_.Values().windowHeight;
    const auto color = ui_colors::ClearBackground;
    config.clearColor = {color.red, color.green, color.blue, color.alpha};
    config.audio.preferredBackend = optionSettings_.Values().audioOutput;
    // Restore the named device after enumeration instead of trusting a stale index.
    config.audio.driverIndex = -1;
    config.audio.fallBackToWasapi = true;
    config.audio.allowNoSoundFallback = true;
    config.showWindow = !smokeTest_;
    config.autoExitAfterRenderedFrames = smokeTest_ ? 3 : 0;
    return config;
}

void YOUMUGame::RegisterScenes(SceneManager &scenes)
{
    if (smokeTest_ && initialSceneId_ == EditorSceneId)
    {
        const auto catalog = SongCatalog{}.Load(optionSettings_.SongsRoot());
        const auto song = std::ranges::find_if(
            catalog.songs, [](const auto &entry) { return !entry.patterns.empty(); });
        if (song == catalog.songs.end())
            throw std::runtime_error("Editor smoke requires an external Songs chart.");
        GameplayLaunchRequest smokeRequest{song->patterns.front().patternPath,
                                                        song->patterns.front().effectPath, song->audioPath,
                                                        song->patterns.front().pattern.mode};
        smokeRequest.songsRoot = optionSettings_.SongsRoot();
        launchRequest_->Set(std::move(smokeRequest));
    }
    // The title and the game song select share one song library, so a song keeps playing and
    // stays selected between them. Editor song select keeps its own list and preview.
    songLibrary_ = std::make_shared<SongLibrary>(AudioPlayback(), optionSettings_);
    auto editorSongLibrary = std::make_shared<SongLibrary>(AudioPlayback(), optionSettings_);
    // Registration stops at the first failure.
    const auto requireRegistered = [](const bool registered)
    {
        if (!registered)
            throw std::runtime_error("Failed to register the game Scenes.");
    };
    requireRegistered(scenes.RegisterScene<YOUMULogoScene>(std::string(LogoSceneId),
                                                           SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                           std::ref(texts_), std::ref(optionSettings_), songLibrary_));
    requireRegistered(scenes.RegisterScene<MusicSelectScene>(std::string(LobbySceneId),
                                                             SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                             songLibrary_, launchRequest_, SongSelectPurpose::Gameplay,
                                                             std::ref(texts_), std::ref(optionSettings_)));
    requireRegistered(scenes.RegisterScene<MusicSelectScene>(std::string(EditorSongSelectSceneId),
                                                             SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                             std::move(editorSongLibrary), launchRequest_,
                                                             SongSelectPurpose::Editor, std::ref(texts_),
                                                             std::ref(optionSettings_)));
    requireRegistered(scenes.RegisterScene<EditorScene>(std::string(EditorSceneId),
                                                        SceneRetention::DestroyOnExit, std::ref(ScreenVisuals()),
                                                        launchRequest_, std::ref(texts_)));
    requireRegistered(scenes.RegisterScene<RhythmTestScene>(std::string(RhythmTestSceneId),
                                                            // Gameplay routes keep only their factory while inactive.
                                                            // The concrete mode Scene is constructed on entry and
                                                            // destroyed as soon as it transfers its result.
                                                            SceneRetention::DestroyOnExit, launchRequest_,
                                                            std::ref(AudioPlayback()), std::ref(ScreenVisuals()),
                                                            rhythmDebugMode_, std::ref(texts_),
                                                            std::ref(pendingResult_)));
    requireRegistered(scenes.RegisterScene(std::string(ResultSceneId), SceneRetention::DestroyOnExit,
                                           [this] { return CreateResultScene(); }));
}

std::unique_ptr<GameScene> YOUMUGame::CreateResultScene()
{
    auto launch = launchRequest_->Snapshot();
    ResultViewContext display{launch.songTitle, launch.patternCreator, launch.IsValid(), launch.songArtist};
    display.backgroundPath = launch.backgroundPath;
    display.backgroundOffsetX = launch.backgroundOffsetX;
    display.backgroundOffsetY = launch.backgroundOffsetY;
    if (!smokeTest_ && !rhythmDebugMode_ && launch.IsValid()) display.recordDatabase = PlayRecordDatabasePath();
    ResultScene::RetryHandler retry;
    if (display.canRetry)
        retry = [store = launchRequest_, snapshot = std::move(launch)](SceneManager &scenes)
        {
            // A retry owns a fresh session; restore the played selection without
            // retaining any note, timer or voice from the completed session.
            store->Set(snapshot);
            return scenes.ChangeScene(RhythmTestSceneId);
        };
    return std::make_unique<ResultScene>(std::move(pendingResult_), ScreenVisuals(), texts_, std::move(display),
                                         std::move(retry));
}

std::string_view YOUMUGame::InitialSceneId() const noexcept
{
    return initialSceneId_;
}

void YOUMUGame::OnClientInitialized(const EngineServices &services)
{
    // This hook runs before the first update/render; output switching retains clips.
    RestoreSavedAudioDriver(services.audio);
    // The performance band is Client presentation, so the game owns it and
    // chooses F7 independently from Engine scheduling.
    performanceBand_.Initialize(ScreenVisuals(), texts_);
    performanceBand_.SetVisible(showPerformanceOverlay_);
}

void YOUMUGame::OnClientUpdated(const UpdateContext &context)
{
    if (context.input.WasKeyPressed(VK_F7))
    {
        showPerformanceOverlay_ = !showPerformanceOverlay_;
        performanceBand_.SetVisible(showPerformanceOverlay_);
    }
    const bool measured = context.performance.hasMeasurement &&
                          context.performance.measurementIndex != lastPerformanceMeasurementIndex_;
    if (measured)
    {
        performanceBand_.Refresh(context.performance);
        lastPerformanceMeasurementIndex_ = context.performance.measurementIndex;
    }
    performanceBand_.Update();
}

void YOUMUGame::OnClientShuttingDown() noexcept
{
    performanceBand_.Shutdown();
}

void YOUMUGame::RestoreSavedAudioDriver(AudioSystem &audio)
{
    const auto &saved = optionSettings_.Values();
    if (audio.RequestedOutput() != saved.audioOutput ||
        (saved.driverIndex < 0 && saved.driverName.empty())) return;
    const auto &drivers = audio.OutputDrivers();
    auto selected = std::ranges::find_if(drivers, [&](const auto &driver) {
        return driver.driverIndex == saved.driverIndex &&
            (saved.driverName.empty() || driver.name == saved.driverName);
    });
    if (selected == drivers.end() && !saved.driverName.empty())
        selected = std::ranges::find(drivers, saved.driverName, &AudioDeviceInfo::name);
    if (selected == drivers.end()) return; // Keep the default device when hardware is unavailable.
    std::string error;
    if (!audio.SetOutputDriver(selected->driverIndex, error)) OutputDebugStringA(error.c_str());
}
