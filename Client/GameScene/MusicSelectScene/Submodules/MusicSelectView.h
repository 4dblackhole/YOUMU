#pragma once
#include "PhantomCore.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/PlayOptionsPanel.h"
#include "SongSelectionState.h"
#include "SongFilterPanel.h"
#include "SongRecordView.h"
#include "SongFavoriteTabs.h"
#include "SongFavoritesPanel.h"
#include "SongSelectPurpose.h"
#include "SongBackgroundPreview.h"
#include "SongListView.h"
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
    ConvertOsu
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
        if (!visible) { CloseAdditionMenu(); filterPanel_.Close(); favoritesPanel_.Close(); records_.CloseFilter(); playOptionsPanel_.Dismiss(); }
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

  private:
    using SortMode = SongSelectionState::SortMode;

    void CreateCategoryBar();
    void CreateRecordPanel();
    void UpdateRecords();
    void OpenRecordFilter();
    bool ProcessRecordFilterPanel(const PhantomCore::InputState &input);
    bool ProcessPlayOptionsPanel(const PhantomCore::InputState &input);
    void CreateSongInformationPanel();
    void CreateSongBrowser();
    void CreateFooter();
    void CreateAdditionMenu();
    void OpenAdditionMenu(PhantomCore::Rect anchor);
    void CloseAdditionMenu();
    bool ProcessAdditionMenu(const PhantomCore::InputState &input);
    void ApplyTexts();
    void UpdateResponsiveLayout();
    void UpdateRecordPanelLayout();
    void UpdateSongInformationLayout();
    void UpdateSongBrowserLayout();
    void UpdateFooterLayout();
    void ResizeBorderedPanel(PhantomCore::Visual2DNode &panel, const PhantomCore::Rect bounds,
                             const float penpotBorderWidth);
    void RefreshSearchPresentation();
    void ProcessPointer(const PhantomCore::InputState &input);
    bool ProcessActions();
    bool ProcessFilterPanel(const PhantomCore::InputState &input);
    bool ProcessFavoritesPanel(const PhantomCore::InputState &input);
    void OpenFavoritesPanel();
    [[nodiscard]] bool IsSelectionModalVisible() const noexcept
    { return filterPanel_.IsVisible() || favoritesPanel_.IsVisible() || records_.FilterVisible() || playOptionsPanel_.IsVisible(); }
    void RefreshFilterButton();
    bool ProcessKeyboard(const PhantomCore::InputState &input);
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
    PhantomCore::Visual2DNode &AddPanel(PhantomCore::Visual2DNode &parent,
                                          const PhantomCore::Rect penpotBounds,
                                          const PhantomCore::Color color, std::string name,
                                          const float penpotCornerRadius = 0.0F);
    PhantomCore::Visual2DNode &AddBorderedPanel(PhantomCore::Visual2DNode &parent,
                                                  const PhantomCore::Rect penpotBounds,
                                                  const PhantomCore::Color fill,
                                                  const PhantomCore::Color border,
                                                  const float penpotBorderWidth,
                                                  const float penpotCornerRadius, std::string name);
    PhantomCore::Visual2DNode &AddLabel(
        PhantomCore::Visual2DNode &parent, const PhantomCore::Rect penpotBounds,
        std::wstring text, const float penpotFontSize, const PhantomCore::Color color,
        std::string name,
        const PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading);
    PhantomCore::Visual2DNode &AddScrollableLabel(PhantomCore::Visual2DNode &parent, PhantomCore::Rect penpotBounds, std::wstring text,
                                    float penpotFontSize, PhantomCore::Color color, std::string name);
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
    SongBackgroundPreview artwork_;
    SongListView songList_;
    std::uint64_t textRevision_{};
    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    bool searchFocused_{};
    bool visible_{true};
    std::unique_ptr<PhantomCore::ITextInput> searchInput_;
    PhantomCore::Visual2DNode *additionMenu_{}, *newDifficultyItem_{}, *convertOsuItem_{};
    PhantomCore::ScreenCanvasHandle canvasHandle_;
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DInputRouter inputRouter_;
    std::optional<PhantomCore::Point> canvasPointer_;
    PhantomCore::Visual2DNode *board_{};
    PhantomCore::Visual2DNode *background_{};
    PhantomCore::Visual2DNode *categoryBar_{};
    PhantomCore::Visual2DNode *categoryLabel_{};
    PhantomCore::Visual2DNode *recordPanel_{};
    PhantomCore::Visual2DNode *informationPanel_{};
    PhantomCore::Visual2DNode *difficultyInformation_{};
    PhantomCore::Visual2DNode *difficultyHeading_{};
    PhantomCore::Visual2DNode *creatorHeading_{};
    PhantomCore::Visual2DNode *informationDivider_{};
    PhantomCore::Visual2DNode *browserPanel_{};
    PhantomCore::Visual2DNode *searchField_{};
    PhantomCore::Visual2DNode *searchCountLabel_{};
    PhantomCore::Visual2DNode *sortSelector_{};
    PhantomCore::Visual2DNode *filterButton_{};
    PhantomCore::Visual2DNode *browserHint_{};
    PhantomCore::Visual2DNode *selectedSongLabel_{};
    PhantomCore::Visual2DNode *selectedArtistLabel_{};
    PhantomCore::Visual2DNode *selectedPatternLabel_{};
    PhantomCore::Visual2DNode *selectedCreatorLabel_{};
    PhantomCore::Visual2DNode *selectedDetailsLabel_{};
    std::array<PhantomCore::Visual2DNode *, 4> extraDetailsLabels_{};
    PhantomCore::Visual2DNode *modeSelector_{};
    PhantomCore::NodeId modeSelectorId_{};
    PhantomCore::Visual2DNode *optionButton_{};
    PhantomCore::Visual2DNode *optionLabel_{};
    PhantomCore::Visual2DNode *backButton_{};
    PhantomCore::Visual2DNode *backFill_{};
    PhantomCore::Visual2DNode *backLabel_{};
    PhantomCore::Visual2DNode *goButton_{};
    PhantomCore::Visual2DNode *goFill_{};
    PhantomCore::Visual2DNode *goLabel_{};
    PhantomCore::NodeId sortSelectorId_{};
    PhantomCore::NodeId searchFieldId_{};
    PhantomCore::NodeId backButtonId_{};
    PhantomCore::NodeId optionButtonId_{};
    PhantomCore::NodeId goButtonId_{};
};
