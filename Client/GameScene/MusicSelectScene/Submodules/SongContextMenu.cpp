#include "SongContextMenu.h"
#include "Presentation/UiColors.h"
#include "SongSelectLayout.h"
#include <algorithm>
#include <string>

using namespace PhantomCore;
using namespace song_select;

namespace
{
    namespace theme = ui_colors::theme;

    [[nodiscard]] const char *TextKey(const SongMenuItem item) noexcept
    {
        switch (item)
        {
        case SongMenuItem::NewDifficulty: return "SongMenu.newDifficulty";
        case SongMenuItem::Edit: return "SongMenu.edit";
        case SongMenuItem::DeleteDifficulty: return "SongMenu.deleteDifficulty";
        case SongMenuItem::DeleteSong: return "SongMenu.deleteSong";
        case SongMenuItem::OpenFolder: return "SongMenu.openFolder";
        case SongMenuItem::Collections: return "SongMenu.collections";
        }
        return "SongMenu.edit";
    }
    // The node name tests and skins find a row by.
    [[nodiscard]] std::string RowName(const SongMenuItem item)
    {
        return TextKey(item);
    }
} // namespace

void SongContextMenu::Initialize(Visual2DCanvas &canvas, TextCatalog &texts)
{
    canvas_ = &canvas;
    texts_ = &texts;
    plate_ = &CreatePanel(canvas.Root(), {}, "SongMenu");
    plate_->SetZIndex(900);
    ApplyFlatStyle(*plate_, theme::BrandDeep);
    SetCornerRadius(*plate_, 4.0F);
    plate_->SetVisible(false);
}

void SongContextMenu::Shutdown() noexcept
{
    rows_.clear();
    items_.clear();
    plate_ = nullptr;
    canvas_ = nullptr;
    texts_ = nullptr;
}

void SongContextMenu::Open(const Point at, std::vector<SongMenuItem> items)
{
    if (plate_ == nullptr)
        return;
    std::vector<NodeId> old;
    for (const auto &child : plate_->Children())
        old.push_back(child->Id());
    for (const auto id : old)
        static_cast<void>(plate_->RemoveChild(id));
    rows_.clear();
    items_ = std::move(items);
    const auto ruled = std::ranges::find(items_, SongMenuItem::OpenFolder);
    const bool rule = ruled != items_.begin() && ruled != items_.end();
    const float height = Padding * 2 + RowHeight * static_cast<float>(items_.size()) + (rule ? RuleGap : 0.0F);
    // Below and to the right of the pointer, moved back inside the Canvas (+Y up, origin at the centre).
    const auto canvas = canvas_->LogicalSize();
    const float left = std::clamp(at.x, -canvas.width * 0.5F, canvas.width * 0.5F - Width);
    const float bottom = std::clamp(at.y - height, -canvas.height * 0.5F, canvas.height * 0.5F - height);
    plate_->SetBounds({left, bottom, Width, height});
    float top = Padding;
    for (const auto item : items_)
    {
        if (item == SongMenuItem::OpenFolder && rule)
        {
            auto &line = CreateSprite(*plate_, {12, height - top - RuleGap * 0.5F - 0.5F, Width - 24, 1}, {}, "SongMenu.Rule");
            ApplyFlatStyle(line, {1, 1, 1, .2F});
            top += RuleGap;
        }
        auto &row = CreateButton(*plate_, {0, height - top - RowHeight, Width, RowHeight}, L"", RowName(item));
        ApplySpriteStyle(row, theme::BrandDeep, theme::Brand);
        auto &text = RequireComponent<TextVisualComponent>(row);
        text.SetFontSize(13);
        text.SetTextColor(PureWhite);
        text.SetHorizontalAlignment(TextAlignment::Leading);
        text.SetContentBounds({14, 0, Width - 28, RowHeight});
        rows_.emplace_back(&row, item);
        top += RowHeight;
    }
    RefreshTexts();
    plate_->SetVisible(true);
}

void SongContextMenu::Close() noexcept
{
    if (plate_ != nullptr)
        plate_->SetVisible(false);
}

bool SongContextMenu::Contains(const NodeId node) const noexcept
{
    if (!IsOpen() || node == 0)
        return false;
    if (node == plate_->Id())
        return true;
    return std::ranges::any_of(rows_, [node](const auto &row) { return row.first->Id() == node; });
}

std::optional<SongMenuItem> SongContextMenu::ProcessAction(const Action &action)
{
    if (!IsOpen() || action.type != ActionType::Clicked)
        return std::nullopt;
    const auto row = std::ranges::find_if(rows_, [&](const auto &entry) { return entry.first->Id() == action.source; });
    if (row == rows_.end())
        return std::nullopt;
    Close();
    return row->second;
}

void SongContextMenu::RefreshTexts()
{
    for (const auto &[row, item] : rows_)
    {
        auto &text = RequireComponent<TextVisualComponent>(*row);
        text.SetText(std::wstring(texts_->Get(TextKey(item))));
        texts_->ApplyFont(text);
    }
}
