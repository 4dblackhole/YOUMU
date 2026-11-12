#include "SongListView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

using namespace PhantomCore;

using namespace song_select;

void SongListView::Attach(Visual2DCanvas &canvas, Visual2DInputRouter &router, Visual2DNode &viewport)
{
    canvas_ = &canvas;
    router_ = &router;
    viewport_ = &viewport;
    viewport.SetClipRect({0.0F, 0.0F, viewport.NodeSize().width, viewport.NodeSize().height});
    content_ = &viewport.CreateChild("SongContent");
    content_->SetSize(viewport.NodeSize());
    content_->SetPosition({0.0F, 0.0F});
    content_->SetZIndex(1);
    scrollbarTrack_ = &CreateSongPanel(visuals_, viewport, layout::ScrollbarTrack, {0.835F, 0.910F, 0.957F, 1.0F},
                                       "ScrollbarTrack", 3.0F);
    scrollbarTrack_->SetZIndex(2);
    scrollbarHandle_ = &CreateSongPanel(visuals_, viewport, layout::ScrollbarTrack, AccentBlue, "ScrollbarHandle", 3.0F);
    scrollbarHandle_->SetZIndex(3);
}

void SongListView::SetLayout(const float innerWidth)
{
    if (viewport_ == nullptr)
        return;
    viewport_->SetClipRect({0.0F, 0.0F, viewport_->NodeSize().width, viewport_->NodeSize().height});
    content_->SetSize(viewport_->NodeSize());
    contentWidth_ = std::max(innerWidth - 24.0F, 1.0F);
    constexpr float tabsHeight = 64;
    const float trackX = std::max(innerWidth - 9.0F, 0.0F);
    scrollbarTrack_->SetBounds(ScaleTopLeftBounds(
        {trackX, layout::ScrollbarTrack.y, layout::ScrollbarTrack.width, layout::ScrollbarTrack.height - tabsHeight},
        viewport_->NodeSize().height));
    UpdateScrollbar();
}

void SongListView::Shutdown() noexcept
{
    difficultyButtonIds_.clear();
    cards_.clear();
    rows_.clear();
    scrollbarHandle_ = nullptr;
    scrollbarTrack_ = nullptr;
    content_ = nullptr;
    viewport_ = nullptr;
    router_ = nullptr;
    canvas_ = nullptr;
    firstVisibleRow_ = lastVisibleRow_ = 0;
    cardsDirty_ = true;
}

void SongListView::Rebuild()
{
    if (!content_ || !canvas_)
        return;
    rows_.clear();
    rows_.reserve(selection_.visibleSongIndices_.size());
    float top = layout::ContentTop;
    for (std::size_t position = 0; position < selection_.visibleSongIndices_.size(); ++position)
    {
        const auto index = selection_.visibleSongIndices_[position];
        const bool focused = position == selection_.focusedSongPosition_;
        const float height = focused ? layout::ExpandedBaseHeight +
                                           layout::DifficultyRowStep *
                                               static_cast<float>(selection_.VisiblePatternCount(index))
                                     : layout::NormalSongHeight;
        rows_.push_back({index, top, height, nullptr, nullptr, nullptr, position});
        top += height + layout::SongGap;
    }
    contentHeight_ = rows_.empty() ? VisibleBottom() : top - layout::SongGap + layout::ContentTop;
    cardsDirty_ = true;
    EnsureFocusedVisible();
}

void SongListView::UpdateVisibleCards()
{
    if (!content_ || !canvas_)
        return;
    const float begin = layout::ContentTop + scrollOffset_ - layout::NormalSongHeight;
    const float end = VisibleBottom() + scrollOffset_ + layout::NormalSongHeight;
    const auto first =
        std::ranges::lower_bound(rows_, begin, {}, [](const SongCard &row) { return row.top + row.height; });
    const auto last = std::ranges::upper_bound(first, rows_.end(), end, {}, &SongCard::top);
    const auto firstIndex = static_cast<std::size_t>(first - rows_.begin());
    const auto lastIndex = static_cast<std::size_t>(last - rows_.begin());
    if (!cardsDirty_ && firstIndex == firstVisibleRow_ && lastIndex == lastVisibleRow_)
        return;
    // Replacing cards invalidates hovered/pressed nodes before they are removed.
    router_->Reset(*canvas_);
    std::vector<NodeId> ids;
    for (const auto &child : content_->Children())
        ids.push_back(child->Id());
    for (const auto id : ids)
        static_cast<void>(content_->RemoveChild(id));
    cards_.clear();
    difficultyButtonIds_.clear();
    for (auto row = first; row != last; ++row)
        CreateCard(*row, row->visiblePosition == selection_.focusedSongPosition_);
    if (rows_.empty())
    {
        const auto &text = MusicSelect(texts_);
        static_cast<void>(CreateSongLabel(*content_,
                        {layout::EmptySongMessage.x, layout::EmptySongMessage.y, std::max(contentWidth_ - 32.0F, 1.0F),
                         layout::EmptySongMessage.height},
                        std::wstring(selection_.catalogLoading_ ? text.noDifficulties : text.noSongsAvailable), 20.0F,
                        DeepBlue, "NoSongs", texts_, TextAlignment::Center));
    }
    firstVisibleRow_ = firstIndex;
    lastVisibleRow_ = lastIndex;
    cardsDirty_ = false;
    router_->InvalidateHitTest();
}

void SongListView::CreateCard(const SongCard &row, const bool focused)
{
    const auto &text = MusicSelect(texts_);
    const auto &song = selection_.catalog_.songs[row.catalogIndex];
    const float height = row.height;
    auto &button = CreateButton(
        *content_, ScaleTopLeftBounds({layout::ContentLeft, row.top, contentWidth_, height}, content_->NodeSize().height),
        L"", std::format("Song.{}", row.visiblePosition));
    ApplyButtonStyle(button, focused ? BorderBlue : PaleBorder, BorderBlue, DeepBlue);
    SetCornerRadius(button, 14.0F);
    CreateSongPanel(visuals_, button, {2.0F, 2.0F, std::max(contentWidth_ - 4.0F, 1.0F), height - 4.0F},
                    focused ? FocusBlue : RowBlue, "Surface", 12.0F);
    BindSkinImage(button, visuals_, "MusicSelect", focused ? "SongCardFocused" : "SongCard", "Surface");

    const float titleWidth = std::max(contentWidth_ - 28.0F, 1.0F);
    const float headingWidth = editor_ ? std::max(titleWidth - 48.0F, 1.0F) : titleWidth;
    auto &title = CreateScrollableSongLabel(button, {14.0F, focused ? 16.0F : 10.0F, headingWidth, focused ? 60.0F : 56.0F},
                                            DisplayLines(song.music.names, song.metadataPath.stem().wstring()),
                                            focused ? 20.0F : 16.0F, DeepBlue, "Title", texts_);
    auto &artist = CreateScrollableSongLabel(button, {14.0F, focused ? 82.0F : 72.0F, headingWidth, focused ? 48.0F : 44.0F},
                                             DisplayLines(song.music.artists), focused ? 12.0F : 11.0F, MutedBlue,
                                             "Artist", texts_);
    RequireComponent<ScrollableTextComponent>(title).SetClickTarget(button.Id());
    RequireComponent<ScrollableTextComponent>(artist).SetClickTarget(button.Id());

    if (focused)
    {
        CreateSongPanel(visuals_, button, {14.0F, 140.0F, titleWidth, 1.0F}, PaleBorder, "Divider");
        if (song.patterns.empty())
            static_cast<void>(CreateSongLabel(button, {14.0F, 152.0F, titleWidth, 24.0F},
                                              std::wstring(text.noDifficulties), 12.0F, MutedBlue,
                                              "NoDifficulties", texts_, TextAlignment::Center));
        const auto visiblePatterns = selection_.VisiblePatternIndices(row.catalogIndex);
        for (std::size_t patternRow = 0; patternRow < visiblePatterns.size(); ++patternRow)
        {
            const auto index = visiblePatterns[patternRow];
            auto &difficulty = CreateButton(
                button,
                ScaleTopLeftBounds({14.0F, 152.0F + layout::DifficultyRowStep * static_cast<float>(patternRow),
                                    titleWidth, layout::DifficultyRowHeight},
                                   button.NodeSize().height),
                PatternName(song.patterns[index]), std::format("Difficulty.{}", index));
            const bool selected = index == selection_.selectedPatternIndex_;
            ApplyButtonStyle(difficulty, selected ? AccentBlue : PureWhite, selected ? AccentHover : PaleBlue,
                             selected ? PureWhite : DeepBlue);
            SetCornerRadius(difficulty, 8.0F);
            BindSkinImage(difficulty, visuals_, "MusicSelect", selected ? "DifficultySelected" : "Difficulty");
            auto &difficultyText = RequireComponent<TextVisualComponent>(difficulty);
            difficultyText.SetFontSize(CanvasFontSize(12.0F));
            texts_.ApplyFont(difficultyText);
            difficultyText.SetHorizontalAlignment(TextAlignment::Leading);
            difficultyText.SetContentBounds(ScaleTopLeftBounds({12.0F, 0.0F, titleWidth - 24.0F, layout::DifficultyRowHeight},
                                                               difficulty.NodeSize().height));
            difficultyButtonIds_.push_back({difficulty.Id(), index});
        }
    }
    const auto createId = editor_ ? CreateDifficultyButton(button, row.visiblePosition) : 0;
    cards_.push_back({row.catalogIndex, row.top, height, &button, &title, &artist, row.visiblePosition, createId});
}

NodeId SongListView::CreateDifficultyButton(Visual2DNode &card, const std::size_t visiblePosition)
{
    const float size = std::min(36.0F, std::max(contentWidth_ - 28.0F, 1.0F));
    auto &button = CreateButton(
        card,
        ScaleTopLeftBounds({std::max(contentWidth_ - size - 14.0F, 0.0F), 14.0F, size, size}, card.NodeSize().height),
        std::wstring(MusicSelect(texts_).addDifficulty), std::format("AddDifficulty.{}", visiblePosition));
    ApplyButtonStyle(button, PaleBlue, FocusBlue, AccentBlue);
    SetCornerRadius(button, 8.0F);
    auto &caption = RequireComponent<TextVisualComponent>(button);
    caption.SetFontSize(CanvasFontSize(28.0F));
    texts_.ApplyFont(caption);
    button.SetZIndex(3);
    button.SetEnabled(!selection_.catalogLoading_);
    BindSkinImage(button, visuals_, "MusicSelect", "AddDifficulty");
    return button.Id();
}

void SongListView::RefreshPatternSelection()
{
    if (!canvas_)
        return;
    for (const auto [id, patternIndex] : difficultyButtonIds_)
    {
        if (auto *button = canvas_->FindNode(id))
        {
            const bool selected = patternIndex == selection_.selectedPatternIndex_;
            ApplyButtonStyle(*button, selected ? AccentBlue : PureWhite, selected ? AccentHover : PaleBlue,
                             selected ? PureWhite : DeepBlue);
            BindSkinImage(*button, visuals_, "MusicSelect", selected ? "DifficultySelected" : "Difficulty");
        }
    }
}

SongListView::ActionResult SongListView::ProcessAction(const Action &action) const
{
    if (action.type != ActionType::Clicked)
        return {};
    for (const auto &card : cards_)
    {
        if (card.createDifficultyId != 0 && action.source == card.createDifficultyId)
            return {ActionKind::CreateDifficultyClicked, card.visiblePosition, card.button->BoundsInCanvas()};
        if (card.button != nullptr && action.source == card.button->Id())
            return {ActionKind::SongClicked, card.visiblePosition, {}};
    }
    for (const auto [id, patternIndex] : difficultyButtonIds_)
        if (action.source == id)
            return {ActionKind::PatternClicked, patternIndex, {}};
    return {};
}

float SongListView::VisibleBottom() const noexcept
{
    return viewport_ ? std::max(viewport_->NodeSize().height / DesignToCanvasScale - layout::ContentTop,
                                layout::ContentTop + 1)
                     : layout::ContentBottom;
}

void SongListView::EnsureFocusedVisible()
{
    if (selection_.focusedSongPosition_ >= rows_.size())
    {
        scrollOffset_ = 0.0F;
        ApplyScrollOffset();
        return;
    }
    const SongCard &card = rows_[selection_.focusedSongPosition_];
    const float visibleTop = layout::ContentTop + scrollOffset_;
    const float visibleBottom = VisibleBottom() + scrollOffset_;
    if (card.top < visibleTop)
        scrollOffset_ = card.top - layout::ContentTop;
    else if (card.top + card.height > visibleBottom)
        scrollOffset_ = card.top + card.height - VisibleBottom();
    ApplyScrollOffset();
}

void SongListView::ScrollBy(const float designPixels)
{
    scrollOffset_ += designPixels;
    ApplyScrollOffset();
}

void SongListView::ApplyScrollOffset()
{
    const float maximum = std::max(contentHeight_ - VisibleBottom(), 0.0F);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maximum);
    if (content_ != nullptr)
        content_->SetPosition({0.0F, scrollOffset_ * DesignToCanvasScale});
    UpdateScrollbar();
    UpdateVisibleCards();
    if (router_ != nullptr)
        router_->InvalidateHitTest();
}

void SongListView::UpdateScrollbar()
{
    if (scrollbarHandle_ == nullptr || viewport_ == nullptr)
        return;
    const float maximum = std::max(contentHeight_ - VisibleBottom(), 0.0F);
    const float visibleHeight = std::max(VisibleBottom() - layout::ContentTop, 1.0F);
    const float trackHeight = visibleHeight;
    const float handleHeight =
        maximum <= 0.0F
            ? trackHeight
            : std::max(48.0F, trackHeight * visibleHeight / std::max(contentHeight_ - layout::ContentTop, visibleHeight));
    const float travel = trackHeight - handleHeight;
    const float handleTop = layout::ScrollbarTrack.y + (maximum <= 0.0F ? 0.0F : travel * scrollOffset_ / maximum);
    const float trackX = std::max(viewport_->NodeSize().width / DesignToCanvasScale - 9.0F, 0.0F);
    scrollbarHandle_->SetBounds(ScaleTopLeftBounds({trackX, handleTop, layout::ScrollbarTrack.width, handleHeight},
                                                   viewport_->NodeSize().height));
}
