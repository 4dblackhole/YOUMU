#include "OsuImportPanel.h"
#include "Presentation/SceneWheel.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include <algorithm>
#include <utility>

using namespace PhantomCore;

using namespace song_select;
namespace
{
    constexpr std::size_t Rows = 7;
    constexpr float RowPitch = 35, RowHeight = 30;
    using State = OsuImportDifficulty::State;
    void Text(Visual2DNode &node, std::wstring text) { RequireComponent<TextVisualComponent>(node).SetText(std::move(text)); }
    bool IsConfirmOrCancelKeyActive(const InputState &input)
    {
        if (input.IsKeyDown(VK_RETURN) || input.IsKeyDown(VK_ESCAPE)) return true;
        return input.WasKeyPressed(VK_RETURN) || input.WasKeyPressed(VK_ESCAPE);
    }
}
Visual2DNode &OsuImportPanel::Button(Visual2DNode &parent, std::string name)
{
    auto &node = CreateButton(parent, {}, L"", std::move(name));
    ApplyButtonStyle(node, PaleBlue, FocusBlue, DeepBlue);
    auto &text = RequireComponent<TextVisualComponent>(node);
    text.SetFontSize(16);
    texts_->ApplyFont(text);
    return node;
}
void OsuImportPanel::Initialize(Visual2DCanvas &canvas, TextCatalog &texts)
{
    canvas_ = &canvas;
    texts_ = &texts;
    overlay_ = &CreatePanel(canvas.Root(), {}, "OsuImport.Overlay");
    overlay_->SetZIndex(1000);
    ApplySpriteStyle(*overlay_, {0, 0, 0, .3F}, {0, 0, 0, .3F});
    overlay_->AddComponent<RectangleCollider2DComponent>();
    overlay_->AddComponent<PointerReceiverComponent>();
    panel_ = &CreatePanel(*overlay_, {}, "OsuImport.Panel");
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
    title_ = label("OsuImport.Title", 24);
    folder_ = label("OsuImport.Folder", 16);
    status_ = label("OsuImport.Status", 14);
    list_ = &panel_->CreateChild("OsuImport.Difficulties");
    import_ = &Button(*panel_, "OsuImport.Import");
    cancel_ = &Button(*panel_, "OsuImport.Cancel");
    previous_ = &Button(*panel_, "OsuImport.Previous");
    next_ = &Button(*panel_, "OsuImport.Next");
    Text(*previous_, L"↑");
    Text(*next_, L"↓");
    ApplyButtonStyle(*import_, AccentBlue, AccentHover, PureWhite);
    overlay_->SetVisible(false);
    RefreshTexts();
    Layout();
}
void OsuImportPanel::Open(std::wstring folder, std::vector<OsuImportDifficulty> difficulties)
{
    folderName_ = std::move(folder);
    difficulties_ = std::move(difficulties);
    checked_.assign(difficulties_.size(), false);
    for (std::size_t index = 0; index < difficulties_.size(); ++index)
        checked_[index] = difficulties_[index].state == State::Ready;
    selected_.reset();
    first_ = 0;
    choice_.reset();
    // The Enter or click that opened the folder dialog must not answer this list.
    waitRelease_ = true;
    overlay_->SetVisible(true);
    RefreshTexts();
    Layout();
}
void OsuImportPanel::Close() noexcept
{
    if (!IsVisible()) return;
    overlay_->SetVisible(false);
    if (!choice_) choice_ = OsuImportChoice{};
}
void OsuImportPanel::Shutdown() noexcept
{
    if (overlay_) overlay_->SetVisible(false);
    overlay_ = panel_ = nullptr;
    canvas_ = nullptr;
    texts_ = nullptr;
    rowIds_.clear();
    checkIds_.clear();
}
void OsuImportPanel::SetViewport() { if (panel_) Layout(); }
void OsuImportPanel::Layout()
{
    const auto logical = canvas_->LogicalSize();
    const float width = std::max(280.0F, std::min(640.0F, logical.width - 24));
    constexpr float height = 452;
    overlay_->SetBounds({-logical.width / 2, -logical.height / 2, logical.width, logical.height});
    panel_->SetBounds({(logical.width - width) / 2, (logical.height - height) / 2, width, height});
    title_->SetBounds({24, 400, width - 48, 30});
    folder_->SetBounds({24, 364, width - 48, 26});
    folder_->SetClipRect({0, 0, width - 48, 26});
    list_->SetBounds({24, 108, width - 88, 245});
    previous_->SetBounds({width - 54, 321, 30, 32});
    next_->SetBounds({width - 54, 108, 30, 32});
    status_->SetBounds({24, 56, width - 48, 44});
    status_->SetClipRect({0, 0, width - 48, 44});
    cancel_->SetBounds({width - 240, 12, 88, 32});
    import_->SetBounds({width - 144, 12, 120, 32});
    RefreshRows();
}
void OsuImportPanel::RefreshTexts()
{
    if (!panel_) return;
    for (const auto &[node, key] : std::vector<std::pair<Visual2DNode *, const char *>>{
        {title_, "OsuImport.title"}, {import_, "OsuImport.import"}, {cancel_, "OsuImport.cancel"}})
    {
        Text(*node, std::wstring{texts_->Get(key)});
        texts_->ApplyFont(RequireComponent<TextVisualComponent>(*node));
    }
    Text(*folder_, folderName_);
    for (auto *node : {folder_, status_}) texts_->ApplyFont(RequireComponent<TextVisualComponent>(*node));
    RefreshRows();
}
std::wstring OsuImportPanel::RowText(const OsuImportDifficulty &difficulty) const
{
    // A difficulty that was never converted has no name of its own, so its file name stands in.
    auto text = difficulty.name.empty() ? difficulty.source.filename().native() : DecodeDisplayText(difficulty.name);
    if (!difficulty.maker.empty()) text += L"  ·  " + DecodeDisplayText(difficulty.maker);
    if (difficulty.state == State::AlreadyImported) text += L"  —  " + std::wstring{texts_->Get("OsuImport.exists")};
    if (difficulty.state == State::Failed) text += L"  —  " + std::wstring{texts_->Get("OsuImport.failed")};
    return text;
}
void OsuImportPanel::RefreshRows()
{
    std::vector<NodeId> children;
    for (const auto &child : list_->Children()) children.push_back(child->Id());
    for (const auto id : children) static_cast<void>(list_->RemoveChild(id));
    rowIds_.clear();
    checkIds_.clear();
    first_ = std::min(first_, difficulties_.size() > Rows ? difficulties_.size() - Rows : 0);
    const float width = list_->NodeSize().width;
    for (std::size_t row = 0; row < Rows && first_ + row < difficulties_.size(); ++row)
    {
        const std::size_t index = first_ + row;
        const auto &difficulty = difficulties_[index];
        const bool ready = difficulty.state == State::Ready;
        const float y = static_cast<float>(Rows - 1 - row) * RowPitch;
        auto &check = Button(*list_, "OsuImport.Check." + std::to_string(index));
        check.SetBounds({0, y, 30, RowHeight});
        check.SetEnabled(ready);
        Text(check, checked_[index] ? L"✓" : L"");
        ApplyButtonStyle(check, checked_[index] ? AccentBlue : PaleBlue, FocusBlue, checked_[index] ? PureWhite : DeepBlue);
        auto &name = Button(*list_, "OsuImport.Difficulty." + std::to_string(index));
        name.SetBounds({38, y, std::max(width - 38, 1.0F), RowHeight});
        name.SetClipRect({0, 0, name.NodeSize().width, RowHeight});
        auto &label = RequireComponent<TextVisualComponent>(name);
        label.SetHorizontalAlignment(TextAlignment::Leading);
        label.SetContentBounds({6, 0, std::max(width - 50, 1.0F), RowHeight});
        Text(name, RowText(difficulty));
        ApplyButtonStyle(name, selected_ == index ? FocusBlue : PureWhite, PaleBlue, ready ? DeepBlue : MutedBlue);
        rowIds_.push_back({name.Id(), index});
        checkIds_.push_back({check.Id(), index});
    }
    previous_->SetEnabled(first_ > 0);
    next_->SetEnabled(first_ + Rows < difficulties_.size());
    import_->SetEnabled(std::ranges::find(checked_, true) != checked_.end());
    RefreshStatus();
}
void OsuImportPanel::RefreshStatus()
{
    // The selected row explains itself; otherwise the line says what the buttons do.
    const bool failedSelected = selected_ && difficulties_[*selected_].state == State::Failed;
    const bool anyReady = std::ranges::any_of(difficulties_, [](const auto &entry) { return entry.state == State::Ready; });
    std::wstring text{texts_->Get(anyReady ? "OsuImport.hint" : "OsuImport.none")};
    if (failedSelected) text = DecodeDisplayText(difficulties_[*selected_].problem);
    Text(*status_, std::move(text));
    RequireComponent<TextVisualComponent>(*status_).SetTextColor(failedSelected ? ui_colors::Error : MutedBlue);
}
void OsuImportPanel::MoveList(int delta)
{
    const auto maximum = difficulties_.size() > Rows ? difficulties_.size() - Rows : 0;
    first_ = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(first_) + delta, 0, static_cast<std::ptrdiff_t>(maximum)));
    RefreshRows();
}
void OsuImportPanel::Answer(const bool import)
{
    OsuImportChoice choice{import, {}};
    for (std::size_t index = 0; import && index < checked_.size(); ++index)
        if (checked_[index]) choice.indices.push_back(index);
    // Import without a checked difficulty is not an answer; the button is disabled for it.
    if (import && choice.indices.empty()) return;
    choice_ = std::move(choice);
    overlay_->SetVisible(false);
}
bool OsuImportPanel::ProcessAction(const Action &action)
{
    if (!IsVisible() || action.type != ActionType::Clicked) return false;
    if (action.source == cancel_->Id()) { Answer(false); return true; }
    if (action.source == import_->Id()) { Answer(true); return true; }
    if (action.source == previous_->Id()) { MoveList(-1); return true; }
    if (action.source == next_->Id()) { MoveList(1); return true; }
    for (const auto &[node, index] : rowIds_) if (action.source == node)
    {
        selected_ = index;
        RefreshRows();
        return true;
    }
    for (const auto &[node, index] : checkIds_) if (action.source == node)
    {
        if (difficulties_[index].state == State::Ready) checked_[index] = !checked_[index];
        RefreshRows();
        return true;
    }
    return false;
}
void OsuImportPanel::ProcessInput(const InputState &input, std::optional<Point> pointer)
{
    if (!IsVisible()) return;
    if (pointer && list_->BoundsInCanvas().Contains(*pointer) && SceneWheelDelta(input) != 0)
        MoveList(SceneWheelDelta(input) > 0 ? -1 : 1);
    if (waitRelease_)
    {
        if (!IsConfirmOrCancelKeyActive(input))
            waitRelease_ = false;
        return;
    }
    if (input.WasKeyPressed(VK_ESCAPE)) Answer(false);
    else if (input.WasKeyPressed(VK_RETURN)) Answer(true);
}
std::optional<OsuImportChoice> OsuImportPanel::TakeChoice() { return std::exchange(choice_, std::nullopt); }
