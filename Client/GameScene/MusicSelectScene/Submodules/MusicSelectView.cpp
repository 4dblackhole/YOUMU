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
    CreateImportButton();
    options_.Initialize(*canvas_, services.audio, services.windowControl);
    if (services.windowControl)
    {
        searchInput_ = services.windowControl->CreateTextInput();
        if (!searchInput_) throw std::runtime_error("Failed to create song search input.");
        searchInput_->SetText(selection_.searchText_);
    }
    filterPanel_.Initialize(*canvas_, texts_, services.windowControl, &screenVisuals_);
    if (!IsEditorSongSelect())
        playOptionsPanel_.Initialize(*canvas_, texts_, services.windowControl, &screenVisuals_);
    favoritesPanel_.Initialize(*canvas_, texts_);
    if (IsEditorSongSelect())
        osuImportPanel_.Initialize(*canvas_, texts_);
    if (recordPanel_) records_.InitializeFilter(*canvas_, services.windowControl);
    tooltip_.Initialize(*canvas_);
    songMenu_.Initialize(*canvas_, texts_);
    alert_.Initialize(*canvas_, &screenVisuals_);
    ApplyTexts();
    UpdateResponsiveLayout();
    selection_.notation_ = settings_.Values().notation;
    RebuildVisibleSongs();
    UpdateSearchInputVisibility();
}

SongSelectCommand MusicSelectView::Update(const InputState &input,
                                           const double deltaSeconds)
{
    command_ = SongSelectCommand::None;
    if (!canvas_)
        return command_;
    if (ProcessAlert(input))
    {
        UpdateSearchInputVisibility();
        return command_;
    }
    if (ProcessDropdownKeys(*canvas_, input))
    {
        tooltip_.Hide();
        return command_;
    }
    SynchronizeSearchText();
    // The notation option changes the shown texts and the name/artist order.
    if (selection_.notation_ != settings_.Values().notation)
    {
        selection_.notation_ = settings_.Values().notation;
        RebuildVisibleSongs();
        songList_.EnsureFocusedVisible();
    }
    if (textRevision_ != texts_.Revision())
    {
        ApplyTexts();
        songList_.Rebuild();
        UpdateSearchInputLayout();
    }
    UpdateRecords();
    options_.Update(deltaSeconds);
    const bool controlDown =
        input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    const bool optionsShortcut = controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('O'));
    if (!IsSelectionModalVisible() && !options_.OwnsKeyboard() && optionsShortcut)
        options_.Toggle();
    // F1 opens Play options straight away (game song select only). The opening frame ends here so the
    // panel does not also read this F1 as its close key.
    const bool playOptionsShortcut = playOptionsButtonId_ != 0 && input.WasKeyPressed(VK_F1);
    if (playOptionsShortcut && !IsSelectionModalVisible() && !options_.IsVisible())
    {
        OpenPlayOptions();
        UpdateSearchInputVisibility();
        return command_;
    }
    ProcessPointer(input);
    // Hints rest while a panel, the options drawer or the right-click menu covers the screen.
    const bool covered = IsSelectionModalVisible() || options_.IsVisible() || songMenu_.IsOpen();
    songList_.RefreshNameHints();
    tooltip_.Update(deltaSeconds, input, inputRouter_.HoveredNode(), !covered, canvasPointer_);
    // Panels are processed in order; the first one that consumes input stops the rest.
    if (ProcessOsuImportPanel(input))
    {
        UpdateSearchInputVisibility();
        return command_;
    }
    const bool panelConsumed =
        ProcessPlayOptionsPanel(input) || ProcessRecordFilterPanel(input) || ProcessFavoritesPanel(input);
    if (panelConsumed || ProcessFilterPanel(input))
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
    if (ProcessSongMenu(input))
    {
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
        UpdateResponsiveLayout();
        records_.Invalidate();
        songList_.Rebuild();
        songList_.EnsureFocusedVisible();
        inputRouter_.InvalidateHitTest();
    }
}

void MusicSelectView::Shutdown() noexcept
{
    alert_.Shutdown();
    songMenu_.Shutdown();
    tooltip_.Shutdown();
    playOptionsPanel_.Shutdown();
    records_.Shutdown();
    filterPanel_.Shutdown();
    favoritesPanel_.Shutdown();
    osuImportPanel_.Shutdown();
    favoriteTabs_.Shutdown();
    searchInput_.reset();
    searchFocused_ = false;
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }
    options_.Shutdown();
    importButton_ = importLabel_ = importHelp_ = nullptr;
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

void MusicSelectView::ShowAlert(AlertContent content)
{
    if (canvas_ == nullptr)
        throw std::logic_error("Song select must be initialized before it shows an alert.");
    // Native text boxes would float above the alert, so their panels close without applying. A delete
    // waiting for its answer is dropped: only the confirmation that asked about it may approve it.
    pendingDeletion_.reset();
    songMenu_.Close();
    filterPanel_.Close();
    favoritesPanel_.Close();
    osuImportPanel_.Close();
    records_.CloseFilter();
    playOptionsPanel_.Dismiss();
    tooltip_.Hide();
    CollapseDropdowns(*canvas_);
    inputRouter_.Reset(*canvas_);
    alert_.Show(std::move(content));
    UpdateSearchInputVisibility();
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
    playOptionsPanel_.ProcessInput(input);
    if (auto applied = playOptionsPanel_.TakeApplied())
    {
        selection_.playOptions_ = *applied;
        RefreshPlayOptionsBadge();
    }
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
