#include "SongFavoriteTabs.h"
#include "SongSelectLayout.h"
#include <algorithm>

using namespace PhantomCore;

using namespace song_select;

namespace
{
    namespace theme = ui_colors::theme;
    constexpr float TabHeight = 34.0F, TabGap = 16.0F, TabPadding = 2.0F, ManageWidth = 32.0F;

    [[nodiscard]] float TabWidth(const std::wstring_view name) noexcept
    {
        return std::clamp(EstimateCaptionWidth(name, 13.0F) + TabPadding * 2.0F, 22.0F, 160.0F);
    }
}

void SongFavoriteTabs::Initialize(Visual2DNode &parent, ScreenVisual2DManager &visuals, TextCatalog &texts)
{
    texts_ = &texts;
    visuals_ = &visuals;
    root_ = &parent.CreateChild("Favorites.Tabs");
    viewport_ = &root_->CreateChild("Favorites.TabViewport");
    content_ = &viewport_->CreateChild("Favorites.TabContent");
    scrollbar_ = &CreateSlider(*root_, {}, 0, "Favorites.TabScroll");
    ApplySpriteStyle(*scrollbar_, {0.122F, 0.298F, 0.6F, 0.14F}, {0.122F, 0.298F, 0.6F, 0.24F});
    // The star opens folder management; it is an icon, not a caption.
    manage_ = &CreateButton(*root_, {}, L"", "Favorites.Manage");
    ApplySpriteStyle(*manage_, {1.0F, 1.0F, 1.0F, 0.0F}, {1.0F, 1.0F, 1.0F, 0.72F});
    auto &star = CreateSprite(*manage_, {(ManageWidth - 18.0F) * 0.5F, (TabHeight - 18.0F) * 0.5F, 18.0F, 18.0F},
                              visuals.RegisterImage(Common(L"Icons.star")), "Icon");
    RequireComponent<SpriteVisualComponent>(star).SetTint(theme::BrandText);
}
void SongFavoriteTabs::Shutdown() noexcept
{
    root_ = viewport_ = content_ = scrollbar_ = manage_ = nullptr;
    texts_ = nullptr;
    visuals_ = nullptr;
    tabs_.clear();
    revision_ = ~std::uint64_t{};
}
void SongFavoriteTabs::SetBounds(Rect bounds)
{
    if (!root_) return;
    root_->SetBounds(bounds);
    Layout();
    RevealSelected();
}
void SongFavoriteTabs::RevealSelected()
{
    const auto selected = std::ranges::find_if(tabs_, [this](const Tab &tab) { return tab.folder == selected_; });
    if (selected == tabs_.end()) return;
    const float width = viewport_->NodeSize().width;
    if (selected->x < offset_) offset_ = selected->x;
    else if (selected->x + selected->width > offset_ + width) offset_ = selected->x + selected->width - width;
    Layout();
}
void SongFavoriteTabs::AddTab(std::uint64_t id, std::wstring name, bool selected, float &x)
{
    const float width = TabWidth(name);
    auto &button = CreateButton(*content_, {x, 0, width, TabHeight}, std::move(name), "Favorites.Tab." + std::to_string(id));
    button.SetClipRect({0, 0, width, TabHeight});
    // Underline tabs: no tab surface, only the underline image on the chosen or hovered tab.
    VisualStyle style{};
    style.normal = style.hovered = style.pressed = style.disabled = {1.0F, 1.0F, 1.0F, 0.0F};
    const auto underline = [this](const char *key)
    {
        const auto path = SkinSetSelection::Instance().Image("MusicSelect", key);
        return path.empty() ? ImageHandle{} : visuals_->RegisterImage(path);
    };
    const auto chosen = underline("Category.Selected"), hovered = underline("Category.Hovered");
    if (selected && chosen)
    {
        style.normalImage = style.hoveredImage = style.pressedImage = style.disabledImage = chosen;
        style.normal = style.hovered = style.pressed = style.disabled = PureWhite;
    }
    else if (hovered)
    {
        style.hoveredImage = style.pressedImage = hovered;
        style.hovered = style.pressed = PureWhite;
    }
    auto &sprite = RequireComponent<SpriteVisualComponent>(button);
    sprite.SetStyle(style);
    sprite.SetCornerRadius(0.0F);
    StyleText(RequireComponent<TextVisualComponent>(button), 13.0F, 800, selected ? theme::BrandText : theme::InkSoft,
              TextAlignment::Center);
    tabs_.push_back({button.Id(), id, x, width});
    x += width + TabGap;
}
void SongFavoriteTabs::Refresh(const SongFavorites &favorites, std::uint64_t selected)
{
    if (!root_) return;
    if (revision_ == favorites.Revision() && selected_ == selected && textRevision_ == texts_->Revision()) return;
    for (const auto &tab : tabs_) static_cast<void>(content_->RemoveChild(tab.node));
    tabs_.clear();
    float x{};
    AddTab(0, std::wstring{texts_->Get("Favorites.all")}, selected == 0, x);
    for (const auto &folder : favorites.Values().folders)
        AddTab(folder.id, folder.name, selected == folder.id, x);
    extent_ = std::max(x - TabGap, 0.0F);
    const bool moved = selected_ != selected;
    selected_ = selected;
    revision_ = favorites.Revision();
    textRevision_ = texts_->Revision();
    Layout();
    if (moved) RevealSelected();
}
void SongFavoriteTabs::Layout()
{
    const float width = std::max(root_->NodeSize().width - ManageWidth - 4.0F, 1.0F);
    viewport_->SetBounds({0, 0, width, TabHeight});
    viewport_->SetClipRect({0, 0, width, TabHeight});
    offset_ = std::clamp(offset_, 0.0F, std::max(extent_ - width, 0.0F));
    content_->SetBounds({-offset_, 0, extent_, TabHeight});
    manage_->SetBounds({width + 4.0F, 0, ManageWidth, TabHeight});
    // The horizontal bar lives in the gap under the tabs and appears only on overflow.
    scrollbar_->SetBounds({0, -10, width, 8});
    scrollbar_->SetVisible(extent_ > width);
    RequireComponent<SliderBehaviorComponent>(*scrollbar_).SetValue(extent_ <= width ? 0 : offset_ / (extent_ - width));
}
std::optional<std::uint64_t> SongFavoriteTabs::ProcessAction(const Action &action)
{
    if (scrollbar_ && action.type == ActionType::ValueChanged && action.source == scrollbar_->Id())
    {
        offset_ = action.value * std::max(extent_ - viewport_->NodeSize().width, 0.0F);
        Layout();
    }
    if (action.type == ActionType::Clicked)
        for (const auto &tab : tabs_) if (action.source == tab.node) return tab.folder;
    return std::nullopt;
}
bool SongFavoriteTabs::IsManageAction(const Action &action) const noexcept
{
    return manage_ && action.type == ActionType::Clicked && action.source == manage_->Id();
}
bool SongFavoriteTabs::Scroll(Point position, float wheel)
{
    if (!root_ || !root_->BoundsInCanvas().Contains(position) || wheel == 0) return false;
    offset_ -= wheel * 48; // About one short tab per wheel notch.
    Layout();
    return true;
}
