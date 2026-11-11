#pragma once
#include "GameFlow/GameplayFeedback.h"
#include "GameplayJudgementView.h"
#include "GameplayProgressView.h"
#include "GameplayHpView.h"
#include "GameplayStatisticsView.h"
#include "PhantomCore.h"
#include "Mode/PlayGameMode.h"
#include "Presentation/LaneKeyBeam.h"
#include "Texts/TextCatalog.h"
#include "GameplayNoteVisibility.h"
#include <array>
#include <unordered_map>

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
    bool PrepareAssets(const PhantomCore::EngineServices &services);
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
    void PresentErrorMessage(const std::string_view message);
    void UpdatePresentation(const RhythmTime time);
    void UpdateHp(double percent) { hpView_.Update(percent); }
    void SetDebugVisible(bool visible);
    [[nodiscard]] RhythmTime InitialTimelineTime() const;
    void UpdateInputPresentation(const PhantomCore::InputState &input, const double deltaSeconds);

  private:
    PhantomCore::ImageHandle backgroundImage_{};
    PhantomCore::Size backgroundSize_{};
    PhantomCore::Point backgroundOffset_{};
    std::filesystem::path backgroundPath_;
    std::string backgroundWarning_;
    enum class FocusNoteType : std::uint8_t
    {
        None,
        Balloon,
        DengDeng,
    };

    struct TimedVisual
    {
        RhythmTime timing{};
        PhantomCore::Visual2DNode *node{};
    };

    struct NoteVisualLayers
    {
        PhantomCore::Visual2DNode *root{};
        PhantomCore::Visual2DNode *ambient{};
        PhantomCore::Visual2DNode *overlay{};
        PhantomCore::Visual2DNode *body{};
        PhantomCore::Visual2DNode *bodyOverlay{};
        PhantomCore::Visual2DNode *tail{};
        PhantomCore::Visual2DNode *tailOverlay{};
        PhantomCore::Visual2DNode *processing{};
        PhantomCore::Visual2DNode *success{};
        PhantomCore::Visual2DNode *counter{};
        PhantomCore::Visual2DNode *counterText{};
        std::vector<TimedVisual> ticks;
        PhantomCore::ImageHandle tickImage{};
        PhantomCore::Size tickSize{};
        RhythmTime timing{};
        RhythmTime endTime{};
        FocusNoteType focusType{FocusNoteType::None};
        PhantomCore::Size processingSize{};
        PhantomCore::Size successSize{};
        float diameter{};
        float bodyWidth{};
        float tailWidth{};
        float tailHeight{};
        std::uint64_t presentedGeneration{};
    };

    void CreatePresentation();
    void CreateLaneVisuals(PhantomCore::Visual2DNode &gearRoot);
    void CreateLaneSurface();
    void UpdateLaneSurfaceLayout(const float laneLength);
    void CreateKeyIndicators(PhantomCore::Visual2DNode &inputPanel);
    void CreateNoteVisuals();
    void CreateNoteVisual(const INote &);
    PhantomCore::ImageHandle NoteImage(std::wstring_view);
    void UpdateMeasureLines(RhythmTime, bool);
    void UpdateTickVisuals(NoteVisualLayers &, const NotePresentationInfo &,
                           RhythmTime, RhythmTime, std::size_t);
    void UpdatePresentationLayout();
    void UpdateNoteTravelLayout();
    [[nodiscard]] float TravelPixels(RhythmDuration duration,
                                     float multiplier = 1.0F) const noexcept;
    float TravelPixelsAt(RhythmTime target, RhythmTime current,
                         float multiplier = 1) const;
    [[nodiscard]] PhantomCore::Point LaneObjectPosition(RhythmTime target,
                                                          RhythmTime current,
                                                          float multiplier) const;
    void HideTransientNoteVisuals();
    void RemoveUnusedNoteVisuals();
    void PresentHitCounter(NoteVisualLayers &layers,
                             const NoteProgress &progress,
                             const float visualHeight);
    void PresentFocusNoteProcessing(NoteVisualLayers &layers,
                                    const NoteProgress &progress,
                                    const RhythmTime time);
    void PresentCompletionEffect(const RhythmTime time);
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
    std::unordered_map<NoteId, NoteVisualLayers> noteVisuals_;
    GameplayNoteVisibility visibility_;
    std::map<std::wstring, PhantomCore::ImageHandle, std::less<>> noteImages_;
    std::vector<std::filesystem::path> preparingImages_;
    float largestHeadRadius_{};
    std::vector<NoteId> presentedNoteIds_;
    std::uint64_t presentationGeneration_{};
    std::unordered_map<NoteId, RhythmTime>
        completionEffects_;
    std::vector<TimedVisual> measureLineVisuals_;
    std::array<PhantomCore::Visual2DNode *, 4> keyIndicators_{};
    std::array<PhantomCore::Visual2DNode *, 4> keyGlows_{};
    std::array<PhantomCore::Visual2DNode *, 4> keyPressFlashes_{};
    std::array<double, 4> keyPressFlashRemainingSeconds_{};
    PhantomCore::Visual2DNode *background_{};
    PhantomCore::Visual2DNode *scrollGearRoot_{};
    PhantomCore::Visual2DNode *scrollGearBorder_{};
    PhantomCore::Visual2DNode *scrollGearSurface_{};
    PhantomCore::Visual2DNode *inputPresentationRoot_{};
    PhantomCore::Visual2DNode *inputPanel_{};
    PhantomCore::Visual2DNode *laneRoot_{};
    PhantomCore::Visual2DNode *noteRoot_{};
    std::vector<PhantomCore::Visual2DNode *> laneTiles_;
    PhantomCore::ImageHandle laneImage_{};
    PhantomCore::ImageHandle strongKeyLightImage_{};
    PhantomCore::ImageHandle weakKeyLightImage_{};
    PhantomCore::ImageHandle keyPressFlashImage_{};
    PhantomCore::Visual2DNode *accuracyIndicator_{};
#if defined(_DEBUG)
    PhantomCore::Visual2DNode *noteDebugLabel_{};
#endif
    PhantomCore::Visual2DNode *errorLabel_{};
    float laneWidth_{152.0F};
    float laneTileLength_{42.0F};
    float judgementLocalY_{76.0F};
    float noteTravelDistance_{1.0F};
    float pixelsPerWholeNote_{1.0F};
    PhantomCore::Size inputPanelSize_{152.0F, 152.0F};
};
