#pragma once

#include "PhantomCore.h"

#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Diagnostics/LaunchDiagnostics.h"
#include "Mode/Results/IPlayResult.h"
#include "Presentation/PerformanceBand.h"
#include "Presentation/VolumeOverlay.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
class SongLibrary;
class GameDiagnostics;

// The Client root for YOUMU.  The Engine owns the platform loop while
// this class selects the game window policy and registers game-owned Scenes.
class YOUMUGame final : public PhantomCore::SceneGameClient
{
  public:
    // diagnostics receives the engine's records and the one-second samples; it is owned by wWinMain and may
    // be null (no records). launch carries the hidden switches that fail on purpose.
    YOUMUGame(bool smokeTest, std::string initialSceneId, bool rhythmDebugMode = false,
              GameDiagnostics *diagnostics = nullptr, DiagnosticsLaunchOptions launch = {});

    [[nodiscard]] PhantomCore::EngineConfig GetEngineConfig() const override;

  protected:
    void RegisterScenes(PhantomCore::SceneManager &scenes) override;
    [[nodiscard]] std::string_view InitialSceneId() const noexcept override;
    void OnClientInitialized(const PhantomCore::EngineServices &services) override;
    void OnClientUpdated(const PhantomCore::UpdateContext &context) override;
    void OnClientShuttingDown() noexcept override;

  private:
    std::unique_ptr<PhantomCore::GameScene> CreateResultScene();
    void RestoreSavedAudioDriver(PhantomCore::AudioSystem &audio);
    void SaveDraggedWindowPosition();
    void RecordDiagnostics(const PhantomCore::UpdateContext &context, bool measured);
    void FailOnceForTest(const PhantomCore::UpdateContext &context);
    // Option.ini's DeviceRemovedData, or on for a watch mode run.
    [[nodiscard]] bool DeviceRemovedData() const noexcept;

    bool smokeTest_{};
    bool showPerformanceOverlay_{};
    bool rhythmDebugMode_{};
    std::string initialSceneId_;
    std::shared_ptr<GameplayLaunchStore> launchRequest_;
    std::shared_ptr<SongLibrary> songLibrary_;
    std::unique_ptr<IPlayResult> pendingResult_;
    // The pending result came from a record (song select), so the result screen does not save it.
    bool pendingResultRecorded_{};
    TextCatalog texts_;
    OptionSettings optionSettings_;
    PerformanceBand performanceBand_;
    PhantomCore::IRenderRateControl *renderRateControl_{}; // Engine-owned.
    VolumeOverlay volumeOverlay_;
    PhantomCore::IWindowControl *windowControl_{};         // Engine-owned; null in offscreen runs.
    std::uint64_t lastPerformanceMeasurementIndex_{};
    GameDiagnostics *diagnostics_{}; // Owned by wWinMain; null when nothing is recorded.
    DiagnosticsLaunchOptions launch_;
    std::string loggedScene_;
    bool failedForTest_{};
};
