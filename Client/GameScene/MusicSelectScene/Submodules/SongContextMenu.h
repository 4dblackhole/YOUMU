#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>
#include <utility>
#include <vector>

enum class SongMenuItem
{
    NewDifficulty,
    Edit,
    DeleteDifficulty,
    DeleteSong,
    OpenFolder,
    Collections,
};

// The right-click menu of a song card or difficulty row: a navy plate at the pointer with one row
// per item and a rule before Open folder. It stays above the song list until an item is chosen, a
// press lands elsewhere or Esc is pressed.
class SongContextMenu final
{
  public:
    static constexpr float Width = 200, RowHeight = 30, Padding = 4, RuleGap = 9;

    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts);
    void Shutdown() noexcept;
    // at is a Canvas point; the plate is kept inside the Canvas.
    void Open(PhantomCore::Point at, std::vector<SongMenuItem> items);
    void Close() noexcept;
    [[nodiscard]] bool IsOpen() const noexcept { return plate_ != nullptr && plate_->IsVisible(); }
    [[nodiscard]] bool Contains(PhantomCore::NodeId node) const noexcept;
    [[nodiscard]] std::optional<SongMenuItem> ProcessAction(const PhantomCore::Action &action);
    [[nodiscard]] const std::vector<SongMenuItem> &Items() const noexcept { return items_; }
    void RefreshTexts();

  private:
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *plate_{};
    std::vector<SongMenuItem> items_;
    std::vector<std::pair<PhantomCore::Visual2DNode *, SongMenuItem>> rows_;
};
