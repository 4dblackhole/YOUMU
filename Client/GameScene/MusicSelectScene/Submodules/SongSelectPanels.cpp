#include "MusicSelectView.h"
#include "Catalog/SongLayout.h"
#include "Mode/PlayGameModeFactory.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Presentation/UiDropdown.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
#include <cmath>

using namespace PhantomCore;

using namespace song_select;

namespace
{
    namespace theme = ui_colors::theme;

    [[nodiscard]] std::wstring LaunchErrorText(
        const SongSelectionState &selection,
        const MusicSelectTextSet &text)
    {
        using Kind = SongSelectionState::LaunchErrorKind;
        if (selection.launchErrorKind_ == Kind::None)
        {
            return {};
        }

        const std::wstring_view prefix =
            selection.launchErrorKind_ == Kind::Catalog        ? text.catalogErrorPrefix
            : selection.launchErrorKind_ == Kind::UnsupportedMode
                ? text.unsupportedModePrefix
                : text.patternErrorPrefix;
        const std::wstring detail =
            selection.launchErrorDetail_.empty()
                ? std::wstring(text.unknownChartError)
                : DecodeDisplayText(selection.launchErrorDetail_);
        return std::wstring(prefix) + detail;
    }


    constexpr std::array<const char *, 6> StatNames{"BPM", "Length", "Notes", "JudgeLevel", "HP", "Mode"};
} // namespace

void MusicSelectView::CreateSheet()
{
    // Song details and the song list share one sheet; the record column stays a window onto
    // the chart BG. The editor's song select uses the same sheet without the record column.
    sheetEdge_ = &CreatePanel(*board_, {}, "SheetEdge");
    ApplySkinPart(*sheetEdge_, screenVisuals_, "Panels.SheetEdge", {0.0F, 0.0F, 0.0F, 0.0F}, 0.0F);
    sheet_ = &CreatePanel(*board_, {}, "Sheet");
    ApplySkinPart(*sheet_, screenVisuals_, "Panels.Sheet", theme::SurfaceSheet, 0.0F);

    // The image covers a 16:9 screen. A wider screen continues it with its right-most column.
    sheetExtension_ = &CreatePanel(*board_, {}, "SheetExtension");
    ApplySkinPart(*sheetExtension_, screenVisuals_, "Panels.Sheet", theme::SurfaceSheet, 0.0F);
    const float column = 1.0F / (layout::SheetImageWidth * 1.5F); // One source pixel at 1080p.
    RequireComponent<SpriteVisualComponent>(*sheetExtension_).SetUvTransform({column, 1.0F}, {1.0F - column * 2.0F, 0.0F});
    sheetExtension_->SetVisible(false);

    columnRule_ = &CreatePanel(*board_, {}, "ColumnRule");
    ApplyFlatStyle(*columnRule_, theme::RuleStrong);
    SetCornerRadius(*columnRule_, 0.0F);
}

void MusicSelectView::CreateRecordPanel()
{
    recordPanel_ = &board_->CreateChild("RecordPanel");
    const auto frame = CalculateFrame(layout::CanvasSize.width);
    recordPanel_->SetBounds(TopLeft(frame.record, layout::CanvasSize.height));
    records_.InitializeContent(*recordPanel_);
}

void MusicSelectView::CreateSongInformationPanel()
{
    informationPanel_ = &board_->CreateChild("SongInformation");
    auto &panel = *informationPanel_;
    const auto frame = CalculateFrame(layout::CanvasSize.width);
    panel.SetBounds(TopLeft(frame.detail, layout::CanvasSize.height));

    auto &jacket = CreatePanel(panel, {}, "BackgroundPreview");
    ApplyFlatStyle(jacket, {0.765F, 0.906F, 0.980F, 1.0F});
    auto &jacketImage = CreateSprite(jacket, {}, {}, "PreviewImage");
    jacketImage.SetVisible(false);
    artwork_.BindPreview(jacket, jacketImage);

    selectedArtistLabel_ =
        &CreateCaption(panel, {}, L"", 15.0F, 700, theme::InkSoft, "SelectedArtist", TextAlignment::Center);
    // A music note before the artist and a quill before the creator, in the light cobalt of the mock-up.
    artistIcon_ = &CreateSprite(panel, {}, screenVisuals_.RegisterImage(Common(L"Icons.music")), "ArtistIcon");
    RequireComponent<SpriteVisualComponent>(*artistIcon_).SetTint(theme::NameIcon);
    selectedSongLabel_ =
        &CreateCaption(panel, {}, L"", 30.0F, 900, theme::Ink, "SelectedSong", TextAlignment::Center);
    selectedCreatorLabel_ =
        &CreateCaption(panel, {}, L"", 14.0F, 700, theme::Ink, "SelectedCreator", TextAlignment::Center);
    creatorIcon_ = &CreateSprite(panel, {}, screenVisuals_.RegisterImage(Common(L"Icons.pencil")), "CreatorIcon");
    // The name lines take the pointer so their tooltips can list every name.
    for (auto *label : {selectedArtistLabel_, selectedSongLabel_, selectedCreatorLabel_})
    {
        label->AddComponent<RectangleCollider2DComponent>();
        label->AddComponent<PointerReceiverComponent>();
    }
    RequireComponent<SpriteVisualComponent>(*creatorIcon_).SetTint(theme::NameIcon);

    // Cobalt parallelogram with the chart name and its difficulty value.
    difficultyBlock_ = &CreatePanel(panel, {}, "DifficultyInformation");
    ApplySkinPart(*difficultyBlock_, screenVisuals_, "Detail.DifficultyBlock", theme::Brand);
    selectedPatternLabel_ =
        &CreateCaption(*difficultyBlock_, {}, L"", 15.0F, 900, PureWhite, "SelectedDifficulty");
    levelBadge_ = &CreatePanel(*difficultyBlock_, {}, "LevelBadge");
    ApplySkinPart(*levelBadge_, screenVisuals_, "Detail.LevelBadge", theme::Sun);
    // The value comes from the catalog; a chart without a calculated difficulty shows a dash.
    static_cast<void>(CreateNumber(*levelBadge_, {0.0F, 0.0F, 40.0F, 22.0F}, L"—", 14.0F, 800, theme::BrandDeep,
                                   "SelectedLevel", TextAlignment::Center));

    for (std::size_t i = 0; i < statRules_.size(); ++i)
    {
        statRules_[i] = &CreateSprite(panel, {}, {}, "Detail.Rule." + std::to_string(i));
        ApplyFlatStyle(*statRules_[i], i < 2 ? theme::RuleStrong : theme::Rule);
    }
    for (std::size_t i = 0; i < StatNames.size(); ++i)
    {
        const std::string name = std::string("Detail.") + StatNames[i];
        statCaptions_[i] =
            &CreateCaption(panel, {}, L"", 13.0F, 700, theme::InkSoft, name + ".Caption", TextAlignment::Center);
        statValues_[i] =
            &CreateNumber(panel, {}, L"", 26.0F, 800, theme::Ink, name + ".Value", TextAlignment::Center);
    }
    // The Base BPM cell takes the pointer: resting on it shows the chart's BPM range.
    for (auto *bpm : {statCaptions_[0], statValues_[0]})
    {
        bpm->AddComponent<RectangleCollider2DComponent>();
        bpm->AddComponent<PointerReceiverComponent>();
    }
}

void MusicSelectView::CreateSongBrowser()
{
    const auto &text = MusicSelect(texts_);
    browserPanel_ = &board_->CreateChild("SongBrowser");
    auto &panel = *browserPanel_;
    const auto frame = CalculateFrame(layout::CanvasSize.width);
    panel.SetBounds(TopLeft(frame.list, layout::CanvasSize.height));
    const float height = panel.NodeSize().height;

    auto &search = CreateButton(panel, TopLeft(layout::SearchField, height), std::wstring(text.searchSongs),
                                "SearchField");
    searchField_ = &search;
    searchFieldId_ = search.Id();
    ApplySkinPart(search, screenVisuals_, "SearchField", {1.0F, 1.0F, 1.0F, 0.74F});
    auto &searchText = RequireComponent<TextVisualComponent>(search);
    StyleText(searchText, 14.0F, 600, theme::Ink);
    // A magnifier leads the field; the song count sits at its right end.
    auto &magnifier = CreateSprite(search, {12.0F, (layout::SearchField.height - 16.0F) * 0.5F, 16.0F, 16.0F},
                                   screenVisuals_.RegisterImage(Common(L"Icons.search")), "SearchIcon");
    RequireComponent<SpriteVisualComponent>(magnifier).SetTint(theme::Caption);
    searchText.SetContentBounds({36.0F, 0.0F, layout::SearchField.width - 36.0F - 84.0F, layout::SearchField.height});
    searchCountLabel_ = &CreateNumber(search, {layout::SearchField.width - 88.0F, 0.0F, 80.0F, layout::SearchField.height},
                                      L"", 12.0F, 500, theme::Caption, "SearchCount", TextAlignment::Trailing, false);

    auto &sort = CreateDropdown(
        panel, TopLeft(layout::SortSelector, height),
        {std::wstring(text.sortModes[0]), std::wstring(text.sortModes[1]),
         std::wstring(text.sortModes[2]), std::wstring(text.sortModes[3])}, "SortSelector");
    sortSelector_ = &sort;
    sortSelectorId_ = sort.Id();
    sort.SetZIndex(10);
    ApplySkinPart(sort, screenVisuals_, "SortSelector", {1.0F, 1.0F, 1.0F, 0.74F});
    auto &sortBehavior = RequireDropdown(sort);
    sortBehavior.SetSelectedIndex(static_cast<std::size_t>(selection_.sortMode_));
    sortBehavior.SetValueStyle(13.0F, 600, theme::Ink);

    filterButton_ = &CreateButton(panel, TopLeft(layout::FilterButton, height), L"", "FilterButton");
    SetCornerRadius(*filterButton_, 2.0F);
    StyleText(RequireComponent<TextVisualComponent>(*filterButton_), 13.0F, 800, theme::BrandText,
              TextAlignment::Center);
    RefreshFilterButton();

    auto &viewport = panel.CreateChild("SongViewport");
    viewport.SetBounds(TopLeft({0.0F, layout::ViewportTop, layout::ListWidth, layout::BodyHeight - layout::ViewportTop},
                               height));
    songList_.Attach(*canvas_, inputRouter_, viewport);
    songList_.SetTooltip(tooltip_);
}

void MusicSelectView::CreateFooter()
{
    const auto &text = MusicSelect(texts_);
    footerBar_ = &CreatePanel(*board_, {}, "FooterBar");
    ApplySkinPart(*footerBar_, screenVisuals_, "BottomBar", {0.941F, 0.973F, 1.0F, 0.96F});

    const auto cell = [this](const std::string &name, const std::string_view key) -> Visual2DNode &
    {
        auto &button = CreateButton(*board_, {}, L"", name);
        ApplySkinPart(button, screenVisuals_, key, {1.0F, 1.0F, 1.0F, 0.0F});
        return button;
    };
    if (!IsEditorSongSelect())
    {
        std::vector<std::wstring> modes;
        for (const auto &mode : AvailableInputLayouts())
            modes.emplace_back(mode.modeId.begin(), mode.modeId.end());
        auto &selector = CreateDropdown(*board_, {}, std::move(modes), "GameModeSelect");
        modeSelector_ = &selector;
        modeSelectorId_ = selector.Id();
        ApplySkinPart(selector, screenVisuals_, "ModeSelector", {1.0F, 1.0F, 1.0F, 0.0F});
        RequireDropdown(selector).SetValueStyle(16.0F, 900, theme::BrandText);

        playOptionsButton_ = &cell("PlayOptions", "PlayOptions");
        playOptionsButtonId_ = playOptionsButton_->Id();
        StyleText(RequireComponent<TextVisualComponent>(*playOptionsButton_), 15.0F, 800, theme::InkSoft,
                  TextAlignment::Center);
        // Gold count of options that differ from a plain play; hidden when there are none.
        playOptionsBadge_ = &CreatePanel(*playOptionsButton_, {}, "PlayOptions.Badge");
        ApplyFlatStyle(*playOptionsBadge_, theme::Sun);
        SetCornerRadius(*playOptionsBadge_, 2.0F);
        playOptionsBadgeText_ = &CreateNumber(*playOptionsBadge_, {0.0F, 0.0F, 18.0F, 18.0F}, L"", 12.0F, 700,
                                              theme::BrandDeep, "Count", TextAlignment::Center, false);
    }
    // The gear opens the global options; Ctrl+O opens the same panel.
    optionButton_ = &cell("OptionSelect", "OptionSelect");
    optionButtonId_ = optionButton_->Id();
    auto &gear = CreateSprite(*optionButton_,
                              {(layout::Gear.width - 20.0F) * 0.5F, (layout::Gear.height - 20.0F) * 0.5F, 20.0F, 20.0F},
                              screenVisuals_.RegisterImage(Common(L"Icons.settings")), "OptionIcon");
    RequireComponent<SpriteVisualComponent>(gear).SetTint(theme::InkSoft);

    backButton_ = &CreateButton(*board_, {}, L"", "Back");
    backButtonId_ = backButton_->Id();
    ApplySkinPart(*backButton_, screenVisuals_, "Back", theme::BrandDeep);
    backLabel_ = &CreateCaption(*backButton_, {}, std::wstring(text.back), 17.0F, 800, PureWhite, "Back.Label",
                                TextAlignment::Center);

    goButton_ = &CreateButton(*board_, {}, L"", "Go");
    goButtonId_ = goButton_->Id();
    {
        // GO keeps full opacity when disabled: the skin supplies its own disabled image.
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = theme::Brand;
        style.disabled = theme::Caption;
        SetSkinBaseStyle(*goButton_, style);
        BindSkinImage(*goButton_, screenVisuals_, "MusicSelect", "Go");
    }
    goLabel_ = &CreateCaption(*goButton_, {}, std::wstring(text.go), 30.0F, 900, PureWhite, "Go.Label",
                              TextAlignment::Center);

    // Launch failures appear in a bubble right above GO instead of inside the song details.
    goError_ = &CreatePanel(*board_, {}, "GoError");
    ApplySkinPart(*goError_, screenVisuals_, "GoError", PureWhite);
    goError_->SetZIndex(20);
    goErrorLabel_ = &CreateCaption(*goError_, {}, L"", 12.0F, 700, theme::Danger, "GoError.Text");
    goError_->SetVisible(false);
}

void MusicSelectView::ApplyTooltips()
{
    const auto hint = [this](const Visual2DNode *node, TooltipText text) {
        if (node != nullptr)
            tooltip_.Set(*node, std::move(text));
    };
    hint(backButton_, {L"", L"Esc"});
    hint(goButton_, {L"", L"Enter"});
    hint(optionButton_, {std::wstring(texts_.Get("Tooltips.options")), L"Ctrl+O"});
    hint(modeSelector_, {std::wstring(texts_.Get("Tooltips.gameMode"))});
    hint(records_.QueryButton(), {std::wstring(texts_.Get("Tooltips.filterRecords"))});
    hint(favoriteTabs_.ManageButton(), {std::wstring(texts_.Get("Tooltips.favoriteFolders"))});
}

void MusicSelectView::ApplyTexts()
{
    const auto &text = MusicSelect(texts_);
    ApplyTooltips();
    records_.RefreshTexts();
    if (filterButton_)
        SetCaption(*filterButton_, std::wstring(texts_.Get("SongFilter.button")));
    if (playOptionsButton_)
        SetCaption(*playOptionsButton_, std::wstring(texts_.Get("PlayOptions.title")));
    RefreshPlayOptionsBadge();
    if (backLabel_)
        SetCaption(*backLabel_, std::wstring(text.back));
    if (goLabel_)
        SetCaption(*goLabel_, std::wstring(text.go));
    const auto setButton = [this](Visual2DNode *node, const std::wstring_view value)
    {
        if (node == nullptr)
            return;
        auto &component = RequireComponent<TextVisualComponent>(*node);
        component.SetText(std::wstring(value));
        texts_.ApplyFont(component);
    };
    if (importLabel_)
        SetCaption(*importLabel_, std::wstring(text.convertOsu));
    if (importHelp_)
        SetCaption(*importHelp_, std::wstring(text.convertOsuHelp));

    const std::array<std::wstring, 6> captions{L"Base BPM",
                                               std::wstring(texts_.Get("MusicSelect.durationLabel")),
                                               L"Notes",
                                               std::wstring(texts_.Get("MusicSelect.judgeLevelLabel")),
                                               std::wstring(texts_.Get("MusicSelect.hpLevelLabel")),
                                               L"Mode"};
    for (std::size_t i = 0; i < captions.size(); ++i)
        if (statCaptions_[i])
            SetCaption(*statCaptions_[i], captions[i]);

    if (sortSelector_ != nullptr)
    {
        auto &combo = RequireDropdown(*sortSelector_);
        const std::size_t selected = combo.SelectedIndex();
        combo.SetItems({std::wstring(text.sortModes[0]), std::wstring(text.sortModes[1]),
                        std::wstring(text.sortModes[2]), std::wstring(text.sortModes[3])});
        combo.SetSelectedIndex(selected);
    }
    if (searchField_ != nullptr)
    {
        auto &search = RequireComponent<TextVisualComponent>(*searchField_);
        search.SetFont(ui_fonts::Text(text.searchSongs));
    }
    options_.RefreshTexts();
    playOptionsPanel_.RefreshTexts();
    filterPanel_.RefreshTexts();
    records_.SetViewport(height_);
    filterPanel_.SetViewport(width_, height_);
    favoriteTabs_.Refresh(selection_.favorites_, selection_.favoriteFolder_);
    favoritesPanel_.RefreshTexts();
    osuImportPanel_.RefreshTexts();
    RefreshSelectionPresentation();
    RefreshSearchPresentation();
    textRevision_ = texts_.Revision();
}

void MusicSelectView::RefreshSelectionPresentation()
{
    if (selectedSongLabel_ == nullptr || selectedArtistLabel_ == nullptr || selectedPatternLabel_ == nullptr)
    {
        return;
    }
    if (selectedCreatorLabel_ == nullptr || difficultyBlock_ == nullptr)
    {
        return;
    }

    const auto &text = MusicSelect(texts_);
    const std::wstring launchError = LaunchErrorText(selection_, text);
    if (goError_ && goErrorLabel_)
    {
        goError_->SetVisible(!launchError.empty());
        SetCaption(*goErrorLabel_, launchError);
    }
    // Empties the chart-dependent half of the details; the song half stays as set by the caller.
    const auto clearPattern = [this]
    {
        difficultyBlock_->SetVisible(false);
        SetCaption(*selectedPatternLabel_, L"");
        SetCaption(*selectedCreatorLabel_, L"");
        tooltip_.Set(*selectedCreatorLabel_, {});
        creatorIcon_->SetVisible(false);
        for (auto *value : statValues_)
            if (value)
                RequireComponent<TextVisualComponent>(*value).SetText(L"");
        for (auto *bpm : {statCaptions_[0], statValues_[0]})
            if (bpm)
                tooltip_.Set(*bpm, {});
        for (auto *caption : statCaptions_)
            if (caption)
                caption->SetVisible(false);
        for (auto *rule : statRules_)
            if (rule)
                rule->SetVisible(false);
        if (goButton_)
            goButton_->SetEnabled(false);
    };
    if (selection_.visibleSongIndices_.empty())
    {
        artwork_.SetImage({}, board_->NodeSize());
        SetCaption(*selectedSongLabel_, L"");
        SetCaption(*selectedArtistLabel_, L"");
        tooltip_.Set(*selectedSongLabel_, {});
        tooltip_.Set(*selectedArtistLabel_, {});
        artistIcon_->SetVisible(false);
        clearPattern();
        return;
    }

    selection_.focusedSongPosition_ =
        std::min(selection_.focusedSongPosition_, selection_.visibleSongIndices_.size() - 1);
    const auto &song =
        selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
    // One notation is shown; the tooltips list every notation and search covers them all.
    const auto notation = selection_.notation_;
    SetCaption(*selectedSongLabel_, SongTitle(song, notation));
    const std::wstring artist = ArtistName(song, notation);
    SetCaption(*selectedArtistLabel_, artist);
    artistIcon_->SetVisible(!artist.empty());
    tooltip_.Set(*selectedSongLabel_, {AllNames(song.music.names, song.music.romanizedName)});
    tooltip_.Set(*selectedArtistLabel_, {AllNames(song.music.artists, song.music.romanizedArtist)});
    if (song.patterns.empty())
    {
        artwork_.SetImage({}, board_->NodeSize());
        clearPattern();
        return;
    }

    selection_.selectedPatternIndex_ =
        std::min(selection_.selectedPatternIndex_, song.patterns.size() - 1);
    const auto &selected = song.patterns[selection_.selectedPatternIndex_];
    artwork_.SetImage(SongLayout::BackgroundPathOfPattern(selected.patternPath, selected.pattern.backgroundFile),
                      board_->NodeSize(), selected.pattern.backgroundOffsetX,
                      selected.pattern.backgroundOffsetY);
    difficultyBlock_->SetVisible(true);
    SetCaption(*selectedPatternLabel_, PatternName(selected, notation));
    RequireComponent<TextVisualComponent>(*levelBadge_->Children().front())
        .SetText(selected.calculatedDifficulty ? std::format(L"{:.2f}", *selected.calculatedDifficulty) : std::wstring(L"—"));
    const std::wstring creator = MakerName(selected, notation);
    SetCaption(*selectedCreatorLabel_, creator);
    tooltip_.Set(*selectedCreatorLabel_, {AllNames(selected.pattern.makers, selected.pattern.romanizedMaker)});
    creatorIcon_->SetVisible(!creator.empty());
    if (goButton_)
        goButton_->SetEnabled(true);

    // The cell shows the YMP Base BPM, the speed Accelerate BPM and BPM scroll speed divide by; the
    // range of the valid timing sections (one value without BPM changes) is its tooltip.
    const bool knownBase = std::isfinite(selected.pattern.baseBpm) && selected.pattern.baseBpm > 0;
    const auto baseBpm = knownBase ? BpmText(selected.pattern.baseBpm) : std::wstring(L"—");
    std::wstring range;
    if (selected.bpmRange)
        range = selected.bpmRange->first == selected.bpmRange->second
                    ? BpmText(selected.bpmRange->first)
                    : BpmText(selected.bpmRange->first) + L"–" + BpmText(selected.bpmRange->second);
    for (auto *bpm : {statCaptions_[0], statValues_[0]})
        if (bpm)
            tooltip_.Set(*bpm, range.empty() ? TooltipText{} : TooltipText{L"BPM " + range});
    std::wstring duration = L"—";
    if (selection_.selectedDurationMicroseconds_)
    {
        const auto seconds = *selection_.selectedDurationMicroseconds_ / 1'000'000;
        duration = std::format(L"{}:{:02}", seconds / 60, seconds % 60);
    }
    const std::array<std::wstring, 6> values{
        baseBpm,
        duration,
        selected.noteCount ? std::to_wstring(*selected.noteCount) : std::wstring(L"—"),
        std::format(L"{:.2f}", selected.pattern.judgementLevel.Value()),
        std::format(L"{:.2f}", selected.pattern.hpLevel.Value()),
        selected.pattern.mode.empty() ? std::wstring(L"—") : DecodeDisplayText(selected.pattern.mode)};
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (statCaptions_[i])
            statCaptions_[i]->SetVisible(true);
        if (statValues_[i])
            RequireComponent<TextVisualComponent>(*statValues_[i]).SetText(values[i]);
    }
    for (auto *rule : statRules_)
        if (rule)
            rule->SetVisible(true);
    UpdateSongInformationLayout();
}
