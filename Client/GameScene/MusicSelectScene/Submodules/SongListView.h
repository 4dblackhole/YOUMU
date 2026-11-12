#pragma once

#include "PhantomCore.h"
#include "SongSelectionState.h"
#include "Texts/TextCatalog.h"

#include <cstddef>
#include <utility>
#include <vector>

// Virtualized song list inside the browser's viewport. Every visible song has
// a row; only rows near the viewport have card nodes. The focused song's card
// expands with one button per visible difficulty. Selection itself stays in
// SongSelectionState; this view owns the rows, cards, scroll offset and bar.
class SongListView final
{
  public:
    enum class ActionKind
    {
        None,
        SongClicked,
        CreateDifficultyClicked,
        PatternClicked,
    };
    struct ActionResult
    {
        ActionKind kind{};
        std::size_t index{};
        // Canvas bounds of the clicked card, captured before any rebuild.
        PhantomCore::Rect anchor{};
    };

    SongListView(PhantomCore::ScreenVisual2DManager &visuals, SongSelectionState &selection, TextCatalog &texts,
                 bool editor) noexcept
        : visuals_(visuals), selection_(selection), texts_(texts), editor_(editor)
    {
    }

    // The viewport is a clipped bordered panel created by the browser panel.
    void Attach(PhantomCore::Visual2DCanvas &canvas, PhantomCore::Visual2DInputRouter &router,
                PhantomCore::Visual2DNode &viewport);
    [[nodiscard]] PhantomCore::Visual2DNode *Viewport() const noexcept { return viewport_; }
    // Call after the viewport was resized. innerWidth is the browser panel's
    // inner design width.
    void SetLayout(float innerWidth);

    // Rebuilds rows from the visible songs and the current focus.
    void Rebuild();
    void EnsureFocusedVisible();
    void ResetScroll() noexcept { scrollOffset_ = 0.0F; }
    void ScrollBy(float designPixels);
    // Restyles difficulty buttons after the selected pattern changed.
    void RefreshPatternSelection();
    [[nodiscard]] ActionResult ProcessAction(const PhantomCore::Action &action) const;
    void Shutdown() noexcept;

  private:
    struct SongCard
    {
        std::size_t catalogIndex{};
        float top{};
        float height{};
        PhantomCore::Visual2DNode *button{};
        PhantomCore::Visual2DNode *title{};
        PhantomCore::Visual2DNode *artist{};
        std::size_t visiblePosition{};
        PhantomCore::NodeId createDifficultyId{};
    };

    void UpdateVisibleCards();
    void CreateCard(const SongCard &row, bool focused);
    PhantomCore::NodeId CreateDifficultyButton(PhantomCore::Visual2DNode &card, std::size_t visiblePosition);
    void ApplyScrollOffset();
    void UpdateScrollbar();
    [[nodiscard]] float VisibleBottom() const noexcept;

    PhantomCore::ScreenVisual2DManager &visuals_;
    SongSelectionState &selection_;
    TextCatalog &texts_;
    bool editor_{};
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DInputRouter *router_{};
    PhantomCore::Visual2DNode *viewport_{};
    PhantomCore::Visual2DNode *content_{};
    PhantomCore::Visual2DNode *scrollbarTrack_{};
    PhantomCore::Visual2DNode *scrollbarHandle_{};
    std::vector<SongCard> cards_;
    std::vector<SongCard> rows_;
    std::vector<std::pair<PhantomCore::NodeId, std::size_t>> difficultyButtonIds_;
    std::size_t firstVisibleRow_{}, lastVisibleRow_{};
    bool cardsDirty_{true};
    float scrollOffset_{};
    float contentHeight_{};
    float contentWidth_{454.29F};
};
