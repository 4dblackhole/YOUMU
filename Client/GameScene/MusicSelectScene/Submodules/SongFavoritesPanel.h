#pragma once
#include "SongFavorites.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <functional>
#include <optional>

// Asks for a collection file: an existing one to import (save false) or where to export
// (save true, starting from the suggested file name). Empty when the player cancels.
using CollectionFilePicker =
    std::function<std::optional<std::filesystem::path>(bool save, const std::wstring &suggestedName)>;

class SongFavoritesPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts);
    void Shutdown() noexcept;
    // chartHash is the selected pattern's (empty without one); label goes into collection files.
    // known labels fill charts that do not have one yet, so exported files name them.
    void Open(const SongFavorites &favorites, std::string chartHash, SongCollectionChart label, std::wstring name,
              const std::map<std::string, SongCollectionChart> &known = {});
    void Close() noexcept;
    void SetViewport();
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> pointer);
    [[nodiscard]] std::optional<SongFavoritesValues> TakeApplied();
    void ShowError(std::string error);
    // The Windows file dialogs by default; tests answer without a window.
    void SetFilePicker(CollectionFilePicker picker) { picker_ = std::move(picker); }

  private:
    PhantomCore::Visual2DNode &Button(PhantomCore::Visual2DNode &parent, std::string name);
    void Layout();
    void RefreshRows();
    void EditName(bool create);
    void ImportCollection();
    void ExportCollection();
    void ShowSelected();
    void MoveList(int delta);
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *overlay_{}, *panel_{}, *title_{}, *pattern_{}, *list_{}, *status_{};
    PhantomCore::Visual2DNode *create_{}, *rename_{}, *remove_{}, *apply_{}, *close_{}, *previous_{}, *next_{};
    PhantomCore::Visual2DNode *import_{}, *export_{};
    std::vector<std::pair<PhantomCore::NodeId, std::uint64_t>> rowIds_, memberIds_;
    SongFavoritesValues draft_;
    std::optional<SongFavoritesValues> applied_;
    CollectionFilePicker picker_;
    std::string chartHash_, error_;
    SongCollectionChart label_;
    std::wstring patternName_, notice_;
    std::uint64_t selected_{};
    std::size_t first_{};
    bool discardKeys_{}, waitRelease_{};
};
