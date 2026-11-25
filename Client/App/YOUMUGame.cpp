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
#include "Diagnostics/GameDiagnostics.h"
#include "Diagnostics/GameLog.h"
#include "Records/LocalOffsetStore.h"
#include "Records/PlayRecordStore.h"

#include <Windows.h>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

using namespace PhantomCore;

namespace
{
    // How long the crash tests run. A device test loses the device after
    // DeviceTestFrames; a stall test holds one Update for StallTestHold, past
    // the shortened limit, and then needs time to be seen running again.
    constexpr std::uint64_t DeviceTestFrames = 600;
    constexpr std::uint64_t StallTestFrames = 600;
    constexpr double StallTestLimitSeconds = 1.0;
    constexpr DWORD StallTestHoldMilliseconds = 3000;

    [[nodiscard]] const char *Name(const AudioOutputBackend output) noexcept
    {
        switch (output)
        {
        case AudioOutputBackend::Automatic:
            return "Automatic";
        case AudioOutputBackend::Wasapi:
            return "WASAPI";
        case AudioOutputBackend::Asio:
            return "ASIO";
        case AudioOutputBackend::NoSound:
            return "NoSound";
        }
        return "unknown";
    }

    [[nodiscard]] const char *Name(const Language language) noexcept
    {
        switch (language)
        {
        case Language::Korean:
            return "ko";
        case Language::English:
            return "en";
        case Language::Japanese:
            return "ja";
        }
        return "unknown";
    }

    // The settings the engine's own start record does not show.
    void LogOptions(const OptionSettings &options)
    {
        const OptionValues &values = options.Values();
        game_log::Write(
            std::format("options: audio {} buffer {}, language {}, skin {}, songs {}, device removed data {}",
                        Name(values.audioOutput), values.dspBufferLength, Name(values.language),
                        game_log::ToUtf8(values.skinSet), game_log::ToUtf8(options.SongsRoot().wstring()),
                        values.deviceRemovedData ? "on" : "off"));
    }
} // namespace

YOUMUGame::YOUMUGame(const bool smokeTest, std::string initialSceneId, const bool rhythmDebugMode,
                     GameDiagnostics *diagnostics, DiagnosticsLaunchOptions launch)
    : smokeTest_(smokeTest), showPerformanceOverlay_(smokeTest), rhythmDebugMode_(rhythmDebugMode),
      initialSceneId_(std::move(initialSceneId)),
      launchRequest_(std::make_shared<GameplayLaunchStore>()),
      optionSettings_(OptionSettings::NextToExecutable()), diagnostics_(diagnostics), launch_(std::move(launch))
{
    std::string error;
    // Invalid/unwritable settings must not prevent startup. The options panel
    // retains the load warning until the user successfully saves new settings.
    if (!optionSettings_.Load(error))
        game_log::Write("Option.ini was not read: " + error);
    LogOptions(optionSettings_);
    GameplayLaunchRequest initialRequest;
    initialRequest.songsRoot = optionSettings_.SongsRoot();
    launchRequest_->Set(std::move(initialRequest));
    texts_.SetLanguage(optionSettings_.Values().language);
    for (const auto &diagnostic : texts_.Diagnostics())
        game_log::Write("texts: " + diagnostic);
    SkinSetSelection::Instance().Initialize(optionSettings_.Values().skinSet);
}

EngineConfig YOUMUGame::GetEngineConfig() const
{
    EngineConfig config;
    config.windowTitle = GameName;
    config.windowWidth = optionSettings_.Values().windowWidth;
    config.windowHeight = optionSettings_.Values().windowHeight;
    config.windowMode = optionSettings_.Values().windowMode;
    config.windowPosition = optionSettings_.Values().windowPosition;
    const auto color = ui_colors::ClearBackground;
    config.clearColor = {color.red, color.green, color.blue, color.alpha};
    config.renderRateMode = optionSettings_.Values().renderRate;
    config.audio.preferredBackend = optionSettings_.Values().audioOutput;
    // The mixer buffer is chosen in the options and only read here, at start.
    config.audio.dspBufferLength = optionSettings_.Values().dspBufferLength;
    // Restore the named device after enumeration instead of trusting a stale index.
    config.audio.driverIndex = -1;
    config.audio.fallBackToWasapi = true;
    config.audio.allowNoSoundFallback = true;
    config.showWindow = !smokeTest_;
    config.autoExitAfterRenderedFrames = smokeTest_ ? 3 : 0;

    config.diagnostics.sink = diagnostics_;
    config.diagnostics.graphicsDeviceRemovedData = optionSettings_.Values().deviceRemovedData;
    config.diagnostics.mainLoopStallSeconds = GameDiagnostics::MainLoopStallSeconds;
    // The hidden crash tests need more than a smoke run's three frames.
    if (launch_.removeGraphicsDevice)
    {
        config.diagnostics.removeGraphicsDeviceAfterRenderedFrames = DeviceTestFrames;
        config.autoExitAfterRenderedFrames = 0;
    }
    if (launch_.stallMainLoop)
    {
        config.diagnostics.mainLoopStallSeconds = StallTestLimitSeconds;
        config.autoExitAfterRenderedFrames = smokeTest_ ? StallTestFrames : 0;
    }
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
                                                             std::ref(texts_), std::ref(optionSettings_),
                                                             MusicSelectScene::RecordedResultHandler(
                                                                 [this](std::unique_ptr<IPlayResult> result)
                                                                 {
                                                                     pendingResult_ = std::move(result);
                                                                     pendingResultRecorded_ = true;
                                                                 })));
    requireRegistered(scenes.RegisterScene<MusicSelectScene>(std::string(EditorSongSelectSceneId),
                                                             SceneRetention::KeepAlive, std::ref(ScreenVisuals()),
                                                             std::move(editorSongLibrary), launchRequest_,
                                                             SongSelectPurpose::Editor, std::ref(texts_),
                                                             std::ref(optionSettings_)));
    requireRegistered(scenes.RegisterScene<EditorScene>(std::string(EditorSceneId),
                                                        SceneRetention::DestroyOnExit, std::ref(ScreenVisuals()),
                                                        launchRequest_, std::ref(texts_), &optionSettings_));
    requireRegistered(scenes.RegisterScene<RhythmTestScene>(std::string(RhythmTestSceneId),
                                                            // Gameplay routes keep only their factory while inactive.
                                                            // The concrete mode Scene is constructed on entry and
                                                            // destroyed as soon as it transfers its result.
                                                            SceneRetention::DestroyOnExit, launchRequest_,
                                                            std::ref(AudioPlayback()), std::ref(ScreenVisuals()),
                                                            rhythmDebugMode_, std::ref(texts_),
                                                            std::ref(pendingResult_), &optionSettings_));
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
    // A play saves its record once; a record opened from song select is only shown again.
    const bool recorded = std::exchange(pendingResultRecorded_, false);
    const bool realPlay = !smokeTest_ && !rhythmDebugMode_;
    if (realPlay && !recorded && launch.IsValid())
        display.recordDatabase = PlayRecordDatabasePath();
    ResultScene::RetryHandler retry;
    if (display.canRetry)
        retry = [store = launchRequest_, snapshot = std::move(launch)](SceneManager &scenes) mutable
        {
            // A retry owns a fresh session; restore the played selection without
            // retaining any note, timer or voice from the completed session. The local
            // offset may have changed during the attempt, so read the stored one again.
            snapshot.localOffsetMs = LocalOffsetStore(LocalOffsetFilePath()).Get(snapshot.patternPath);
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
    renderRateControl_ = services.renderRateControl;
    windowControl_ = services.windowControl;
    // The performance band is Client presentation, so the game owns it and
    // chooses F7 independently from Engine scheduling.
    performanceBand_.Initialize(ScreenVisuals(), texts_);
    performanceBand_.SetVisible(showPerformanceOverlay_);
    // Alt+wheel volume on every screen; it runs after the screens, which ignore the wheel while Alt is held.
    volumeOverlay_.Initialize(ScreenVisuals(), optionSettings_, services.windowControl);
}

void YOUMUGame::OnClientUpdated(const UpdateContext &context)
{
    // The options panel only saves the choice; the engine applies it from the next loop.
    if (renderRateControl_)
        renderRateControl_->SetMode(optionSettings_.Values().renderRate);
    SaveDraggedWindowPosition();
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
    RecordDiagnostics(context, measured);
    StallOnceForTest(context);
    performanceBand_.Update();
    volumeOverlay_.Update(context.input, context.deltaSeconds);
}

// The log gets a line when the Scene on screen changes; the recent statistics get the engine's sample once a
// second. Neither writes to disk in a normal run beyond the Scene lines.
void YOUMUGame::RecordDiagnostics(const UpdateContext &context, const bool measured)
{
    const std::string_view scene = Scenes().CurrentSceneId();
    if (scene != loggedScene_)
    {
        loggedScene_ = scene;
        game_log::Write("scene: " + loggedScene_);
    }
    if (measured && diagnostics_)
        diagnostics_->AddSample(context.performance, context.totalSeconds, scene);
}

// --crash-test=stall: one Update that does not come back for a while.
void YOUMUGame::StallOnceForTest(const UpdateContext &context)
{
    if (!launch_.stallMainLoop || stalledForTest_ || context.totalSeconds < 0.5)
        return;
    stalledForTest_ = true;
    game_log::Write("crash test: holding one Update");
    Sleep(StallTestHoldMilliseconds);
}

// A framed window the user dragged starts in the same place next time.
void YOUMUGame::SaveDraggedWindowPosition()
{
    WindowPosition position{};
    if (!windowControl_ || !windowControl_->TakeUserMove(position))
        return;
    auto candidate = optionSettings_.Values();
    candidate.windowPosition = position;
    std::string error;
    if (!optionSettings_.Save(candidate, error))
        game_log::Write("The window position was not saved: " + error);
}

void YOUMUGame::OnClientShuttingDown() noexcept
{
    performanceBand_.Shutdown();
    volumeOverlay_.Shutdown();
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
    if (!audio.SetOutputDriver(selected->driverIndex, error))
        game_log::Write("The saved audio device was not restored: " + error);
}
