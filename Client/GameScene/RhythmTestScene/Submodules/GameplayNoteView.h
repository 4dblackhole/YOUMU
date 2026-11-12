#pragma once

#include "GameFlow/GameplayFeedback.h"
#include "GameplayNoteVisibility.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

#include <filesystem>
#include <map>
#include <unordered_map>
#include <vector>

// Owns the notes, long-note parts, ticks, measure lines and focus-note effects
// of one lane. Nodes live under a child of the borrowed lane root, or on the
// Canvas root for HUD-level focus effects, and are released by that Canvas.
// The lane's local +Y axis is the future-note direction; the presenter rotates
// the lane root for a mode's screen orientation.
class GameplayNoteView final
{
  public:
    // Skin images every note visual may use; the Scene prepares them first.
    static void AppendImagePaths(std::vector<std::filesystem::path> &paths);

    void Attach(PhantomCore::Visual2DCanvas &canvas, PhantomCore::Visual2DNode &laneRoot);
    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, TextCatalog &texts, const PlaySession &session);
    // Lane width and judgement position in lane-local pixels. The lane root's
    // current height is the visible travel length.
    void SetLaneLayout(float laneWidth, float judgementLocalY);
    void ApplyFeedback(const GameplayFeedback &feedback);
    void Update(RhythmTime time);
    [[nodiscard]] RhythmTime InitialTimelineTime() const;
    void Shutdown() noexcept;

  private:
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

    void CreateNoteVisual(const INote &note);
    PhantomCore::ImageHandle NoteImage(std::wstring_view file);
    void UpdateTravelLayout();
    void UpdateMeasureLines(RhythmTime time, bool enabled);
    void UpdateNote(const INote &model, RhythmTime time, int zIndex, bool &focusClaimed);
    void UpdateLongNoteParts(NoteVisualLayers &layers, const NotePresentationInfo &presentation, RhythmTime time,
                             RhythmTime anchor, float localY, float speed, std::size_t processedTicks);
    void UpdateTickVisuals(NoteVisualLayers &layers, const NotePresentationInfo &presentation, RhythmTime time,
                           RhythmTime anchor, std::size_t processedTicks);
    [[nodiscard]] float TravelPixelsAt(RhythmTime target, RhythmTime current, float multiplier = 1) const;
    [[nodiscard]] PhantomCore::Point LaneObjectPosition(RhythmTime target, RhythmTime current,
                                                        float multiplier) const;
    void HideTransientNoteVisuals();
    void RemoveUnusedNoteVisuals();
    void PresentHitCounter(NoteVisualLayers &layers, const NoteProgress &progress, float visualHeight);
    void PresentFocusNoteProcessing(NoteVisualLayers &layers, const NoteProgress &progress, RhythmTime time);
    void PresentCompletionEffect(RhythmTime time);

    PhantomCore::ScreenVisual2DManager *visuals_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DNode *laneRoot_{};
    PhantomCore::Visual2DNode *noteRoot_{};
    const PlaySession *session_{};
    std::unordered_map<NoteId, NoteVisualLayers> noteVisuals_;
    GameplayNoteVisibility visibility_;
    std::map<std::wstring, PhantomCore::ImageHandle, std::less<>> noteImages_;
    std::vector<NoteId> presentedNoteIds_;
    std::uint64_t presentationGeneration_{};
    std::unordered_map<NoteId, RhythmTime> completionEffects_;
    std::vector<TimedVisual> measureLineVisuals_;
    float largestHeadRadius_{};
    float laneWidth_{152.0F};
    float judgementLocalY_{76.0F};
    float noteTravelDistance_{1.0F};
    float pixelsPerWholeNote_{1.0F};
};
