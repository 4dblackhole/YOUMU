#pragma once
#include "SongFavorites.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>

class SongFavoritesPanel final
{
  public:
    void Initialize(PhantomCore::Visual2DCanvas &canvas, TextCatalog &texts);
    void Shutdown() noexcept;
    void Open(const SongFavorites &favorites, const std::filesystem::path &pattern, std::wstring name);
    void Close() noexcept;
    void SetViewport();
    void RefreshTexts();
    [[nodiscard]] bool IsVisible() const noexcept { return overlay_ && overlay_->IsVisible(); }
    bool ProcessAction(const PhantomCore::Action &action);
    void ProcessInput(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> pointer);
    [[nodiscard]] std::optional<SongFavoritesValues> TakeApplied();
    void ShowError(std::string error);

  private:
    PhantomCore::Visual2DNode &Button(PhantomCore::Visual2DNode &parent, std::string name);
    void Layout();
    void RefreshRows();
    void EditName(bool create);
    void MoveList(int delta);
    PhantomCore::Visual2DCanvas *canvas_{};
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *overlay_{}, *panel_{}, *title_{}, *pattern_{}, *list_{}, *status_{};
    PhantomCore::Visual2DNode *create_{}, *rename_{}, *remove_{}, *apply_{}, *close_{}, *previous_{}, *next_{};
    std::vector<std::pair<PhantomCore::NodeId, std::uint64_t>> rowIds_, memberIds_;
    SongFavoritesValues draft_;
    std::optional<SongFavoritesValues> applied_;
    std::string key_, error_;
    std::wstring patternName_;
    std::uint64_t selected_{};
    std::size_t first_{};
    bool discardKeys_{}, waitRelease_{};
};
