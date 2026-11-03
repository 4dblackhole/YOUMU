#pragma once
#include "SongFavorites.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>

class SongFavoriteTabs final
{
  public:
    void Initialize(Visual2DNode &parent, TextCatalog &texts);
    void Shutdown() noexcept;
    void SetBounds(Rect bounds);
    void Refresh(const SongFavorites &favorites, std::uint64_t selected);
    [[nodiscard]] std::optional<std::uint64_t> ProcessAction(const Action &action);
    [[nodiscard]] bool IsManageAction(const Action &action) const noexcept;
    bool Scroll(Point position, float wheel);

  private:
    void Layout();
    void AddTab(std::uint64_t id, std::wstring name, bool selected, float &x);
    TextCatalog *texts_{};
    Visual2DNode *root_{}, *viewport_{}, *content_{}, *scrollbar_{}, *manage_{};
    std::vector<std::pair<NodeId, std::uint64_t>> tabs_;
    float offset_{}, extent_{};
    std::uint64_t revision_{~std::uint64_t{}}, textRevision_{}, selected_{};
};
