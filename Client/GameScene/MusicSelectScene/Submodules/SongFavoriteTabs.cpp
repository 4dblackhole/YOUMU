#include "SongFavoriteTabs.h"
#include "SongSelectLayout.h"
#include <algorithm>

using namespace PhantomCore;

using namespace song_select;

void SongFavoriteTabs::Initialize(Visual2DNode &parent, TextCatalog &texts)
{
    texts_ = &texts;
    root_ = &parent.CreateChild("Favorites.Tabs");
    viewport_ = &root_->CreateChild("Favorites.TabViewport");
    content_ = &viewport_->CreateChild("Favorites.TabContent");
    scrollbar_ = &CreateSlider(*root_, {}, 0, "Favorites.TabScroll");
    ApplySpriteStyle(*scrollbar_, PaleBlue, FocusBlue);
    manage_ = &CreateButton(*root_, {}, L"", "Favorites.Manage");
    ApplyButtonStyle(*manage_, PaleBlue, FocusBlue, DeepBlue);
    RequireComponent<TextVisualComponent>(*manage_).SetFontSize(14);
}
void SongFavoriteTabs::Shutdown() noexcept
{
    root_ = viewport_ = content_ = scrollbar_ = manage_ = nullptr;
    texts_ = nullptr;
    tabs_.clear();
    revision_ = ~std::uint64_t{};
}
void SongFavoriteTabs::SetBounds(Rect bounds)
{
    if (!root_) return;
    root_->SetBounds(bounds);
    Layout();
    const auto selected = std::ranges::find_if(tabs_, [this](const auto &tab) { return tab.second == selected_; });
    if (selected != tabs_.end())
    {
        const float x = static_cast<float>(selected - tabs_.begin()) * 142;
        if (x < offset_) offset_ = x;
        else if (x + 138 > offset_ + viewport_->NodeSize().width) offset_ = x + 138 - viewport_->NodeSize().width;
        Layout();
    }
}
void SongFavoriteTabs::AddTab(std::uint64_t id, std::wstring name, bool selected, float &x)
{
    constexpr float width = 138;
    auto &button = CreateButton(*content_, {x, 0, width, 27}, std::move(name), "Favorites.Tab." + std::to_string(id));
    button.SetClipRect({0, 0, width, 27});
    ApplyButtonStyle(button, selected ? AccentBlue : PureWhite, FocusBlue, selected ? PureWhite : DeepBlue);
    auto &label = RequireComponent<TextVisualComponent>(button);
    label.SetFontSize(14);
    label.SetContentBounds({6, 0, width - 12, 27});
    texts_->ApplyFont(label);
    tabs_.push_back({button.Id(), id});
    x += width + 4;
}
void SongFavoriteTabs::Refresh(const SongFavorites &favorites, std::uint64_t selected)
{
    if (!root_ || (revision_ == favorites.Revision() && selected_ == selected && textRevision_ == texts_->Revision())) return;
    for (const auto &[node, id] : tabs_) { static_cast<void>(id); static_cast<void>(content_->RemoveChild(node)); }
    tabs_.clear();
    float x{};
    AddTab(0, std::wstring{texts_->Get("Favorites.all")}, selected == 0, x);
    float selectedX{};
    for (const auto &folder : favorites.Values().folders)
    {
        if (selected == folder.id) selectedX = x;
        AddTab(folder.id, folder.name, selected == folder.id, x);
    }
    extent_ = std::max(x - 4, 0.0F);
    auto &label = RequireComponent<TextVisualComponent>(*manage_);
    label.SetText(std::wstring{texts_->Get("Favorites.manage")});
    texts_->ApplyFont(label);
    if (selected_ != selected)
    {
        const float width = viewport_->NodeSize().width;
        if (selectedX < offset_) offset_ = selectedX;
        else if (selectedX + 138 > offset_ + width) offset_ = selectedX + 138 - width;
    }
    selected_ = selected;
    revision_ = favorites.Revision();
    textRevision_ = texts_->Revision();
    Layout();
}
void SongFavoriteTabs::Layout()
{
    const float width = std::max(root_->NodeSize().width - 84, 1.0F);
    viewport_->SetBounds({0, 9, width, 27});
    viewport_->SetClipRect({0, 0, width, 27});
    offset_ = std::clamp(offset_, 0.0F, std::max(extent_ - width, 0.0F));
    content_->SetBounds({-offset_, 0, extent_, 27});
    manage_->SetBounds({width + 4, 9, 80, 27});
    scrollbar_->SetBounds({0, 0, width, 7});
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
        for (const auto &[node, id] : tabs_) if (action.source == node) return id;
    return std::nullopt;
}
bool SongFavoriteTabs::IsManageAction(const Action &action) const noexcept
{
    return manage_ && action.type == ActionType::Clicked && action.source == manage_->Id();
}
bool SongFavoriteTabs::Scroll(Point position, float wheel)
{
    if (!root_ || !root_->BoundsInCanvas().Contains(position) || wheel == 0) return false;
    offset_ -= wheel * 100;
    Layout();
    return true;
}
