#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "Mode/Results/IPlayResult.h"
#include "Presentation/UiErrorLine.h"
#include <memory>

class GameplaySessionController;
class GameplayPresenter;
class GameplayPauseMenu;
class RhythmTestScene final : public PhantomCore::GameScene
{
  public:
    explicit RhythmTestScene(std::shared_ptr<GameplayLaunchStore> launchRequest,
                             PhantomCore::AudioPlaybackManager &playback,
                             PhantomCore::ScreenVisual2DManager &screenVisuals,
                             bool debugMode,
                             TextCatalog &texts,
                             std::unique_ptr<IPlayResult> &pendingResult);

    ~RhythmTestScene() override;

    void Initialize(const PhantomCore::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void ShowResult(PhantomCore::SceneManager &scenes);
    void UpdatePauseMenu(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes);
    bool PreparePresentation();
    void PrepareLoadingBackground();
    void UpdateLoadingBackgroundLayout();
    // The loading screen stays until the attempt's clock first runs, also under a pause menu opened
    // before the start or after a failed preparation; its card hides while that menu is open.
    void PresentLoadingScreen();
    void PresentScreenError();
    std::unique_ptr<IPlayResult> &pendingResult_;
    std::vector<std::unique_ptr<IHpGauge>> hpGauges_;
    std::unique_ptr<GameplaySessionController> controller_;
    std::unique_ptr<GameplayPresenter> view_;
    std::unique_ptr<GameplayPauseMenu> pauseMenu_;
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    GameplayLaunchRequest launch_;
    PhantomCore::ScreenCanvasHandle loadingCanvas_;
    PhantomCore::Visual2DNode *loadingBackground_{};
    PhantomCore::Visual2DNode *loadingCard_{};
    PhantomCore::Visual2DNode *loadingBarPiece_{};
    PhantomCore::Size loadingBackgroundSize_{};
    bool loadingBackgroundReady_{};
    std::optional<PhantomCore::EngineServices> services_;
    UiErrorLine errorLine_;
    bool presentationReady_{};
    double nextPreparationPoll_{};
};
