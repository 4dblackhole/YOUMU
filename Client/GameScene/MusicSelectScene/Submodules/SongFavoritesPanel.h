#pragma once
#include "SongFavorites.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>

class SongFavoritesPanel final
{
  public:
    void Initialize(Visual2DCanvas &canvas, TextCatalog &texts);
    void Shutdown() noexcept;
    void Open(const SongFavorites &favorites, const std::filesystem::path &pattern, std::wstring name);
    void Close() noexcept;
    void SetViewport();
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const Action &action);
    void ProcessInput(const InputState &input, std::optional<Point> pointer);
    [[nodiscard]] std::optional<SongFavoritesValues> TakeApplied();
    void ShowError(std::string error);

  private:
    Visual2DNode &Button(Visual2DNode &parent, std::string name);
    void Layout();
    void RefreshRows();
    void EditName(bool create);
    void MoveList(int delta);
    Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    Visual2DNode *overlay_{}, *panel_{}, *title_{}, *pattern_{}, *list_{}, *status_{};
    Visual2DNode *create_{}, *rename_{}, *remove_{}, *apply_{}, *close_{}, *previous_{}, *next_{};
    std::vector<std::pair<NodeId, std::uint64_t>> rowIds_, memberIds_;
    SongFavoritesValues draft_;
    std::optional<SongFavoritesValues> applied_;
    std::string key_, error_;
    std::wstring patternName_;
    std::uint64_t selected_{};
    std::size_t first_{};
    bool discardKeys_{}, waitRelease_{};
};
