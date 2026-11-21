#pragma once
#include "PhantomCore.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/UiAlert.h"
#include "Presentation/UiDropdown.h"
#include "Presentation/UiTooltip.h"
#include "Presentation/PlayOptionsPanel.h"
#include "SongSelectionState.h"
#include "SongFilterPanel.h"
#include "SongRecordView.h"
#include "SongFavoriteTabs.h"
#include "SongFavoritesPanel.h"
#include "OsuImportPanel.h"
#include "SongSelectPurpose.h"
#include "SongBackgroundPreview.h"
#include "SongListView.h"
#include "SongSelectLayout.h"
#include "Texts/TextCatalog.h"
#include <filesystem>
#include <array>
#include <vector>

enum class SongSelectCommand
{
    None,
    Back,
    Launch,
    CreateDifficulty,
    ConvertOsu,
    OpenRecord // A record row was clicked; TakeOpenedRecord gives it.
};
class MusicSelectView final
{
  public:
    MusicSelectView(PhantomCore::ScreenVisual2DManager &visuals, SongSelectionState &state,
                    SongSelectPurpose purpose, TextCatalog &texts,
                    OptionSettings &options, std::filesystem::path recordDatabase = {})
        : selection_(state), purpose_(purpose), screenVisuals_(visuals), texts_(texts),
          settings_(options), options_(texts, options, &visuals), records_(texts, visuals, std::move(recordDatabase)),
          artwork_(visuals), songList_(visuals, state, texts, purpose == SongSelectPurpose::Editor)
    {
    }
    void SetVisible(bool visible)
    {
        visible_ = visible;
        if (!visible) { filterPanel_.Close(); favoritesPanel_.Close(); osuImportPanel_.Close(); records_.CloseFilter(); playOptionsPanel_.Dismiss(); tooltip_.Hide(); alert_.Hide(); if (canvas_) CollapseDropdowns(*canvas_); }
        else records_.Invalidate();
        static_cast<void>(canvasHandle_.SetVisible(visible));
        UpdateSearchInputVisibility();
    }
    void Initialize(const PhantomCore::EngineServices &services);
    SongSelectCommand Update(const PhantomCore::InputState &input, double deltaSeconds);
    void OnResize(const std::uint32_t width, const std::uint32_t height);
    void Shutdown() noexcept;
    void RebuildVisibleSongs();
    void RefreshCatalogPresentation();
    void RefreshSelectionPresentation();
    // Opens the shared alert above song select. Panels with text boxes close first, and until the
    // alert is answered no key or pointer reaches song select.
    void ShowAlert(AlertContent content);
    // Editor song select: lists the difficulties of a scanned osu! song folder. The answer
    // (Import with the checked ones, or Cancel) is taken once with TakeOsuImportChoice.
    void OpenOsuImportPanel(std::wstring folder, std::vector<OsuImportDifficulty> difficulties);
    [[nodiscard]] std::optional<OsuImportChoice> TakeOsuImportChoice() { return osuImportPanel_.TakeChoice(); }
    // The record whose row was clicked (SongSelectCommand::OpenRecord), taken once.
    [[nodiscard]] std::optional<PlayRecord> TakeOpenedRecord() noexcept { return records_.TakeOpenedRecord(); }
    // The scene reports whether the focused song is the one being heard.
    void SetNowPlaying(const bool playing) { songList_.SetNowPlaying(playing); }

  private:
    using SortMode = SongSelectionState::SortMode;

    void CreateSheet();
    void CreateLetterbox();
    void CreateRecordPanel();
    void UpdateRecords();
    void OpenRecordFilter();
    bool ProcessRecordFilterPanel(const PhantomCore::InputState &input);
    bool ProcessPlayOptionsPanel(const PhantomCore::InputState &input);
    void CreateSongInformationPanel();
    void CreateSongBrowser();
    void CreateFooter();
    // Editor song select: Import from osu! at the top of the left column (where game song select has records).
    void CreateImportButton();
    void ApplyTexts();
    void ApplyTooltips();
    void RefreshPlayOptionsBadge();
    void UpdateResponsiveLayout();
    void UpdateRecordPanelLayout();
    void UpdateSongInformationLayout();
    void UpdateSongBrowserLayout();
    void UpdateFooterLayout(const song_select::SongSelectFrame &frame);
    void UpdateLetterbox(PhantomCore::Size canvasSize, const song_select::SongSelectFrame &frame);
    void UpdateSheetLayout(const song_select::SongSelectFrame &frame);
    void RefreshSearchPresentation();
    [[nodiscard]] PhantomCore::PointerInput CanvasPointerInput(const PhantomCore::InputState &input);
    void ProcessPointer(const PhantomCore::InputState &input);
    bool ProcessAlert(const PhantomCore::InputState &input);
    bool ProcessActions();
    bool ProcessFilterPanel(const PhantomCore::InputState &input);
    bool ProcessFavoritesPanel(const PhantomCore::InputState &input);
    void OpenFavoritesPanel();
    bool ProcessOsuImportPanel(const PhantomCore::InputState &input);
    [[nodiscard]] bool IsSelectionModalVisible() const noexcept
    {
        const bool listPanel = favoritesPanel_.IsVisible() || osuImportPanel_.IsVisible();
        const bool filterOrRecords = filterPanel_.IsVisible() || records_.FilterVisible();
        return listPanel || filterOrRecords || playOptionsPanel_.IsVisible();
    }
    void RefreshFilterButton();
    bool ProcessKeyboard(const PhantomCore::InputState &input);
    // The footer button and F1 open Play options with the current selection's options.
    void OpenPlayOptions();
    bool ProcessSearchKeyboard(const PhantomCore::InputState &input);
    void SetSearchFocus(bool focused, bool selectAll = false);
    void SynchronizeSearchText();
    void UpdateSearchInputLayout();
    void UpdateSearchInputVisibility();
    void MoveSongFocus(const int delta);
    void MoveDifficultyFocus(const int delta);
    void SelectVisibleSong(const std::size_t visiblePosition);
    void SelectPattern(const std::size_t index);
    bool IsEditorSongSelect() const noexcept;
    void RefreshSelection();
    SongSelectionState &selection_;
    SongSelectPurpose purpose_;
    SongSelectCommand command_{};
    PhantomCore::ScreenVisual2DManager &screenVisuals_;
    TextCatalog &texts_;
    OptionSettings &settings_;
    OptionsPanel options_;
    PlayOptionsPanel playOptionsPanel_;
    SongFilterPanel filterPanel_;
    SongRecordView records_;
    SongFavoriteTabs favoriteTabs_;
    SongFavoritesPanel favoritesPanel_;
    OsuImportPanel osuImportPanel_;
    UiTooltip tooltip_;
    UiAlert alert_;
    SongBackgroundPreview artwork_;
    SongListView songList_;
    std::uint64_t textRevision_{};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    bool searchFocused_{};
    bool visible_{true};
    std::unique_ptr<PhantomCore::ITextInput> searchInput_;
    PhantomCore::Visual2DNode *importButton_{}, *importLabel_{}, *importHelp_{};
    PhantomCore::ScreenCanvasHandle canvasHandle_;
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DInputRouter inputRouter_;
    std::optional<PhantomCore::Point> canvasPointer_;
    PhantomCore::Visual2DNode *board_{};
    PhantomCore::Visual2DNode *background_{};
    PhantomCore::Visual2DNode *sheetEdge_{};
    PhantomCore::Visual2DNode *sheet_{};
    PhantomCore::Visual2DNode *sheetExtension_{};
    PhantomCore::Visual2DNode *columnRule_{};
    PhantomCore::Visual2DNode *footerBar_{};
    std::array<PhantomCore::Visual2DNode *, 4> letterbox_{};
    PhantomCore::Visual2DNode *recordPanel_{};
    PhantomCore::Visual2DNode *informationPanel_{};
    PhantomCore::Visual2DNode *difficultyBlock_{};
    PhantomCore::Visual2DNode *levelBadge_{};
    PhantomCore::Visual2DNode *creatorIcon_{}, *artistIcon_{};
    // Table rules: top, bottom, between the rows, then between the columns.
    std::array<PhantomCore::Visual2DNode *, 5> statRules_{};
    std::array<PhantomCore::Visual2DNode *, 6> statCaptions_{};
    std::array<PhantomCore::Visual2DNode *, 6> statValues_{};
    PhantomCore::Visual2DNode *browserPanel_{};
    PhantomCore::Visual2DNode *searchField_{};
    PhantomCore::Visual2DNode *searchCountLabel_{};
    PhantomCore::Visual2DNode *sortSelector_{};
    PhantomCore::Visual2DNode *filterButton_{};
    PhantomCore::Visual2DNode *selectedSongLabel_{};
    PhantomCore::Visual2DNode *selectedArtistLabel_{};
    PhantomCore::Visual2DNode *selectedPatternLabel_{};
    PhantomCore::Visual2DNode *selectedCreatorLabel_{};
    PhantomCore::Visual2DNode *modeSelector_{};
    PhantomCore::NodeId modeSelectorId_{};
    PhantomCore::Visual2DNode *playOptionsButton_{};
    PhantomCore::NodeId playOptionsButtonId_{};
    PhantomCore::Visual2DNode *playOptionsBadge_{}, *playOptionsBadgeText_{};
    PhantomCore::Visual2DNode *optionButton_{};
    PhantomCore::Visual2DNode *backButton_{};
    PhantomCore::Visual2DNode *backLabel_{};
    PhantomCore::Visual2DNode *goButton_{};
    PhantomCore::Visual2DNode *goLabel_{};
    PhantomCore::Visual2DNode *goError_{};
    PhantomCore::Visual2DNode *goErrorLabel_{};
    PhantomCore::NodeId sortSelectorId_{};
    PhantomCore::NodeId searchFieldId_{};
    PhantomCore::NodeId backButtonId_{};
    PhantomCore::NodeId optionButtonId_{};
    PhantomCore::NodeId goButtonId_{};
};
