#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "Mode/Results/IPlayResult.h"
#include <memory>

class GameplaySessionController;
class GameplayPresenter;
class GameplayPauseMenu;
class EditorTestControls;
class RhythmTestScene final : public GameScene
{
  public:
    explicit RhythmTestScene(std::shared_ptr<GameplayLaunchStore> launchRequest,
                             AudioPlaybackManager &playback,
                             ScreenVisual2DManager &screenVisuals,
                             bool debugMode,
                             TextCatalog &texts,
                             std::unique_ptr<IPlayResult> &pendingResult);

    ~RhythmTestScene() override;

    void Initialize(const EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void ShowResult(SceneManager &scenes);
    void UpdatePauseMenu(const UpdateContext &context, SceneManager &scenes);
    bool PreparePresentation();
    bool TryPreparePresentation();
    void ReturnToEditor(SceneManager &scenes);
    void PrepareLoadingBackground();
    void UpdateLoadingBackgroundLayout();
    std::unique_ptr<IPlayResult> &pendingResult_;
    std::vector<std::unique_ptr<IHpGauge>> hpGauges_;
    std::unique_ptr<GameplaySessionController> controller_;
    std::unique_ptr<GameplayPresenter> view_;
    std::unique_ptr<GameplayPauseMenu> pauseMenu_;
    std::unique_ptr<EditorTestControls> testControls_;
    ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    GameplayLaunchRequest launch_;
    std::shared_ptr<GameplayLaunchStore> launchStore_;
    bool testPreparationFailed_{};
    ScreenCanvasHandle loadingCanvas_;
    Visual2DNode *loadingBackground_{};
    Size loadingBackgroundSize_{};
    bool loadingBackgroundReady_{};
    std::optional<EngineServices> services_;
    bool presentationReady_{};
    double nextPreparationPoll_{};
};
