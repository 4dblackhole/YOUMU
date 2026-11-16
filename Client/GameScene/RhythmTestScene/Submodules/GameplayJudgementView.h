#pragma once

#include "PhantomCore.h"
#include "Note/Submodules/NoteTypes.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <cstdint>

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
    void CreateTimingBands();
    void CreateTimingCaptions(TextCatalog &texts);
    void CreateHitLines();
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
    PhantomCore::Visual2DNode *margin_{};
    std::array<PhantomCore::Visual2DNode *, AccuracyRange::BandCount> bands_{};
    std::array<PhantomCore::Visual2DNode *, 3> labels_{};
    // Reused pool: at most 50 lines live at once, plus slots for displaced
    // lines that are still fading out. No per-hit node or image is created.
    std::array<HitLine, 64> hitLines_{};
    std::uint64_t nextLineOrder_{};
    std::array<PhantomCore::Visual2DNode *, 8> judgementImages_{};
    PhantomCore::Visual2DNode *activeJudgement_{};
    double judgementElapsedSeconds_{FeedbackSeconds};
};
