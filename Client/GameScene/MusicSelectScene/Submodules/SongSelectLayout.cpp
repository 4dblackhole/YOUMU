#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

using namespace song_select;

void MusicSelectView::UpdateResponsiveLayout()
{
    if (canvas_ == nullptr || board_ == nullptr || background_ == nullptr ||
        categoryBar_ == nullptr || (!IsEditorSongSelect() && recordPanel_ == nullptr) ||
        informationPanel_ == nullptr || browserPanel_ == nullptr || optionButton_ == nullptr ||
        backButton_ == nullptr || goButton_ == nullptr)
    {
        return;
    }

    const Size canvasSize = canvas_->LogicalSize();
    const ResponsiveSongSelectLayout responsive =
        CalculateResponsiveLayout(canvasSize.width, IsEditorSongSelect());
    board_->SetSize(canvasSize);
    board_->SetPosition({0.0F, 0.0F});
    background_->SetBounds(responsive.background);
    artwork_.LayoutBackground(canvasSize);

    ResizeBorderedPanel(*categoryBar_, responsive.category, 3.0F);
    if (recordPanel_ != nullptr)
    {
        ResizeBorderedPanel(*recordPanel_, responsive.record, 3.0F);
    }
    ResizeBorderedPanel(*informationPanel_, responsive.information, 3.0F);
    ResizeBorderedPanel(*browserPanel_, responsive.browser, 3.0F);
    categoryBar_->SetClipRect(
        {0.0F, 0.0F, categoryBar_->NodeSize().width, categoryBar_->NodeSize().height});
    if (recordPanel_ != nullptr)
    {
        recordPanel_->SetClipRect(
            {0.0F, 0.0F, recordPanel_->NodeSize().width, recordPanel_->NodeSize().height});
    }
    informationPanel_->SetClipRect(
        {0.0F, 0.0F, informationPanel_->NodeSize().width, informationPanel_->NodeSize().height});
    browserPanel_->SetClipRect(
        {0.0F, 0.0F, browserPanel_->NodeSize().width, browserPanel_->NodeSize().height});

    UpdateRecordPanelLayout();
    UpdateSongInformationLayout();
    UpdateSongBrowserLayout();

    if (IsEditorSongSelect()) ResizeBorderedPanel(*optionButton_, responsive.option, 3.0F);
    else
    {
        const float gap = 8.0F;
        const float half = std::max((responsive.option.width - gap) / 2, 1.0F);
        if (modeSelector_) modeSelector_->SetBounds({responsive.option.x, responsive.option.y, half, responsive.option.height});
        ResizeBorderedPanel(*optionButton_, {responsive.option.x + half + gap, responsive.option.y, half, responsive.option.height}, 3.0F);
    }
    backButton_->SetBounds(responsive.back);
    goButton_->SetBounds(responsive.go);
    UpdateFooterLayout();
    UpdateSearchInputLayout();
    filterPanel_.SetViewport(width_, height_);
    playOptionsPanel_.SetViewport(height_);
    recordFilterPanel_.SetViewport(height_);
    favoritesPanel_.SetViewport();
}

void MusicSelectView::UpdateRecordPanelLayout()
{
    if (recordPanel_ == nullptr || recordSelector_ == nullptr || emptyRecordMessage_ == nullptr)
    {
        return;
    }
    LayoutRecordControls();
    const float panelWidth = recordPanel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(panelWidth - 44.0F, 1.0F);
    recordSelector_->SetBounds(
        ScaleTopLeftBounds({22.0F, 22.0F, innerWidth, layout::RecordSelector.height},
                           recordPanel_->NodeSize().height));
    emptyRecordMessage_->SetBounds(
        ScaleTopLeftBounds({22.0F, 400.0F, innerWidth, layout::EmptyRecordMessage.height},
                           recordPanel_->NodeSize().height));
}

void MusicSelectView::UpdateSongInformationLayout()
{
    if (informationPanel_ == nullptr || selectedSongLabel_ == nullptr ||
        selectedArtistLabel_ == nullptr || difficultyInformation_ == nullptr ||
        difficultyHeading_ == nullptr || creatorHeading_ == nullptr ||
        selectedPatternLabel_ == nullptr || selectedCreatorLabel_ == nullptr ||
        informationDivider_ == nullptr || selectedDetailsLabel_ == nullptr)
    {
        return;
    }

    const float panelWidth = informationPanel_->NodeSize().width / DesignToCanvasScale;
    const float previewWidth = artwork_.LayoutPreview(panelWidth, informationPanel_->NodeSize().height);
    const float detailsX = previewWidth > 0.0F ? 24.0F + (previewWidth + 20.0F) : 24.0F;

    const float detailsWidth = std::max(panelWidth - detailsX - 24.0F, 1.0F);
    selectedSongLabel_->SetBounds(ScaleTopLeftBounds(
        {detailsX, layout::SelectedSong.y, detailsWidth, layout::SelectedSong.height},
        informationPanel_->NodeSize().height));
    selectedArtistLabel_->SetBounds(ScaleTopLeftBounds(
        {detailsX, layout::SelectedArtist.y, detailsWidth, layout::SelectedArtist.height},
        informationPanel_->NodeSize().height));
    ResizeBorderedPanel(*difficultyInformation_,
                        ScaleTopLeftBounds({detailsX, layout::DifficultyInformation.y, detailsWidth,
                                            layout::DifficultyInformation.height},
                                           informationPanel_->NodeSize().height),
                        1.0F);

    const float headingGap = std::clamp((detailsWidth - 32.0F) * 0.03F, 2.0F, 8.71F);
    const float headingWidth = std::max((detailsWidth - 32.0F - headingGap) * 0.5F, 1.0F);
    const float creatorX = 16.0F + headingWidth + headingGap;
    difficultyHeading_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::DifficultyHeading.y, headingWidth, layout::DifficultyHeading.height},
        difficultyInformation_->NodeSize().height));
    creatorHeading_->SetBounds(ScaleTopLeftBounds(
        {creatorX, layout::CreatorHeading.y, headingWidth, layout::CreatorHeading.height},
        difficultyInformation_->NodeSize().height));
    selectedPatternLabel_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::SelectedDifficulty.y, headingWidth, layout::SelectedDifficulty.height},
        difficultyInformation_->NodeSize().height));
    selectedCreatorLabel_->SetBounds(ScaleTopLeftBounds(
        {creatorX, layout::SelectedCreator.y, headingWidth, layout::SelectedCreator.height},
        difficultyInformation_->NodeSize().height));
    const float informationInnerWidth = std::max(detailsWidth - 32.0F, 1.0F);
    informationDivider_->SetBounds(
        ScaleTopLeftBounds({16.0F, layout::InformationDivider.y, informationInnerWidth,
                            layout::InformationDivider.height},
                           difficultyInformation_->NodeSize().height));
    selectedDetailsLabel_->SetBounds(ScaleTopLeftBounds(
        {16.0F, layout::SelectedDetails.y, informationInnerWidth, layout::SelectedDetails.height},
        difficultyInformation_->NodeSize().height));
    for (std::size_t i = 0; i < extraDetailsLabels_.size(); ++i)
        extraDetailsLabels_[i]->SetBounds(ScaleTopLeftBounds(
            {16.0F, layout::ExtraDetails.y + 24.0F * static_cast<float>(i), informationInnerWidth, 22.0F},
            difficultyInformation_->NodeSize().height));
}

void MusicSelectView::UpdateSongBrowserLayout()
{
    if (browserPanel_ == nullptr || searchField_ == nullptr || searchCountLabel_ == nullptr ||
        sortSelector_ == nullptr || filterButton_ == nullptr || songViewport_ == nullptr || songContent_ == nullptr ||
        scrollbarTrack_ == nullptr || scrollbarHandle_ == nullptr || browserHint_ == nullptr)
    {
        return;
    }

    const float panelWidth = browserPanel_->NodeSize().width / DesignToCanvasScale;
    const float innerWidth = std::max(panelWidth - 28.0F, 1.0F);
    constexpr float tabsHeight = 64;
    favoriteTabs_.SetBounds(ScaleTopLeftBounds({14, 14, innerWidth, 54}, browserPanel_->NodeSize().height));
    searchField_->SetBounds(
        ScaleTopLeftBounds({14.0F, layout::SearchField.y + tabsHeight, innerWidth, layout::SearchField.height},
                           browserPanel_->NodeSize().height));
    auto &searchText = RequireComponent<TextVisualComponent>(*searchField_);
    const float countWidth = std::min(96.0F, innerWidth);
    const float searchTextWidth = std::max(innerWidth - countWidth - 18.0F, 1.0F);
    searchText.SetContentBounds(
        ScaleTopLeftBounds({14.0F, 0.0F, searchTextWidth, layout::SearchField.height},
                           searchField_->NodeSize().height));
    searchCountLabel_->SetBounds(
        ScaleTopLeftBounds({std::max(innerWidth - countWidth - 17.0F, 0.0F), 0.0F, countWidth,
                            layout::SearchCount.height},
                           searchField_->NodeSize().height));

    const float filterWidth = std::min(90.0F, innerWidth * .3F);
    const float sortWidth = std::max(innerWidth - filterWidth - 10.0F, 1.0F);
    sortSelector_->SetBounds(
        ScaleTopLeftBounds({14.0F, layout::SortSelector.y + tabsHeight, sortWidth, layout::SortSelector.height},
                           browserPanel_->NodeSize().height));
    filterButton_->SetBounds(
        ScaleTopLeftBounds({14.0F + sortWidth + 10.0F, layout::SortSelector.y + tabsHeight, filterWidth, layout::SortSelector.height},
                           browserPanel_->NodeSize().height));
    ResizeBorderedPanel(
        *songViewport_,
        ScaleTopLeftBounds({14.0F, layout::SongViewport.y + tabsHeight, innerWidth, layout::SongViewport.height - tabsHeight},
                           browserPanel_->NodeSize().height),
        1.5F);
    songViewport_->SetClipRect(
        {0.0F, 0.0F, songViewport_->NodeSize().width, songViewport_->NodeSize().height});
    songContent_->SetSize(songViewport_->NodeSize());
    songContentWidth_ = std::max(innerWidth - 24.0F, 1.0F);

    const float trackX = std::max(innerWidth - 9.0F, 0.0F);
    scrollbarTrack_->SetBounds(
        ScaleTopLeftBounds({trackX, layout::ScrollbarTrack.y, layout::ScrollbarTrack.width,
                            layout::ScrollbarTrack.height - tabsHeight},
                           songViewport_->NodeSize().height));
    browserHint_->SetBounds(
        ScaleTopLeftBounds({18.0F, layout::BrowserHint.y, std::max(panelWidth - 36.0F, 1.0F),
                            layout::BrowserHint.height},
                           browserPanel_->NodeSize().height));
    UpdateScrollbar();
}

void MusicSelectView::UpdateFooterLayout()
{
    if (optionButton_ == nullptr || optionLabel_ == nullptr || backButton_ == nullptr ||
        backFill_ == nullptr || backLabel_ == nullptr || goButton_ == nullptr ||
        goFill_ == nullptr || goLabel_ == nullptr)
    {
        return;
    }
    optionLabel_->SetBounds(
        {0.0F, 0.0F, optionButton_->NodeSize().width, optionButton_->NodeSize().height});

    const auto resizeEdgeButton = [](Visual2DNode &button,
                                     Visual2DNode &fill,
                                     Visual2DNode &label) {
        const Size buttonSize = button.NodeSize();
        const float padding = std::min(
            {4.0F * DesignToCanvasScale, buttonSize.width * 0.1F, buttonSize.height * 0.1F});
        fill.SetBounds({padding, padding, std::max(buttonSize.width - padding * 2.0F, 1.0F),
                        std::max(buttonSize.height - padding * 2.0F, 1.0F)});
        label.SetBounds({0.0F, 0.0F, buttonSize.width, buttonSize.height});
    };
    resizeEdgeButton(*backButton_, *backFill_, *backLabel_);
    resizeEdgeButton(*goButton_, *goFill_, *goLabel_);
}

void MusicSelectView::ResizeBorderedPanel(Visual2DNode &panel,
                                          const Rect bounds,
                                          const float penpotBorderWidth)
{
    panel.SetBounds(bounds);
    const float borderWidth = penpotBorderWidth * DesignToCanvasScale;
    for (const auto &child : panel.Children())
    {
        if (child->Name() == "Surface")
        {
            child->SetBounds({borderWidth, borderWidth,
                              std::max(panel.NodeSize().width - borderWidth * 2.0F, 1.0F),
                              std::max(panel.NodeSize().height - borderWidth * 2.0F, 1.0F)});
            break;
        }
    }
}
