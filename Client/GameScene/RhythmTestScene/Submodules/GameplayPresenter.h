#pragma once
#include "App/FileProgress.h"
#include "GameFlow/GameplayFeedback.h"
#include "GameplayJudgementView.h"
#include "GameplayProgressView.h"
#include "GameplayHpView.h"
#include "GameplayStatisticsView.h"
#include "PhantomCore.h"
#include "Mode/PlayGameMode.h"
#include "Presentation/LaneKeyBeam.h"
#include "Texts/TextCatalog.h"
#include "GameplayNoteView.h"
#include "GameplayInputView.h"
#include "GameplayHighlightView.h"
#include <array>

// Composes the gameplay screen: background, scroll gear and lane and HUD views.
// Each view owns its own nodes and state; the presenter places them and forwards
// session time, feedback and input. The Scene shows diagnostics on the shared
// screen error line, including BackgroundWarning.
class GameplayPresenter final
{
  public:
    GameplayPresenter(PhantomCore::ScreenVisual2DManager &visuals,
                      TextCatalog &texts)
        : screenVisuals_(visuals), texts_(texts)
    {
    }
    void Initialize(const PhantomCore::EngineServices &services,
                    const PlaySession &session);
    // Prepares the play screen's images one at a time (the loading bar counts them), then the chart's BG.
    bool PrepareAssets(const PhantomCore::EngineServices &services);
    // Images ready so far, the chart's BG included; nothing is known before the first PrepareAssets.
    [[nodiscard]] FileProgress PreparedImages() const noexcept
    {
        return {preparedImages_ + (backgroundPrepared_ ? 1U : 0U),
                preparingImages_.size() + (backgroundPath_.empty() ? 0U : 1U)};
    }
    // Percent from the launch request, shared by every mode's lane key beam.
    void ConfigureKeyBeamOpacity(const int percent) noexcept
    {
        keyBeam_.SetOpacity(static_cast<float>(percent) / 100.0F);
    }
    // Percent of the background's brightness (osu!'s background dim): 0 draws it black,
    // 100 as the image is. Applies at once when the play screen already exists.
    void ConfigureBackgroundBrightness(int percent);
    void ConfigureBackground(std::filesystem::path path, double x, double y)
    {
        backgroundPath_ = std::move(path);
        backgroundOffset_ = {static_cast<float>(x), static_cast<float>(y)};
    }
    void SetVisible(bool visible)
    {
        static_cast<void>(canvasHandle_.SetVisible(visible));
    }
    void AdvanceEffects(double delta)
    {
        keyBeam_.Update(delta);
        judgementView_.Update(delta);
    }
    void OnResize(const std::uint32_t width, const std::uint32_t height);
    void Shutdown() noexcept;
    void ApplyFeedback(const GameplayFeedback &feedback);
    void UpdatePresentation(const RhythmTime time);
    void UpdateHp(const GameplayHpState &state) { hpView_.Update(state); }
    // Why the chart's BG could not be shown (the screen keeps its plain colour), or empty.
    [[nodiscard]] std::string_view BackgroundWarning() const noexcept
    {
        return backgroundWarning_;
    }
    [[nodiscard]] RhythmTime InitialTimelineTime() const;
    void UpdateInputPresentation(const PhantomCore::InputState &input, const double deltaSeconds);

  private:
    PhantomCore::ImageHandle backgroundImage_{};
    PhantomCore::Size backgroundSize_{};
    PhantomCore::Point backgroundOffset_{};
    float backgroundBrightness_{1.0F};
    void ApplyBackgroundBrightness();
    std::filesystem::path backgroundPath_;
    std::string backgroundWarning_;

    void CreatePresentation();
    void CreateLaneVisuals(PhantomCore::Visual2DNode &gearRoot);
    void CreateLaneSurface();
    void UpdateLaneSurfaceLayout(const float laneLength);
    void UpdatePresentationLayout();
    void UpdateAccuracyPresentation();
#if defined(_DEBUG)
    void UpdateDebugPresentation(RhythmTime time);
#endif
    void PresentJudgementFeedback(const GameplayFeedback &feedback);
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    PhantomCore::ScreenVisual2DManager &screenVisuals_;
    TextCatalog &texts_;
    PhantomCore::ScreenCanvasHandle canvasHandle_;
    PhantomCore::Visual2DCanvas *canvas_{};
    const PlaySession *session_{};
    LaneKeyBeam keyBeam_;
    GameplayJudgementView judgementView_;
    GameplayProgressView progressView_;
    GameplayHpView hpView_;
    GameplayStatisticsView statisticsView_;
    GameplayNoteView noteView_;
    GameplayHighlightView highlightView_;
    std::vector<std::filesystem::path> preparingImages_;
    std::size_t preparedImages_{};
    bool backgroundPrepared_{};
    GameplayInputView inputView_;
    PhantomCore::Visual2DNode *background_{};
    PhantomCore::Visual2DNode *scrollGearRoot_{};
    PhantomCore::Visual2DNode *scrollGearBorder_{};
    PhantomCore::Visual2DNode *inputPresentationRoot_{};
    PhantomCore::Visual2DNode *inputPanel_{};
    PhantomCore::Visual2DNode *laneRoot_{};
    std::vector<PhantomCore::Visual2DNode *> laneTiles_;
    PhantomCore::ImageHandle laneImage_{};
    PhantomCore::Visual2DNode *accuracyIndicator_{};
#if defined(_DEBUG)
    PhantomCore::Visual2DNode *noteDebugLabel_{};
#endif
    float laneWidth_{152.0F};
    float laneTileLength_{42.0F};
    float judgementLocalY_{76.0F};
    PhantomCore::Size inputPanelSize_{152.0F, 152.0F};
};
