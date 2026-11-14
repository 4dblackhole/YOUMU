#include "MusicSelectView.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"

using namespace PhantomCore;

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

    background_ = &CreatePanel(*board_, {0.0F, 0.0F, layout::CanvasSize.width, layout::CanvasSize.height}, "Background");
    ApplyFlatStyle(*background_, CanvasBlue);
    SetCornerRadius(*background_, 0.0F);
    BindSkinImage(*background_, screenVisuals_, "MusicSelect", "Background");
    artwork_.CreateBackground(*board_);
    CreateSheet();
    CreateLetterbox();
    if (!IsEditorSongSelect())
    {
        CreateRecordPanel();
    }
    CreateSongInformationPanel();
    CreateSongBrowser();
    favoriteTabs_.Initialize(*browserPanel_, screenVisuals_, texts_);
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
    if (!IsEditorSongSelect()) playOptionsPanel_.Initialize(*canvas_, texts_, services.windowControl);
    favoritesPanel_.Initialize(*canvas_, texts_);
    if (recordPanel_) records_.InitializeFilter(*canvas_, services.windowControl);
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
        songList_.Rebuild();
        UpdateSearchInputLayout();
    }
    UpdateRecords();
    options_.Update(deltaSeconds);
    playOptionsPanel_.Update(deltaSeconds);
    const bool controlDown =
        input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    const bool optionsShortcut = controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('O'));
    if (!IsSelectionModalVisible() && !options_.OwnsKeyboard() && optionsShortcut)
    {
        CloseAdditionMenu();
        options_.Toggle();
    }
    ProcessPointer(input);
    // Panels are processed in order; the first one that consumes input stops the rest.
    const bool panelConsumed =
        ProcessPlayOptionsPanel(input) || ProcessRecordFilterPanel(input) || ProcessFavoritesPanel(input);
    if (panelConsumed || ProcessFilterPanel(input))
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
        records_.Invalidate();
        songList_.Rebuild();
        songList_.EnsureFocusedVisible();
        inputRouter_.InvalidateHitTest();
    }
}

void MusicSelectView::Shutdown() noexcept
{
    playOptionsPanel_.Shutdown();
    records_.Shutdown();
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
    songList_.Shutdown();
    goErrorLabel_ = nullptr;
    goError_ = nullptr;
    goLabel_ = nullptr;
    goButton_ = nullptr;
    backLabel_ = nullptr;
    backButton_ = nullptr;
    optionButton_ = nullptr;
    playOptionsButton_ = nullptr;
    playOptionsButtonId_ = 0;
    modeSelector_ = nullptr;
    modeSelectorId_ = 0;
    statCaptions_ = {};
    statValues_ = {};
    creatorIcon_ = nullptr;
    levelBadge_ = nullptr;
    difficultyBlock_ = nullptr;
    selectedCreatorLabel_ = nullptr;
    selectedPatternLabel_ = nullptr;
    selectedArtistLabel_ = nullptr;
    selectedSongLabel_ = nullptr;
    sortSelector_ = nullptr;
    filterButton_ = nullptr;
    searchCountLabel_ = nullptr;
    searchField_ = nullptr;
    browserPanel_ = nullptr;
    artwork_.Reset();
    informationPanel_ = nullptr;
    recordPanel_ = nullptr;
    letterbox_ = {};
    footerBar_ = nullptr;
    columnRule_ = nullptr;
    sheetExtension_ = nullptr;
    sheet_ = nullptr;
    sheetEdge_ = nullptr;
    background_ = nullptr;
    board_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}

void MusicSelectView::RebuildVisibleSongs()
{
    selection_.RebuildVisibleSongs();
    songList_.ResetScroll();
    RefreshCatalogPresentation();
}

void MusicSelectView::RefreshCatalogPresentation()
{
    favoriteTabs_.Refresh(selection_.favorites_, selection_.favoriteFolder_);
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

bool MusicSelectView::ProcessPlayOptionsPanel(const InputState &input)
{
    if (!playOptionsPanel_.IsVisible()) return false;
    for (const auto &action : canvas_->TakeActions()) playOptionsPanel_.ProcessAction(action);
    playOptionsPanel_.ProcessInput(input, canvasPointer_);
    if (auto applied = playOptionsPanel_.TakeApplied()) selection_.playOptions_ = *applied;
    if (!playOptionsPanel_.IsVisible()) inputRouter_.Reset(*canvas_);
    else inputRouter_.InvalidateHitTest(); // Expansion/scrolling may have moved controls.
    return true; // Closing input and queued background actions never reach song selection.
}

void MusicSelectView::RefreshFilterButton()
{
    if (!filterButton_) return;
    // An active filter fills the button with cobalt so it reads as "on".
    const bool active = selection_.filter_.IsActive();
    VisualStyle style{};
    style.normal = active ? ui_colors::theme::Brand : Color{1.0F, 1.0F, 1.0F, 0.72F};
    style.hovered = active ? ui_colors::theme::BrandDeep : PureWhite;
    style.pressed = ui_colors::theme::BrandDeep;
    style.disabled = style.normal;
    RequireComponent<SpriteVisualComponent>(*filterButton_).SetStyle(style);
    RequireComponent<TextVisualComponent>(*filterButton_)
        .SetTextColor(active ? PureWhite : ui_colors::theme::BrandText);
}

bool MusicSelectView::IsEditorSongSelect() const noexcept
{
    return purpose_ == SongSelectPurpose::Editor;
}

void MusicSelectView::RefreshSelection()
{
    songList_.Rebuild();
    RefreshSelectionPresentation();
    songList_.EnsureFocusedVisible();
}

void MusicSelectView::CreateLetterbox()
{
    // Bands outside the scaled 16:9 arrangement. They sit above the board and below every popup.
    auto &frame = canvas_->CreateNode(Anchor::Center, "SongSelect.Letterbox");
    frame.SetZIndex(5);
    for (auto &band : letterbox_)
    {
        band = &CreatePanel(frame, {}, "SongSelect.LetterboxBand");
        auto &visual = RequireComponent<SpriteVisualComponent>(*band);
        visual.SetTint(ui_colors::theme::Letterbox);
        visual.SetCornerRadius(0.0F);
        band->SetVisible(false);
    }
}
