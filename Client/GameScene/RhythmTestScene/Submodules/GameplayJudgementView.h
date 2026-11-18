#pragma once

#include "PhantomCore.h"
#include "Note/Submodules/NoteTypes.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <cstdint>
#include <limits>
#include <string>

struct PlayStatistics;

// Owns the timing-guide and judgement-overlay presentation state. Canvas owns
// the nodes; Shutdown must run before Canvas destruction. No gameplay mutation.
class GameplayJudgementView final
{
  public:
    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, PhantomCore::Visual2DCanvas &canvas,
                    PhantomCore::Visual2DNode &judgementCircle, const AccuracyRange &baseRange,
                    TextCatalog &texts);
    void OnResize(float logicalWidth);
    // Centres the guide horizontally on the screen (zero at Canvas x 0) below the gear.
    void SetGuidePlacement(float gearBottom);
    void Present(const NoteEvent &event);
    // Mean signed error of the attempt's graded hits, read from the live result
    // statistics so it always equals the result screen's mean.
    void ShowAverageError(const PlayStatistics &statistics);
    void PresentCompletion(bool success);
    void Update(double deltaSeconds);
    void Reset() noexcept;
    void Shutdown() noexcept;

  private:
    static constexpr double FeedbackSeconds = 0.2;
    // One translucent vertical line per timed hit, coloured by its judgement.
    struct HitLine
    {
        PhantomCore::Visual2DNode *node{};
        PhantomCore::Color color{};
        RhythmDuration error{};
        std::uint64_t order{};
        double age{};
        double quickFadeAge{-1.0}; // >= 0 while a displaced line fades out early
        float quickFadeFrom{};     // alpha at the moment the early fade began
        bool alive{};
    };
    void CreateTimingGuide(PhantomCore::Visual2DNode &root, TextCatalog &texts);
    void CreateTimingPanel();
    void CreateTimingBands();
    void CreateTimingCaptions(TextCatalog &texts);
    void CreateAverageError(TextCatalog &texts);
    void CreateHitLines();
    void LayoutTimingPanel(float halfWidth);
    void CreateJudgementImages(PhantomCore::ScreenVisual2DManager &visuals,
                               PhantomCore::Visual2DNode &judgementCircle);
    void AddHitLine(RhythmDuration error, PhantomCore::Color color);
    void RetireOldestLiveLine();
    [[nodiscard]] HitLine &FreeOrOldestLine();
    void PositionLine(HitLine &);
    void AnimateLine(HitLine &, double delta);
    void ShowImage(std::size_t);

    std::array<RhythmDuration, AccuracyRange::BandCount> halfWindows_{};
    RhythmDuration displayHalfWindow_{};
    float pixelsPerMicrosecond_{};
    PhantomCore::ScreenVisual2DManager *visuals_{};
    PhantomCore::Visual2DNode *guide_{};
    // Hud.TimingPanel cut into left wing, filler, centre (with the tab),
    // filler and right wing; only the fillers stretch with the guide width.
    std::array<PhantomCore::Visual2DNode *, 5> panelPieces_{};
    PhantomCore::Visual2DNode *margin_{};
    std::array<PhantomCore::Visual2DNode *, AccuracyRange::BandCount> bands_{};
    std::array<PhantomCore::Visual2DNode *, 2> labels_{};
    PhantomCore::Visual2DNode *averageValue_{};
    PhantomCore::Visual2DNode *averageUnit_{};
    std::wstring averageEmpty_;
    // Shown mean in tenths of a millisecond; EmptyAverage while no hit is graded.
    static constexpr std::int64_t EmptyAverage = std::numeric_limits<std::int64_t>::min();
    std::int64_t shownAverage_{EmptyAverage};
    // Reused pool: at most 50 lines live at once, plus slots for displaced
    // lines that are still fading out. No per-hit node or image is created.
    std::array<HitLine, 64> hitLines_{};
    std::uint64_t nextLineOrder_{};
    std::array<PhantomCore::Visual2DNode *, 8> judgementImages_{};
    PhantomCore::Visual2DNode *activeJudgement_{};
    double judgementElapsedSeconds_{FeedbackSeconds};
};
