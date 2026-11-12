#pragma once

#include "PhantomCore.h"

#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"
#include "Mode/Results/IPlayResult.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
class EditorWorkspaceStore;

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
    void OnClientRendered(const PhantomCore::RenderContext &context) override;
    void OnClientShuttingDown() noexcept override;

  private:
    void RefreshPerformanceText(const PhantomCore::PerformanceStatistics &performance);
    std::unique_ptr<PhantomCore::GameScene> CreateResultScene();
    void RestoreSavedAudioDriver(PhantomCore::AudioSystem &audio);
    static void SubmitPerformanceLine(PhantomCore::TextRenderSystem &textRendering,
                                      std::wstring_view text, const PhantomCore::FontHandle &font,
                                      float layoutY, float viewportWidth);

    bool smokeTest_{};
    bool showPerformanceOverlay_{};
    bool rhythmDebugMode_{};
    std::string initialSceneId_;
    std::shared_ptr<GameplayLaunchStore> launchRequest_;
    std::shared_ptr<EditorWorkspaceStore> editorWorkspaceStore_;
    std::unique_ptr<IPlayResult> pendingResult_;
    TextCatalog texts_;
    OptionSettings optionSettings_;
    PhantomCore::FontHandle performanceFont_;
    std::uint64_t lastPerformanceMeasurementIndex_{};
    std::wstring framesPerSecondText_;
    std::wstring updatesPerSecondText_;
    std::wstring drawCallsText_;
};
