#pragma once

#include "PhantomCore.h"

#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Mode/Results/IPlayResult.h"
#include "Presentation/PerformanceBand.h"
#include "Presentation/VolumeOverlay.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
class SongLibrary;

// The Client root for YOUMU.  The Engine owns the platform loop while
// this class selects the game window policy and registers game-owned Scenes.
class YOUMUGame final : public PhantomCore::SceneGameClient
{
  public:
    YOUMUGame(bool smokeTest, std::string initialSceneId,
                   bool rhythmDebugMode = false);

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
    VolumeOverlay volumeOverlay_;
    std::uint64_t lastPerformanceMeasurementIndex_{};
};
