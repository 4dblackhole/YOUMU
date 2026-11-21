#pragma once

#include "PhantomCore.h"
#include "Presentation/UiScrollbar.h"
#include "Presentation/UiTooltip.h"
#include "SongSelectionState.h"
#include "Texts/TextCatalog.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

// Virtualized song list inside the browser's viewport. Every visible song has
// a row; only rows near the viewport have card nodes. The focused song's card
// expands with one button per visible difficulty. Selection itself stays in
// SongSelectionState; this view owns the rows, cards, scroll offset and bar. While the
// focused song is heard its title carries the now-playing mark, and a card line whose name is
// cut or that has several names hints them all.
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

    // The viewport is a clipped node created by the browser column.
    void Attach(PhantomCore::Visual2DCanvas &canvas, PhantomCore::Visual2DInputRouter &router,
                PhantomCore::Visual2DNode &viewport);
    [[nodiscard]] PhantomCore::Visual2DNode *Viewport() const noexcept { return viewport_; }
    // The screen's tooltip, for the card name hints.
    void SetTooltip(UiTooltip &tooltip) noexcept { tooltip_ = &tooltip; }
    // Call after the viewport was resized.
    void SetLayout();

    // Rebuilds rows from the visible songs and the current focus.
    void Rebuild();
    void EnsureFocusedVisible();
    void ResetScroll() noexcept { scrollOffset_ = 0.0F; }
    void ScrollBy(float pixels);
    // Follows a scrollbar drag; call once per frame after the input router.
    void TrackScrollbar(std::optional<PhantomCore::Point> canvasPointer, bool leftButtonDown);
    // Shows the now-playing mark before the focused song's title while its audio plays.
    void SetNowPlaying(bool playing);
    [[nodiscard]] bool IsNowPlayingShown() const noexcept;
    // Hints card titles and artists whose line is cut or that have several names. Measured text
    // widths arrive after a frame, so the screen calls this every frame.
    void RefreshNameHints();
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
        PhantomCore::Visual2DNode *nowPlaying{};
        float textLeft{}, textWidth{};
        bool titleHinted{}, artistHinted{};
    };

    void UpdateVisibleCards();
    void CreateCard(const SongCard &row, bool focused);
    PhantomCore::NodeId CreateDifficultyButton(PhantomCore::Visual2DNode &card, std::size_t visiblePosition, bool focused);
    void ApplyScrollOffset();
    void ApplyNowPlaying(const SongCard &card) const;
    void HintName(PhantomCore::Visual2DNode &line, const std::vector<std::string> &names, std::string_view romanized,
                  bool &hinted);
    void ClearNameHints();
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
    UiScrollbar scrollbar_;
    UiTooltip *tooltip_{};
    std::vector<SongCard> cards_;
    std::vector<SongCard> rows_;
    std::vector<std::pair<PhantomCore::NodeId, std::size_t>> difficultyButtonIds_;
    std::size_t firstVisibleRow_{}, lastVisibleRow_{};
    bool cardsDirty_{true};
    bool nowPlaying_{};
    float scrollOffset_{};
    float contentHeight_{};
};
