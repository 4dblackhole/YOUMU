#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include <algorithm>
#include <cmath>

using namespace song_select;

namespace
{
    std::uint32_t NativeColor(Color color)
    {
        const auto byte = [](float value) { return static_cast<std::uint32_t>(std::round(std::clamp(value, 0.0F, 1.0F) * 255)); };
        return (byte(color.red) << 16) | (byte(color.green) << 8) | byte(color.blue);
    }
}

void MusicSelectView::SetSearchFocus(bool focused, bool selectAll)
{
    searchFocused_ = focused;
    if (searchInput_)
    {
        if (focused) searchInput_->Focus(selectAll);
        else
        {
            searchInput_->Blur();
            SynchronizeSearchText();
        }
    }
    RefreshSearchPresentation();
}

void MusicSelectView::SynchronizeSearchText()
{
    if (!searchInput_) return;
    std::wstring text;
    if (searchInput_->TakeText(text) && text != selection_.searchText_)
    {
        selection_.searchText_ = std::move(text);
        RebuildVisibleSongs();
    }
    searchFocused_ = searchInput_->HasFocus();
}

void MusicSelectView::UpdateSearchInputVisibility()
{
    if (!searchInput_) return;
    const bool available = visible_ && !IsSelectionModalVisible() && !options_.IsVisible() &&
                           !(additionMenu_ && additionMenu_->IsVisible());
    searchInput_->SetVisible(available);
    if (!available) searchFocused_ = false;
}

void MusicSelectView::UpdateSearchInputLayout()
{
    if (!searchInput_ || !canvas_ || !searchField_ || height_ == 0) return;
    const auto field = searchField_->BoundsInCanvas();
    const auto &text = RequireComponent<TextVisualComponent>(*searchField_);
    const auto content = text.ContentBounds();
    const auto logical = canvas_->LogicalSize();
    const float scale = static_cast<float>(height_) / logical.height;
    const auto fontHeight = static_cast<std::uint32_t>(std::max(1.0F, std::round(text.FontSize() * scale)));
    const float editHeight = std::min(field.height * scale, static_cast<float>(fontHeight) + 4.0F);
    // Canvas is centered/Y-up; native control bounds are top-left physical pixels.
    const float x = (field.x + content.x + logical.width / 2.0F) * scale;
    const float y = (logical.height / 2.0F - field.y - field.height / 2.0F) * scale - editHeight / 2.0F;
    searchInput_->SetBounds({static_cast<std::int32_t>(std::round(x)),
                            static_cast<std::int32_t>(std::round(y)),
                            static_cast<std::uint32_t>(std::max(1.0F, std::round(content.width * scale))),
                            static_cast<std::uint32_t>(std::max(1.0F, std::round(editHeight)))});
    const auto &font = text.Font();
    searchInput_->SetFont(font.source == TextFontSource::System ? std::wstring_view(font.value) : L"Segoe UI", fontHeight);
    searchInput_->SetColors(NativeColor(text.TextColor()), NativeColor(PureWhite));
}
