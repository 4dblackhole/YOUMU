#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

using namespace song_select;

namespace
{
    // Checks the nodes from left to right, as a chain of null checks would.
    template <typename... Nodes>
    [[nodiscard]] bool AllCreated(const Nodes *...nodes) noexcept
    {
        return ((nodes != nullptr) && ...);
    }
}

void MusicSelectView::UpdateResponsiveLayout()
{
    if (!AllCreated(canvas_, board_, background_, informationPanel_, browserPanel_, backButton_, goButton_))
    {
        return;
    }

    const Size canvasSize = canvas_->LogicalSize();
    const SongSelectFrame frame = CalculateFrame(canvasSize.width);
    const float height = layout::CanvasSize.height;
    // The board holds the 16:9-or-wider arrangement and is scaled as a whole on narrower screens.
    board_->SetSize({frame.width, height});
    board_->SetPosition({0.0F, 0.0F});
    board_->Transform().SetScale(frame.scale, frame.scale, 1.0F);
    background_->SetBounds({0.0F, 0.0F, frame.width, height});
    artwork_.LayoutBackground({frame.width, height});
    UpdateLetterbox(canvasSize, frame);

    UpdateSheetLayout(frame);
    if (recordPanel_ != nullptr)
        recordPanel_->SetBounds(TopLeft(frame.record, height));
    informationPanel_->SetBounds(TopLeft(frame.detail, height));
    browserPanel_->SetBounds(TopLeft(frame.list, height));

    UpdateRecordPanelLayout();
    UpdateSongInformationLayout();
    UpdateSongBrowserLayout();
    UpdateFooterLayout(frame);
    UpdateSearchInputLayout();
    filterPanel_.SetViewport(width_, height_);
    playOptionsPanel_.SetViewport(height_);
    records_.SetViewport(height_);
    favoritesPanel_.SetViewport();
    osuImportPanel_.SetViewport();
}

void MusicSelectView::UpdateSheetLayout(const SongSelectFrame &frame)
{
    if (!AllCreated(sheetEdge_, sheet_, sheetExtension_, columnRule_))
    {
        return;
    }
    const float height = layout::CanvasSize.height;
    sheetEdge_->SetBounds({layout::SheetEdgeLeft, 0.0F, layout::SheetEdgeWidth, height});
    sheet_->SetBounds({layout::SheetLeft, 0.0F, layout::SheetImageWidth, height});

    // Only a screen wider than 16:9 has room past the image.
    const float imageRight = layout::SheetLeft + layout::SheetImageWidth;
    const bool wider = frame.width > imageRight;
    sheetExtension_->SetVisible(wider);
    if (wider)
        sheetExtension_->SetBounds({imageRight, 0.0F, frame.width - imageRight, height});

    columnRule_->SetBounds(TopLeft({frame.list.x - 1.0F, frame.list.y + layout::ColumnRuleInset, 1.0F,
                                    frame.list.height - layout::ColumnRuleInset * 2.0F},
                                   height));
}

void MusicSelectView::UpdateLetterbox(const Size canvasSize, const SongSelectFrame &frame)
{
    const float contentWidth = frame.width * frame.scale, contentHeight = layout::CanvasSize.height * frame.scale;
    const float vertical = (canvasSize.height - contentHeight) * 0.5F;
    const float horizontal = (canvasSize.width - contentWidth) * 0.5F;
    const std::array<Rect, 4> bands{
        Rect{-canvasSize.width * 0.5F, contentHeight * 0.5F, canvasSize.width, vertical},
        Rect{-canvasSize.width * 0.5F, -canvasSize.height * 0.5F, canvasSize.width, vertical},
        Rect{-canvasSize.width * 0.5F, -contentHeight * 0.5F, horizontal, contentHeight},
        Rect{contentWidth * 0.5F, -contentHeight * 0.5F, horizontal, contentHeight}};
    for (std::size_t i = 0; i < bands.size(); ++i)
    {
        if (letterbox_[i] == nullptr)
            continue;
        const bool visible = bands[i].width > 0.01F && bands[i].height > 0.01F;
        letterbox_[i]->SetVisible(visible);
        if (visible)
            letterbox_[i]->SetBounds(bands[i]);
    }
}

void MusicSelectView::UpdateRecordPanelLayout()
{
    if (recordPanel_) records_.Layout();
}

void MusicSelectView::UpdateSongInformationLayout()
{
    if (!AllCreated(informationPanel_, selectedSongLabel_, selectedArtistLabel_, selectedCreatorLabel_,
                    difficultyBlock_, selectedPatternLabel_, levelBadge_, creatorIcon_, artistIcon_))
    {
        return;
    }

    // Jacket, names and figures share one vertical axis through the middle of the column.
    const Size panel = informationPanel_->NodeSize();
    const float center = panel.width * 0.5F;
    const float textWidth = std::max(panel.width - 32.0F, 1.0F);
    const auto row = [&](const float top, const float rowHeight) {
        return TopLeft({center - textWidth * 0.5F, top, textWidth, rowHeight}, panel.height);
    };
    artwork_.LayoutPreview(TopLeft(
        {center - layout::JacketSize * 0.5F, layout::JacketTop, layout::JacketSize, layout::JacketSize}, panel.height));
    selectedArtistLabel_->SetBounds(row(layout::ArtistTop, 22.0F));
    selectedSongLabel_->SetBounds(row(layout::TitleTop, 36.0F));
    selectedSongLabel_->SetClipRect({0.0F, 0.0F, textWidth, 36.0F});
    selectedCreatorLabel_->SetBounds(row(layout::CreatorTop, 20.0F));

    // The note and the quill sit right before the centred artist and creator names; each name line
    // takes the pointer over its text (and icon) for the all-names tooltip.
    const auto captionWidth = [&](Visual2DNode &label, const float size) {
        return std::min(EstimateCaptionWidth(RequireComponent<TextVisualComponent>(label).Text(), size), textWidth);
    };
    const float artistWidth = captionWidth(*selectedArtistLabel_, 15.0F);
    const float titleWidth = captionWidth(*selectedSongLabel_, 30.0F);
    const float creatorWidth = captionWidth(*selectedCreatorLabel_, 14.0F);
    constexpr float iconLead = layout::NameIconGap + layout::NameIconSize;
    const auto placeIcon = [&](Visual2DNode &icon, const float width, const float top) {
        icon.SetBounds(TopLeft({center - width * 0.5F - iconLead, top, layout::NameIconSize, layout::NameIconSize},
                               panel.height));
    };
    placeIcon(*artistIcon_, artistWidth, layout::ArtistTop + 3.0F);
    placeIcon(*creatorIcon_, creatorWidth, layout::CreatorTop + 2.0F);
    const auto hoverArea = [&](Visual2DNode &label, const float width, const float lead, const float height) {
        if (auto *collider = label.GetComponent<RectangleCollider2DComponent>())
            collider->SetLocalBounds({(textWidth - width) * 0.5F - lead, 0.0F, width + lead, height});
    };
    hoverArea(*selectedArtistLabel_, artistWidth, iconLead, 22.0F);
    hoverArea(*selectedSongLabel_, titleWidth, 0.0F, 36.0F);
    hoverArea(*selectedCreatorLabel_, creatorWidth, iconLead, 20.0F);

    // The block grows with the chart name up to DifficultyBlockMaxWidth and keeps the value badge at its right end;
    // a longer name is clipped at the label's edge.
    constexpr float badgeWidth = 40.0F, badgeHeight = 22.0F, nameLeft = 16.0F, badgeGap = 8.0F, blockRight = 14.0F;
    const auto &name = RequireComponent<TextVisualComponent>(*selectedPatternLabel_);
    const float nameWidth = EstimateCaptionWidth(name.Text(), 15.0F) + 4.0F;
    const float blockWidth =
        std::clamp(nameLeft + nameWidth + badgeGap + badgeWidth + blockRight, layout::DifficultyBlock.width,
                   std::min(layout::DifficultyBlockMaxWidth, textWidth));
    const float blockHeight = layout::DifficultyBlock.height;
    difficultyBlock_->SetBounds(
        TopLeft({center - blockWidth * 0.5F, layout::DifficultyBlockTop, blockWidth, blockHeight}, panel.height));
    const float labelWidth = std::max(blockWidth - nameLeft - badgeGap - badgeWidth - blockRight, 1.0F);
    selectedPatternLabel_->SetBounds({nameLeft, 0.0F, labelWidth, blockHeight});
    selectedPatternLabel_->SetClipRect({0.0F, 0.0F, labelWidth, blockHeight});
    levelBadge_->SetBounds({blockWidth - blockRight - badgeWidth, (blockHeight - badgeHeight) * 0.5F, badgeWidth,
                            badgeHeight});
    levelBadge_->Children().front()->SetBounds({0.0F, 0.0F, badgeWidth, badgeHeight});

    const float gridLeft = center - layout::StatWidth * 1.5F;
    const float tableWidth = layout::StatWidth * 3.0F;
    const std::array<Rect, 5> rules{
        Rect{gridLeft, layout::StatTableTop, tableWidth, 1.0F},
        Rect{gridLeft, layout::StatTableTop + layout::StatTableHeight - 1.0F, tableWidth, 1.0F},
        Rect{gridLeft, layout::StatRowRule, tableWidth, 1.0F},
        Rect{gridLeft + layout::StatWidth, layout::StatTableTop, 1.0F, layout::StatTableHeight},
        Rect{gridLeft + layout::StatWidth * 2.0F, layout::StatTableTop, 1.0F, layout::StatTableHeight}};
    for (std::size_t i = 0; i < rules.size(); ++i)
        if (statRules_[i] != nullptr)
            statRules_[i]->SetBounds(TopLeft(rules[i], panel.height));
    for (std::size_t i = 0; i < statCaptions_.size(); ++i)
    {
        if (statCaptions_[i] == nullptr || statValues_[i] == nullptr)
            continue;
        const float x = gridLeft + layout::StatWidth * static_cast<float>(i % 3);
        const float top = layout::StatTop + layout::StatPitch * static_cast<float>(i / 3);
        statCaptions_[i]->SetBounds(TopLeft({x, top, layout::StatWidth, 18.0F}, panel.height));
        statValues_[i]->SetBounds(TopLeft({x, top + 18.0F, layout::StatWidth, 30.0F}, panel.height));
    }
}

void MusicSelectView::UpdateSongBrowserLayout()
{
    if (browserPanel_ == nullptr || searchField_ == nullptr || songList_.Viewport() == nullptr)
    {
        return;
    }
    const float height = browserPanel_->NodeSize().height;
    favoriteTabs_.SetBounds(TopLeft(layout::Tabs, height));
    songList_.Viewport()->SetBounds(
        TopLeft({0.0F, layout::ViewportTop, layout::ListWidth, layout::BodyHeight - layout::ViewportTop}, height));
    songList_.SetLayout();
}

void MusicSelectView::UpdateFooterLayout(const SongSelectFrame &frame)
{
    if (!AllCreated(footerBar_, optionButton_, backButton_, backLabel_, goButton_, goLabel_, goError_, goErrorLabel_))
    {
        return;
    }
    const float height = layout::CanvasSize.height;
    footerBar_->SetBounds(TopLeft(frame.footer, height));
    backButton_->SetBounds(TopLeft(frame.back, height));
    // The block's right edge is slanted, so the caption centres on the solid part.
    backLabel_->SetBounds({0.0F, 0.0F, frame.back.width - 16.0F, frame.back.height});
    goButton_->SetBounds(TopLeft(frame.go, height));
    goLabel_->SetBounds({24.0F, 0.0F, frame.go.width - 24.0F, frame.go.height});
    goError_->SetBounds(TopLeft({frame.width - layout::GoError.width - 16.0F, frame.go.y - layout::GoError.height - 10.0F,
                                 layout::GoError.width, layout::GoError.height},
                                height));
    goErrorLabel_->SetBounds({14.0F, 0.0F, layout::GoError.width - 24.0F, layout::GoError.height});

    // Mode, Play options and the gear sit side by side in the middle of the band.
    const float cellTop = frame.footer.y + (layout::FooterHeight - layout::Gear.height) * 0.5F;
    const float total = modeSelector_ ? layout::ModeChip.width + layout::PlayOptions.width + layout::Gear.width
                                      : layout::Gear.width;
    float x = (frame.width - total) * 0.5F;
    if (modeSelector_ != nullptr && playOptionsButton_ != nullptr)
    {
        modeSelector_->SetBounds(TopLeft({x, cellTop, layout::ModeChip.width, layout::ModeChip.height}, height));
        x += layout::ModeChip.width;
        playOptionsButton_->SetBounds(TopLeft({x, cellTop, layout::PlayOptions.width, layout::PlayOptions.height}, height));
        x += layout::PlayOptions.width;
        RefreshPlayOptionsBadge();
    }
    optionButton_->SetBounds(TopLeft({x, cellTop, layout::Gear.width, layout::Gear.height}, height));
}

void MusicSelectView::RefreshPlayOptionsBadge()
{
    if (!AllCreated(playOptionsButton_, playOptionsBadge_, playOptionsBadgeText_))
        return;
    const auto count = ChangedPlayOptionCount(selection_.playOptions_);
    playOptionsBadge_->SetVisible(count > 0);
    RequireComponent<TextVisualComponent>(*playOptionsBadgeText_).SetText(std::to_wstring(count));
    // The badge follows the centred caption, 8 to its right (popups.css .count-badge).
    const auto size = playOptionsButton_->NodeSize();
    const auto &caption = RequireComponent<TextVisualComponent>(*playOptionsButton_);
    const float captionRight = (size.width + EstimateCaptionWidth(caption.Text(), caption.FontSize())) * 0.5F;
    playOptionsBadge_->SetBounds({std::min(captionRight + 8.0F, size.width - 18.0F), (size.height - 18.0F) * 0.5F, 18.0F, 18.0F});
    playOptionsBadgeText_->SetBounds({0.0F, 0.0F, 18.0F, 18.0F});
}
