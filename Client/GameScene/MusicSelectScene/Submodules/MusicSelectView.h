#pragma once
#include "PhantomCore.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/PlayOptionsPanel.h"
#include "SongSelectionState.h"
#include "SongFilterPanel.h"
#include "RecordFilterPanel.h"
#include "SongFavoriteTabs.h"
#include "SongFavoritesPanel.h"
#include "SongSelectPurpose.h"
#include "SongBackgroundPreview.h"
#include "Texts/TextCatalog.h"
#include <filesystem>
#include <array>
#include <vector>
#include <future>
#include "Records/PlayRecordStore.h"

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
          settings_(options), options_(texts, options, &visuals), recordDatabase_(std::move(recordDatabase)),
          artwork_(visuals)
    {
    }
    void SetVisible(bool visible)
    {
        visible_ = visible;
        if (!visible) { CloseAdditionMenu(); filterPanel_.Close(); favoritesPanel_.Close(); recordFilterPanel_.Close(); playOptionsPanel_.Dismiss(); }
        else ++recordRevision_;
        static_cast<void>(canvasHandle_.SetVisible(visible));
        UpdateSearchInputVisibility();
    }
    void Initialize(const PhantomCore::EngineServices &services);
    SongSelectCommand Update(const PhantomCore::InputState &input, double deltaSeconds);
    void OnResize(const std::uint32_t width, const std::uint32_t height);
    void Shutdown() noexcept;
    void RebuildVisibleSongs();
    void RefreshSelectionPresentation();

  private:
    using SortMode = SongSelectionState::SortMode;
    struct SongCard
    {
        std::size_t catalogIndex{};
        float top{};
        float height{};
        PhantomCore::Visual2DNode *button{};
        PhantomCore::Visual2DNode *title{};
        PhantomCore::Visual2DNode *artist{};
        std::size_t visiblePosition{};
        PhantomCore::NodeId createDifficultyId{};
    };

    void CreateCategoryBar();
    void CreateRecordPanel();
    void UpdateRecords();
    void ShowRecords(const std::vector<PlayRecord> &records);
    void OpenRecordFilter();
    bool ProcessRecordFilterPanel(const PhantomCore::InputState &input);
    bool ProcessPlayOptionsPanel(const PhantomCore::InputState &input);
    void LayoutRecordControls();
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
    void RebuildSongCards();
    void UpdateVisibleSongCards();
    void CreateSongCard(const std::size_t visiblePosition, const std::size_t catalogIndex,
                        const float top, const float height, const bool focused);
    PhantomCore::NodeId CreateDifficultyButton(PhantomCore::Visual2DNode &card, std::size_t visiblePosition);
    void RefreshSearchPresentation();
    void EnsureFocusedCardVisible();
    void ApplyScrollOffset();
    void UpdateScrollbar();
    void ProcessPointer(const PhantomCore::InputState &input);
    bool ProcessActions();
    bool ProcessFilterPanel(const PhantomCore::InputState &input);
    bool ProcessFavoritesPanel(const PhantomCore::InputState &input);
    void OpenFavoritesPanel();
    [[nodiscard]] bool IsSelectionModalVisible() const noexcept
    { return filterPanel_.IsVisible() || favoritesPanel_.IsVisible() || recordFilterPanel_.IsVisible() || playOptionsPanel_.IsVisible(); }
    [[nodiscard]] float VisibleSongBottom() const noexcept;
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
    RecordFilterPanel recordFilterPanel_;
    RecordFilter recordFilter_;
    SongFavoriteTabs favoriteTabs_;
    SongFavoritesPanel favoritesPanel_;
    std::filesystem::path recordDatabase_;
    SongBackgroundPreview artwork_;
    std::uint64_t textRevision_{};
    std::vector<SongCard> songCards_;
    std::vector<SongCard> songRows_;
    std::size_t firstVisibleRow_{}, lastVisibleRow_{};
    bool songCardsDirty_{true};
    std::vector<std::pair<PhantomCore::NodeId, std::size_t>> difficultyButtonIds_;
    float scrollOffset_{};
    float contentHeight_{};
    float songContentWidth_{454.29F};
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
    std::future<std::vector<PlayRecord>> recordJob_;
    std::filesystem::path recordPattern_;
    std::string recordMode_, recordQuery_;
    std::size_t recordOffset_{};
    std::uint64_t recordRevision_{1}, recordJobRevision_{}, recordDisplayRevision_{};
    bool recordNext_{};
    PhantomCore::Visual2DNode *recordQueryButton_{}, *recordPreviousButton_{}, *recordNextButton_{};
    PhantomCore::Visual2DNode *recordRows_{};
    PhantomCore::Visual2DNode *recordPanel_{};
    PhantomCore::Visual2DNode *recordSelector_{};
    PhantomCore::Visual2DNode *emptyRecordMessage_{};
    PhantomCore::Visual2DNode *informationPanel_{};
    PhantomCore::Visual2DNode *difficultyInformation_{};
    PhantomCore::Visual2DNode *difficultyHeading_{};
    PhantomCore::Visual2DNode *creatorHeading_{};
    PhantomCore::Visual2DNode *informationDivider_{};
    PhantomCore::Visual2DNode *browserPanel_{};
    PhantomCore::Visual2DNode *songViewport_{};
    PhantomCore::Visual2DNode *songContent_{};
    PhantomCore::Visual2DNode *scrollbarTrack_{};
    PhantomCore::Visual2DNode *scrollbarHandle_{};
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
    PhantomCore::NodeId recordSelectorId_{};
    PhantomCore::NodeId sortSelectorId_{};
    PhantomCore::NodeId searchFieldId_{};
    PhantomCore::NodeId backButtonId_{};
    PhantomCore::NodeId optionButtonId_{};
    PhantomCore::NodeId goButtonId_{};
};
