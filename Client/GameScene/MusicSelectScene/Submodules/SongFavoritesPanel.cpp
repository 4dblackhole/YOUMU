#include "SongFavoritesPanel.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Presentation/ValueDialog.h"
#include <algorithm>
#include <utility>

using namespace song_select;
namespace
{
    constexpr std::size_t Rows = 6;
    void Text(Visual2DNode &node, std::wstring text) { RequireComponent<TextVisualComponent>(node).SetText(std::move(text)); }
}
Visual2DNode &SongFavoritesPanel::Button(Visual2DNode &parent, std::string name)
{
    auto &node = CreateButton(parent, {}, L"", std::move(name));
    ApplyButtonStyle(node, PaleBlue, FocusBlue, DeepBlue);
    auto &text = RequireComponent<TextVisualComponent>(node);
    text.SetFontSize(16);
    texts_->ApplyFont(text);
    return node;
}
void SongFavoritesPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts)
{
    canvas_ = &canvas;
    texts_ = &texts;
    overlay_ = &CreatePanel(canvas.Root(), {}, "Favorites.Overlay");
    overlay_->SetZIndex(1000);
    ApplySpriteStyle(*overlay_, {0, 0, 0, .3F}, {0, 0, 0, .3F});
    overlay_->AddComponent<RectangleCollider2DComponent>();
    overlay_->AddComponent<PointerReceiverComponent>();
    panel_ = &CreatePanel(*overlay_, {}, "Favorites.Panel");
    ApplySpriteStyle(*panel_, PanelWhite, PanelWhite);
    const auto label = [this](const char *name, float size)
    {
        auto &node = CreateLabel(*panel_, {}, L"", name);
        auto &text = RequireComponent<TextVisualComponent>(node);
        text.SetFontSize(size);
        text.SetTextColor(DeepBlue);
        texts_->ApplyFont(text);
        return &node;
    };
    title_ = label("Favorites.Title", 24);
    pattern_ = label("Favorites.Pattern", 16);
    status_ = label("Favorites.Status", 14);
    list_ = &panel_->CreateChild("Favorites.Folders");
    create_ = &Button(*panel_, "Favorites.Create");
    rename_ = &Button(*panel_, "Favorites.Rename");
    remove_ = &Button(*panel_, "Favorites.Delete");
    apply_ = &Button(*panel_, "Favorites.Apply");
    close_ = &Button(*panel_, "Favorites.Close");
    previous_ = &Button(*panel_, "Favorites.Previous");
    next_ = &Button(*panel_, "Favorites.Next");
    Text(*previous_, L"↑");
    Text(*next_, L"↓");
    ApplyButtonStyle(*apply_, AccentBlue, AccentHover, PureWhite);
    overlay_->SetVisible(false);
    RefreshTexts();
    Layout();
}
void SongFavoritesPanel::Open(const SongFavorites &favorites, const std::filesystem::path &pattern, std::wstring name)
{
    draft_ = favorites.Values();
    key_ = SongFavorites::PatternKey(pattern);
    patternName_ = std::move(name);
    error_ = favorites.LoadWarning();
    selected_ = draft_.folders.empty() ? 0 : draft_.folders.front().id;
    first_ = 0;
    applied_.reset();
    waitRelease_ = discardKeys_ = false;
    overlay_->SetVisible(true);
    RefreshTexts();
    Layout();
}
void SongFavoritesPanel::Close() noexcept
{
    if (overlay_) overlay_->SetVisible(false);
    applied_.reset();
}
void SongFavoritesPanel::Shutdown() noexcept
{
    Close();
    overlay_ = panel_ = nullptr;
    canvas_ = nullptr;
    texts_ = nullptr;
    rowIds_.clear();
    memberIds_.clear();
}
void SongFavoritesPanel::SetViewport() { if (panel_) Layout(); }
void SongFavoritesPanel::Layout()
{
    const auto logical = canvas_->LogicalSize();
    const float width = std::max(280.0F, std::min(600.0F, logical.width - 24));
    constexpr float height = 452;
    overlay_->SetBounds({-logical.width / 2, -logical.height / 2, logical.width, logical.height});
    panel_->SetBounds({(logical.width - width) / 2, (logical.height - height) / 2, width, height});
    title_->SetBounds({24, 400, width - 150, 30});
    close_->SetBounds({width - 104, 400, 80, 30});
    pattern_->SetBounds({24, 352, width - 48, 36});
    pattern_->SetClipRect({0, 0, width - 48, 36});
    list_->SetBounds({24, 132, width - 88, 210});
    previous_->SetBounds({width - 54, 307, 30, 32});
    next_->SetBounds({width - 54, 132, 30, 32});
    const float third = (width - 64) / 3;
    create_->SetBounds({24, 90, third, 32});
    rename_->SetBounds({32 + third, 90, third, 32});
    remove_->SetBounds({40 + third * 2, 90, third, 32});
    status_->SetBounds({24, 53, width - 48, 32});
    apply_->SetBounds({width - 144, 12, 120, 32});
    RefreshRows();
}
void SongFavoritesPanel::RefreshTexts()
{
    if (!panel_) return;
    for (const auto &[node, key] : std::vector<std::pair<Visual2DNode *, const char *>>{
        {title_, "Favorites.title"}, {create_, "Favorites.create"}, {rename_, "Favorites.rename"},
        {remove_, "Favorites.remove"}, {apply_, "Favorites.apply"}, {close_, "Favorites.close"}})
    {
        Text(*node, std::wstring{texts_->Get(key)});
        texts_->ApplyFont(RequireComponent<TextVisualComponent>(*node));
    }
    Text(*pattern_, key_.empty() ? std::wstring{texts_->Get("Favorites.noPattern")} :
        std::wstring{texts_->Get("Favorites.pattern")} + patternName_);
    Text(*status_, error_.empty() ? std::wstring{texts_->Get("Favorites.hint")} :
        std::wstring{texts_->Get("Favorites.error")} + DecodeDisplayText(error_));
    for (auto *node : {pattern_, status_}) texts_->ApplyFont(RequireComponent<TextVisualComponent>(*node));
    RequireComponent<TextVisualComponent>(*status_).SetTextColor(error_.empty() ? MutedBlue : ui_colors::Error);
    RefreshRows();
}
void SongFavoritesPanel::RefreshRows()
{
    std::vector<NodeId> children;
    for (const auto &child : list_->Children()) children.push_back(child->Id());
    for (const auto id : children) static_cast<void>(list_->RemoveChild(id));
    rowIds_.clear();
    memberIds_.clear();
    first_ = std::min(first_, draft_.folders.size() > Rows ? draft_.folders.size() - Rows : 0);
    const float width = list_->NodeSize().width;
    for (std::size_t row = 0; row < Rows && first_ + row < draft_.folders.size(); ++row)
    {
        const auto &folder = draft_.folders[first_ + row];
        const float y = 175 - static_cast<float>(row) * 35;
        auto &member = Button(*list_, "Favorites.Member." + std::to_string(folder.id));
        member.SetBounds({0, y, 30, 30});
        member.SetEnabled(!key_.empty());
        const bool checked = !key_.empty() && folder.patternKeys.contains(key_);
        Text(member, checked ? L"✓" : L"");
        ApplyButtonStyle(member, checked ? AccentBlue : PaleBlue, FocusBlue, checked ? PureWhite : DeepBlue);
        auto &name = Button(*list_, "Favorites.Folder." + std::to_string(folder.id));
        name.SetBounds({38, y, std::max(width - 38, 1.0F), 30});
        name.SetClipRect({0, 0, name.NodeSize().width, 30});
        auto &label = RequireComponent<TextVisualComponent>(name);
        label.SetHorizontalAlignment(TextAlignment::Leading);
        label.SetContentBounds({6, 0, std::max(width - 50, 1.0F), 30});
        Text(name, folder.name);
        ApplyButtonStyle(name, selected_ == folder.id ? FocusBlue : PureWhite, PaleBlue, DeepBlue);
        rowIds_.push_back({name.Id(), folder.id});
        memberIds_.push_back({member.Id(), folder.id});
    }
    if (draft_.folders.empty())
    {
        auto &empty = CreateLabel(*list_, {0, 60, width, 70}, std::wstring{texts_->Get("Favorites.empty")}, "Favorites.Empty");
        auto &text = RequireComponent<TextVisualComponent>(empty);
        text.SetTextColor(MutedBlue);
        text.SetFontSize(16);
        texts_->ApplyFont(text);
    }
    rename_->SetEnabled(draft_.Find(selected_) != nullptr);
    remove_->SetEnabled(draft_.Find(selected_) != nullptr);
    previous_->SetEnabled(first_ > 0);
    next_->SetEnabled(first_ + Rows < draft_.folders.size());
}
void SongFavoritesPanel::MoveList(int delta)
{
    const auto maximum = draft_.folders.size() > Rows ? draft_.folders.size() - Rows : 0;
    first_ = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(first_) + delta, 0, static_cast<std::ptrdiff_t>(maximum)));
    RefreshRows();
}
void SongFavoritesPanel::EditName(bool create)
{
    const auto *folder = draft_.Find(selected_);
    if (!create && !folder) return;
    std::vector<DialogValue> fields{{std::wstring{texts_->Get("Favorites.name")}, create ? L"" : folder->name}};
    const bool accepted = ShowValueDialog(std::wstring{texts_->Get(create ? "Favorites.create" : "Favorites.rename")}, fields,
        std::wstring{texts_->Get("Favorites.ok")}, std::wstring{texts_->Get("Favorites.cancel")}, [&]
        { fields[0].value = SongFavoritesValues::FolderName(fields[0].value); });
    discardKeys_ = waitRelease_ = true;
    if (!accepted) return;
    if (create)
    {
        selected_ = draft_.Create(std::move(fields[0].value));
        first_ = draft_.folders.size() > Rows ? draft_.folders.size() - Rows : 0;
    }
    else draft_.Rename(selected_, std::move(fields[0].value));
    RefreshRows();
}
bool SongFavoritesPanel::ProcessAction(const Action &action)
{
    if (!IsVisible() || action.type != ActionType::Clicked) return false;
    if (action.source == close_->Id()) { Close(); return true; }
    if (action.source == apply_->Id()) { applied_ = draft_; return true; }
    try
    {
        if (action.source == create_->Id()) { EditName(true); return true; }
        if (action.source == rename_->Id()) { EditName(false); return true; }
        if (action.source == remove_->Id())
        {
            draft_.Remove(selected_);
            selected_ = draft_.folders.empty() ? 0 : draft_.folders.front().id;
            RefreshRows();
            return true;
        }
        if (action.source == previous_->Id()) { MoveList(-1); return true; }
        if (action.source == next_->Id()) { MoveList(1); return true; }
        for (const auto &[node, id] : rowIds_) if (action.source == node)
        {
            selected_ = id;
            RefreshRows();
            return true;
        }
        for (const auto &[node, id] : memberIds_) if (action.source == node && !key_.empty())
        {
            const auto *folder = draft_.Find(id);
            if (folder) draft_.SetMembership(id, key_, !folder->patternKeys.contains(key_));
            RefreshRows();
            return true;
        }
    }
    catch (const std::exception &exception) { ShowError(exception.what()); return true; }
    return false;
}
void SongFavoritesPanel::ProcessInput(const InputState &input, std::optional<Point> pointer)
{
    if (!IsVisible()) return;
    if (pointer && list_->BoundsInCanvas().Contains(*pointer) && input.MouseWheelDelta() != 0)
        MoveList(input.MouseWheelDelta() > 0 ? -1 : 1);
    if (discardKeys_) { discardKeys_ = false; return; }
    if (waitRelease_)
    {
        if (!input.IsKeyDown(VK_RETURN) && !input.IsKeyDown(VK_ESCAPE) && !input.WasKeyPressed(VK_RETURN) && !input.WasKeyPressed(VK_ESCAPE))
            waitRelease_ = false;
        return;
    }
    if (input.WasKeyPressed(VK_ESCAPE)) Close();
    else if (input.WasKeyPressed(VK_RETURN)) applied_ = draft_;
}
std::optional<SongFavoritesValues> SongFavoritesPanel::TakeApplied() { return std::exchange(applied_, std::nullopt); }
void SongFavoritesPanel::ShowError(std::string error) { error_ = std::move(error); RefreshTexts(); }
