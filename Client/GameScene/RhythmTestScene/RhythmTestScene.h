#pragma once
#include "PhantomCore.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "Texts/TextCatalog.h"
#include "Mode/Results/IPlayResult.h"
#include "Presentation/UiErrorLine.h"
#include "Submodules/GameplayLoadingSettings.h"
#include "Submodules/GameplayOffsetNotice.h"
#include "Submodules/GameplayScreenFade.h"
#include <memory>

class GameplaySessionController;
class GameplayPresenter;
class GameplayPauseMenu;
class OptionSettings;
class RhythmTestScene final : public PhantomCore::GameScene
{
  public:
    explicit RhythmTestScene(std::shared_ptr<GameplayLaunchStore> launchRequest,
                             PhantomCore::AudioPlaybackManager &playback,
                             PhantomCore::ScreenVisual2DManager &screenVisuals,
                             bool debugMode,
                             TextCatalog &texts,
                             std::unique_ptr<IPlayResult> &pendingResult,
                             // Where the loading screen saves a changed background brightness; tests may omit it.
                             OptionSettings *options = nullptr);

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
    // Black over everything while a restart darkens the screen (held restart key or a chosen Retry).
    void PresentScreenFade();
    // Shows the local offset after a +/- change and keeps it for the next attempts of this pattern.
    void PresentLocalOffset(double deltaSeconds);
    void SaveLocalOffset() noexcept;
    // The loading screen's play settings: shown until the attempt starts, they hold the start
    // while in use and apply brightness and local offset changes.
    void UpdateLoadingSettings(const PhantomCore::InputState &input);
    void ApplyBackgroundBrightness(int percent);
    void SaveBackgroundBrightness() noexcept;
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
    bool loadingBackgroundImage_{}; // The chart BG is drawn (brightness applies), not the plain colour.
    std::optional<PhantomCore::EngineServices> services_;
    UiErrorLine errorLine_;
    GameplayScreenFade screenFade_;
    GameplayOffsetNotice offsetNotice_;
    GameplayLoadingSettings loadingSettings_;
    OptionSettings *options_{};
    std::uint64_t shownOffsetRevision_{};
    bool presentationReady_{};
    double nextPreparationPoll_{};
};
