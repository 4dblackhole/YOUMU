#pragma once

#include "Common/RhythmTypes.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

class PlaySession;

// Caches the last compiled note/tail time and owns only progress presentation.
// Canvas owns the nodes; Shutdown releases observers before Canvas destruction.
class GameplayProgressView final
{
  public:
    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, PhantomCore::Visual2DNode &parent,
                    const PlaySession &session, TextCatalog &texts);
    [[nodiscard]] float Height() const noexcept
    {
        return height_;
    }
    void SetLayout(PhantomCore::Rect bounds);
    void Update(RhythmTime time);
    void Shutdown() noexcept;

  private:
    void CacheLastNoteTime(const PlaySession &session);
    void CreateTimeLabels(TextCatalog &texts);
    void UpdateFillLayout();

    PhantomCore::Visual2DNode *root_{};
    PhantomCore::Visual2DNode *track_{};
    PhantomCore::Visual2DNode *fill_{};
    PhantomCore::Visual2DNode *marker_{};
    PhantomCore::Visual2DNode *currentLabel_{};
    PhantomCore::Visual2DNode *totalLabel_{};
    RhythmTime endTime_{};
    std::int64_t displayedSeconds_{-1};
    float height_{};
    float trackHeight_{};
    PhantomCore::Size markerSize_{};
    float labelWidth_{};
    float fraction_{};
    bool hasNotes_{};
};
