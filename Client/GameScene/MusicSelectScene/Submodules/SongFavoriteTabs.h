#pragma once
#include "SongFavorites.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>

class SongFavoriteTabs final
{
  public:
    void Initialize(PhantomCore::Visual2DNode &parent, TextCatalog &texts);
    void Shutdown() noexcept;
    void SetBounds(PhantomCore::Rect bounds);
    void Refresh(const SongFavorites &favorites, std::uint64_t selected);
    [[nodiscard]] std::optional<std::uint64_t> ProcessAction(const PhantomCore::Action &action);
    [[nodiscard]] bool IsManageAction(const PhantomCore::Action &action) const noexcept;
    bool Scroll(PhantomCore::Point position, float wheel);

  private:
    void Layout();
    void AddTab(std::uint64_t id, std::wstring name, bool selected, float &x);
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *root_{}, *viewport_{}, *content_{}, *scrollbar_{}, *manage_{};
    std::vector<std::pair<PhantomCore::NodeId, std::uint64_t>> tabs_;
    float offset_{}, extent_{};
    std::uint64_t revision_{~std::uint64_t{}}, textRevision_{}, selected_{};
};
