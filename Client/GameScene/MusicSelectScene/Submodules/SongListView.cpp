#include "SongListView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

#include <cmath>
#include <numbers>

using namespace PhantomCore;

using namespace song_select;

namespace
{
    namespace theme = ui_colors::theme;

    constexpr Color TranslucentRow{1.0F, 1.0F, 1.0F, 0.16F};
    constexpr Color CardWhite{1.0F, 1.0F, 1.0F, 0.92F};
    constexpr Color FocusedArtist{0.949F, 0.969F, 1.0F, 1.0F};

    // Height of the selected card: its heading plus one row per difficulty. A song
    // without charts keeps one row for the NO DIFFICULTIES line.
    [[nodiscard]] constexpr float FocusedHeight(const std::size_t difficulties) noexcept
    {
        return layout::DifficultyTop + layout::DifficultyPitch * static_cast<float>(std::max<std::size_t>(difficulties, 1)) +
               layout::FocusedPadding;
    }
    static_assert(FocusedHeight(3) == 172.0F);

    // The now-playing mark: three gold bars before the title rising and falling out of step.
    constexpr float NowPlayingWidth = 13.0F, NowPlayingHeight = 12.0F, NowPlayingRoom = 21.0F;
    constexpr float NowPlayingBar = 3.0F, NowPlayingPitch = 5.0F;
    constexpr double NowPlayingPeriod = 0.8, NowPlayingStagger = 0.27;

    // 30% -> 100% -> 30% of the mark's height every 0.8 s with eased turns.
    [[nodiscard]] float NowPlayingLevel(const double seconds) noexcept
    {
        const double phase = std::fmod(seconds / NowPlayingPeriod, 1.0);
        const double rise = phase < 0.5 ? phase * 2.0 : (1.0 - phase) * 2.0;
        const double eased = 0.5 - 0.5 * std::cos(rise * std::numbers::pi);
        return static_cast<float>(0.3 + 0.7 * eased);
    }

    void LayoutNowPlaying(Visual2DNode &mark, const double seconds)
    {
        float x = 0.0F;
        double ahead = 0.0;
        // Bars stand on the mark's bottom edge.
        for (const auto &bar : mark.Children())
        {
            bar->SetBounds({x, 0.0F, NowPlayingBar, NowPlayingHeight * NowPlayingLevel(seconds + ahead)});
            x += NowPlayingPitch;
            ahead += NowPlayingStagger;
        }
    }

    Visual2DNode &CreateNowPlaying(Visual2DNode &card, const float left)
    {
        auto &mark = card.CreateChild("NowPlaying");
        mark.SetBounds(TopLeft({left, 15.0F, NowPlayingWidth, NowPlayingHeight}, card.NodeSize().height));
        for (int bar = 0; bar < 3; ++bar)
            RequireComponent<SpriteVisualComponent>(CreateSprite(mark, {}, {}, "Bar"))
                .SetStyle(VisualStyle{theme::Sun, theme::Sun, theme::Sun, theme::Sun});
        LayoutNowPlaying(mark, 0.0);
        mark.AddComponent<AnimatorComponent>([clock = 0.0](Visual2DNode &node, const double elapsed) mutable {
            clock += elapsed;
            LayoutNowPlaying(node, clock);
        });
        mark.SetVisible(false);
        return mark;
    }

    void StyleDifficulty(Visual2DNode &button, ScreenVisual2DManager &visuals, const bool selected)
    {
        ApplySkinPart(button, visuals, selected ? "DifficultySelected" : "Difficulty",
                      selected ? PureWhite : TranslucentRow);
        const Color color = selected ? theme::Brand : PureWhite;
        RequireComponent<TextVisualComponent>(button).SetTextColor(color);
        for (const auto &child : button.Children())
            if (child->Name() == "Level")
                RequireComponent<TextVisualComponent>(*child).SetTextColor(color);
    }
} // namespace

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
    scrollbar_.Attach(viewport, "Scrollbar");
    scrollbar_.SetZIndex(3);
}

void SongListView::SetLayout()
{
    if (viewport_ == nullptr)
        return;
    const Size size = viewport_->NodeSize();
    viewport_->SetClipRect({0.0F, 0.0F, size.width, size.height});
    content_->SetSize(size);
    scrollbar_.SetRail({size.width - layout::ScrollbarWidth - 4.0F, layout::ContentTop, layout::ScrollbarWidth,
                        std::max(size.height - layout::ContentTop * 2.0F, 1.0F)});
    UpdateScrollbar();
}

void SongListView::Shutdown() noexcept
{
    difficultyButtonIds_.clear();
    cards_.clear();
    rows_.clear();
    scrollbar_.Detach();
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
        const float height = focused ? FocusedHeight(selection_.VisiblePatternCount(index)) : layout::SongHeight;
        rows_.push_back({index, top, height, nullptr, nullptr, nullptr, position});
        top += height + (layout::SongPitch - layout::SongHeight);
    }
    contentHeight_ = rows_.empty() ? VisibleBottom()
                                   : top - (layout::SongPitch - layout::SongHeight) + layout::ContentTop;
    cardsDirty_ = true;
    EnsureFocusedVisible();
}

void SongListView::UpdateVisibleCards()
{
    if (!content_ || !canvas_)
        return;
    const float begin = layout::ContentTop + scrollOffset_ - layout::SongPitch;
    const float end = VisibleBottom() + scrollOffset_ + layout::SongPitch;
    const auto first =
        std::ranges::lower_bound(rows_, begin, {}, [](const SongCard &row) { return row.top + row.height; });
    const auto last = std::ranges::upper_bound(first, rows_.end(), end, {}, &SongCard::top);
    const auto firstIndex = static_cast<std::size_t>(first - rows_.begin());
    const auto lastIndex = static_cast<std::size_t>(last - rows_.begin());
    if (!cardsDirty_ && firstIndex == firstVisibleRow_ && lastIndex == lastVisibleRow_)
        return;
    // Replacing cards invalidates hovered/pressed nodes before they are removed.
    router_->Reset(*canvas_);
    ClearNameHints();
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
        // An empty catalog shows one line; an empty search or filter result names the cause.
        // The first catalog load shows no message at all.
        const auto &text = MusicSelect(texts_);
        const bool catalogEmpty = selection_.catalog_.songs.empty();
        if (!(catalogEmpty && selection_.catalogLoading_))
            static_cast<void>(CreateNumber(*content_, {layout::SongLeft, 120.0F, layout::SongWidth, 28.0F},
                                       std::wstring(catalogEmpty ? text.noDifficulties : text.noSongsAvailable), 20.0F,
                                       700, theme::Caption, "NoSongs", TextAlignment::Center, false));
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
    const float left = focused ? layout::SongLeft - layout::FocusedOutset : layout::SongLeft;
    const float width = focused ? layout::SongWidth + layout::FocusedOutset : layout::SongWidth;
    auto &button = CreateButton(*content_, TopLeft({left, row.top, width, height}, content_->NodeSize().height), L"",
                                std::format("Song.{}", row.visiblePosition));
    ApplySkinPart(button, visuals_, focused ? "SongCardFocused" : "SongCard", focused ? theme::Brand : CardWhite);

    // One line each: the representative title and artist. The editor keeps room for its + button.
    const float textLeft = focused ? 34.0F : 28.0F;
    const float textWidth = std::max(width - textLeft - (editor_ ? 56.0F : 44.0F), 1.0F);
    auto &title = CreateCaption(button, {textLeft, 10.0F, textWidth, 20.0F}, SongTitle(song, selection_.notation_), focused ? 17.0F : 15.0F,
                                focused ? std::uint16_t{900} : std::uint16_t{800}, focused ? PureWhite : theme::Ink,
                                "Title");
    title.SetClipRect({0.0F, 0.0F, textWidth, 20.0F});
    auto &artist = CreateCaption(button, {textLeft, 30.0F, textWidth, 18.0F}, ArtistName(song, selection_.notation_), 13.0F, 600,
                                 focused ? FocusedArtist : theme::Caption, "Artist");
    artist.SetClipRect({0.0F, 0.0F, textWidth, 18.0F});

    if (focused)
    {
        const float rowLeft = 34.0F;
        if (song.patterns.empty())
            static_cast<void>(CreateNumber(button, {rowLeft, layout::DifficultyTop, layout::DifficultyWidth,
                                                    layout::DifficultyHeight},
                                           std::wstring(text.noDifficulties), 14.0F, 700, PureWhite,
                                           "NoDifficulties", TextAlignment::Leading, false));
        const auto visiblePatterns = selection_.VisiblePatternIndices(row.catalogIndex);
        for (std::size_t patternRow = 0; patternRow < visiblePatterns.size(); ++patternRow)
        {
            const auto index = visiblePatterns[patternRow];
            auto &difficulty = CreateButton(
                button,
                TopLeft({rowLeft, layout::DifficultyTop + layout::DifficultyPitch * static_cast<float>(patternRow),
                         layout::DifficultyWidth, layout::DifficultyHeight},
                        button.NodeSize().height),
                PatternName(song.patterns[index], selection_.notation_), std::format("Difficulty.{}", index));
            auto &difficultyText = RequireComponent<TextVisualComponent>(difficulty);
            StyleText(difficultyText, 13.0F, 800, PureWhite);
            difficultyText.SetContentBounds({20.0F, 0.0F, layout::DifficultyWidth - 20.0F - 60.0F, layout::DifficultyHeight});
            // A chart without a calculated difficulty shows a dash.
            const auto &level = song.patterns[index].calculatedDifficulty;
            static_cast<void>(CreateNumber(difficulty, {layout::DifficultyWidth - 60.0F, 0.0F, 48.0F, layout::DifficultyHeight},
                                           level ? std::format(L"{:.2f}", *level) : std::wstring(L"—"), 14.0F, 800,
                                           PureWhite, "Level", TextAlignment::Trailing));
            StyleDifficulty(difficulty, visuals_, index == selection_.selectedPatternIndex_);
            difficultyButtonIds_.push_back({difficulty.Id(), index});
        }
    }
    const auto createId = editor_ ? CreateDifficultyButton(button, row.visiblePosition) : 0;
    auto *nowPlaying = focused ? &CreateNowPlaying(button, textLeft) : nullptr;
    cards_.push_back({row.catalogIndex, row.top, height, &button, &title, &artist, row.visiblePosition, createId,
                      nowPlaying, textLeft, textWidth});
    ApplyNowPlaying(cards_.back());
}

void SongListView::SetNowPlaying(const bool playing)
{
    if (playing == nowPlaying_)
        return;
    nowPlaying_ = playing;
    for (const auto &card : cards_)
        ApplyNowPlaying(card);
}

bool SongListView::IsNowPlayingShown() const noexcept
{
    return std::ranges::any_of(cards_, [](const SongCard &card) {
        return card.nowPlaying != nullptr && card.nowPlaying->IsVisible();
    });
}

void SongListView::ApplyNowPlaying(const SongCard &card) const
{
    if (card.nowPlaying == nullptr)
        return;
    // The mark takes room before the title, which keeps its right edge.
    card.nowPlaying->SetVisible(nowPlaying_);
    const float shift = nowPlaying_ ? NowPlayingRoom : 0.0F;
    const float width = std::max(card.textWidth - shift, 1.0F);
    card.title->SetBounds(TopLeft({card.textLeft + shift, 10.0F, width, 20.0F}, card.button->NodeSize().height));
    card.title->SetClipRect({0.0F, 0.0F, width, 20.0F});
}

void SongListView::RefreshNameHints()
{
    if (tooltip_ == nullptr)
        return;
    const auto &songs = selection_.catalog_.songs;
    for (auto &card : cards_)
    {
        if (card.catalogIndex >= songs.size())
            continue;
        const auto &song = songs[card.catalogIndex];
        HintName(*card.title, song.music.names, song.music.romanizedName, card.titleHinted);
        HintName(*card.artist, song.music.artists, song.music.romanizedArtist, card.artistHinted);
    }
}

void SongListView::HintName(Visual2DNode &line, const std::vector<std::string> &names, const std::string_view romanized,
                            bool &hinted)
{
    // A line hints every name when it has several, or the full one when its line cuts it.
    const auto &text = RequireComponent<TextVisualComponent>(line);
    const auto measured = text.MeasuredSize();
    const auto &clip = line.ClipRect();
    const bool cut = measured.has_value() && clip.has_value() && measured->width > clip->width + 0.5F;
    const bool wanted = cut || NameCount(names, romanized) > 1;
    if (wanted == hinted)
        return;
    hinted = wanted;
    if (!wanted)
    {
        tooltip_->SetLine(line, {});
        return;
    }
    auto all = AllNames(names, romanized);
    tooltip_->SetLine(line, {all.empty() ? std::wstring(text.Text()) : std::move(all)});
}

void SongListView::ClearNameHints()
{
    if (tooltip_ == nullptr)
        return;
    for (auto &card : cards_)
    {
        // Only lines that were hinted are registered; the rest need no call.
        if (card.titleHinted)
            tooltip_->Set(*card.title, {});
        if (card.artistHinted)
            tooltip_->Set(*card.artist, {});
        card.titleHinted = card.artistHinted = false;
    }
}

NodeId SongListView::CreateDifficultyButton(Visual2DNode &card, const std::size_t visiblePosition)
{
    constexpr float size = 32.0F;
    auto &button = CreateButton(card, TopLeft({card.NodeSize().width - size - 12.0F, 13.0F, size, size}, card.NodeSize().height),
                                L"", std::format("AddDifficulty.{}", visiblePosition));
    ApplySkinPart(button, visuals_, "AddDifficulty", PureWhite);
    auto &plus = CreateSprite(button, {(size - 16.0F) * 0.5F, (size - 16.0F) * 0.5F, 16.0F, 16.0F},
                              visuals_.RegisterImage(Common(L"Icons.plus")), "Icon");
    RequireComponent<SpriteVisualComponent>(plus).SetTint(theme::BrandText);
    button.SetZIndex(3);
    button.SetEnabled(!selection_.catalogLoading_);
    return button.Id();
}

void SongListView::RefreshPatternSelection()
{
    if (!canvas_)
        return;
    for (const auto [id, patternIndex] : difficultyButtonIds_)
        if (auto *button = canvas_->FindNode(id))
            StyleDifficulty(*button, visuals_, patternIndex == selection_.selectedPatternIndex_);
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
    return viewport_ ? std::max(viewport_->NodeSize().height - layout::ContentTop, layout::ContentTop + 1.0F)
                     : layout::BodyHeight - layout::ViewportTop - layout::ContentTop;
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

void SongListView::ScrollBy(const float pixels)
{
    scrollOffset_ += pixels;
    ApplyScrollOffset();
}

void SongListView::ApplyScrollOffset()
{
    const float maximum = std::max(contentHeight_ - VisibleBottom(), 0.0F);
    scrollOffset_ = std::clamp(scrollOffset_, 0.0F, maximum);
    if (content_ != nullptr)
        content_->SetPosition({0.0F, scrollOffset_});
    UpdateScrollbar();
    UpdateVisibleCards();
    if (router_ != nullptr)
        router_->InvalidateHitTest();
}

void SongListView::UpdateScrollbar()
{
    if (viewport_ == nullptr)
        return;
    const float visibleHeight = std::max(VisibleBottom() - layout::ContentTop, 1.0F);
    scrollbar_.SetRange(visibleHeight, std::max(contentHeight_ - layout::ContentTop, visibleHeight), scrollOffset_);
}

void SongListView::TrackScrollbar(const std::optional<Point> canvasPointer, const bool leftButtonDown)
{
    if (const auto offset = scrollbar_.Track(canvasPointer, leftButtonDown))
    {
        scrollOffset_ = *offset;
        ApplyScrollOffset();
    }
}
