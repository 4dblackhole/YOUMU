#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace song_select;

void MusicSelectView::Initialize(const EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas(
        {layout::CanvasSize, CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the Lobby screen Canvas.");
    }

    board_ = &canvas_->CreateNode(Anchor::Center,
                                  IsEditorSongSelect() ? "YOUMU.EditorSongSelect.Board"
                                                       : "YOUMU.SongSelect.Board");
    board_->SetPivot({0.5F, 0.5F});
    board_->SetSize(layout::CanvasSize);
    board_->SetPosition({0.0F, 0.0F});

    background_ = &AddPanel(*board_, layout::DesignBoard, CanvasBlue, "Background");
    chartBackground_ = &CreateSprite(*board_, {}, {}, "SongBackground");
    chartBackground_->SetVisible(false);
    CreateCategoryBar();
    if (!IsEditorSongSelect())
    {
        CreateRecordPanel();
    }
    CreateSongInformationPanel();
    CreateSongBrowser();
    favoriteTabs_.Initialize(*browserPanel_, texts_);
    CreateFooter();
    CreateAdditionMenu();
    options_.Initialize(*canvas_, services.audio, services.windowControl);
    if (services.windowControl)
    {
        searchInput_ = services.windowControl->CreateTextInput();
        if (!searchInput_) throw std::runtime_error("Failed to create song search input.");
        searchInput_->SetText(selection_.searchText_);
    }
    filterPanel_.Initialize(*canvas_, texts_, services.windowControl);
    favoritesPanel_.Initialize(*canvas_, texts_);
    if (recordPanel_) recordFilterPanel_.Initialize(*canvas_, texts_, services.windowControl);
    ApplyTexts();
    UpdateResponsiveLayout();
    RebuildVisibleSongs();
    UpdateSearchInputVisibility();
}

SongSelectCommand MusicSelectView::Update(const InputState &input,
                                           const double deltaSeconds)
{
    command_ = SongSelectCommand::None;
    if (!canvas_)
        return command_;
    SynchronizeSearchText();
    if (textRevision_ != texts_.Revision())
    {
        ApplyTexts();
        RebuildSongCards();
        UpdateSearchInputLayout();
    }
    UpdateRecords();
    options_.Update(deltaSeconds);
    const bool controlDown =
        input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (!IsSelectionModalVisible() && !options_.OwnsKeyboard() && controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('O')))
    {
        CloseAdditionMenu();
        options_.Toggle();
    }
    ProcessPointer(input);
    if (ProcessRecordFilterPanel(input) || ProcessFavoritesPanel(input) || ProcessFilterPanel(input))
    {
        UpdateSearchInputVisibility();
        return command_;
    }
    if (ProcessAdditionMenu(input))
    {
        UpdateSearchInputVisibility();
        return command_;
    }
    if (options_.ProcessInput(input, canvasPointer_, inputRouter_.HoveredNode(), inputRouter_))
    {
        static_cast<void>(canvas_->TakeActions());
        UpdateSearchInputVisibility();
        return command_;
    }
    if (!ProcessActions())
        ProcessKeyboard(input);
    UpdateSearchInputVisibility();
    return command_;
}

void MusicSelectView::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        CloseAdditionMenu();
        UpdateResponsiveLayout();
        ++recordRevision_;
        RebuildSongCards();
        EnsureFocusedCardVisible();
        inputRouter_.InvalidateHitTest();
    }
}

void MusicSelectView::Shutdown() noexcept
{
    recordFilterPanel_.Shutdown();
    filterPanel_.Shutdown();
    favoritesPanel_.Shutdown();
    favoriteTabs_.Shutdown();
    searchInput_.reset();
    searchFocused_ = false;
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    options_.Shutdown();
    additionMenu_ = newDifficultyItem_ = convertOsuItem_ = nullptr;
    canvasPointer_.reset();
    difficultyButtonIds_.clear();
    songCards_.clear();
    songRows_.clear();
    goLabel_ = nullptr;
    goFill_ = nullptr;
    goButton_ = nullptr;
    backLabel_ = nullptr;
    backFill_ = nullptr;
    backButton_ = nullptr;
    optionLabel_ = nullptr;
    optionButton_ = nullptr;
    modeSelector_ = nullptr;
    modeSelectorId_ = 0;
    selectedDetailsLabel_ = nullptr;
    extraDetailsLabels_ = {};
    selectedCreatorLabel_ = nullptr;
    selectedPatternLabel_ = nullptr;
    selectedArtistLabel_ = nullptr;
    selectedSongLabel_ = nullptr;
    browserHint_ = nullptr;
    sortSelector_ = nullptr;
    filterButton_ = nullptr;
    searchCountLabel_ = nullptr;
    searchField_ = nullptr;
    scrollbarHandle_ = nullptr;
    scrollbarTrack_ = nullptr;
    songContent_ = nullptr;
    songViewport_ = nullptr;
    browserPanel_ = nullptr;
    informationDivider_ = nullptr;
    creatorHeading_ = nullptr;
    difficultyHeading_ = nullptr;
    difficultyInformation_ = nullptr;
    previewPath_.clear();
    previewImageSize_ = {};
    previewImage_ = nullptr;
    previewEmpty_ = nullptr;
    previewTitle_ = nullptr;
    preview_ = nullptr;
    informationPanel_ = nullptr;
    emptyRecordMessage_ = nullptr;
    recordSelector_ = nullptr;
    recordPanel_ = nullptr;
    recordRows_ = recordQueryButton_ = recordPreviousButton_ = recordNextButton_ = nullptr;
    ++recordRevision_;
    categoryBar_ = nullptr;
    categoryLabel_ = nullptr;
    background_ = nullptr;
    chartBackground_ = nullptr;
    chartBackgroundOffset_ = {};
    board_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}

void MusicSelectView::RebuildVisibleSongs()
{
    selection_.RebuildVisibleSongs();
    favoriteTabs_.Refresh(selection_.favorites_, selection_.favoriteFolder_);
    scrollOffset_ = 0.0F;
    RefreshSelection();
    RefreshSearchPresentation();
    RefreshFilterButton();
}

bool MusicSelectView::ProcessFilterPanel(const InputState &input)
{
    if (!filterPanel_.IsVisible()) return false;
    for (const auto &action : canvas_->TakeActions()) filterPanel_.ProcessAction(action);
    filterPanel_.ProcessInput(input);
    if (auto applied = filterPanel_.TakeApplied())
    {
        selection_.filter_ = std::move(*applied);
        RebuildVisibleSongs();
    }
    if (!filterPanel_.IsVisible()) inputRouter_.Reset(*canvas_);
    return true;
}

void MusicSelectView::RefreshFilterButton()
{
    if (!filterButton_) return;
    ApplyButtonStyle(*filterButton_, selection_.filter_.IsActive() ? AccentBlue : PureWhite,
                     PaleBlue, selection_.filter_.IsActive() ? PureWhite : DeepBlue);
}

bool MusicSelectView::IsEditorSongSelect() const noexcept
{
    return purpose_ == SongSelectPurpose::Editor;
}

void MusicSelectView::RefreshSelection()
{
    RebuildSongCards();
    RefreshSelectionPresentation();
    EnsureFocusedCardVisible();
}
