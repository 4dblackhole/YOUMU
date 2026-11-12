#include "MusicSelectView.h"
#include "Presentation/ScrollableTextComponent.h"
#include "Mode/PlayGameModeFactory.h"
#include "SongSelectLayout.h"
#include "SongSelectionText.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

using namespace PhantomCore;

using namespace song_select;

namespace
{
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
} // namespace

namespace
{
    // Outline geometry leaves the interior empty, so the panel's 90% fill blends
    // directly with the chart BG instead of an opaque border-colored rectangle.
    class SongPanelBorder final : public Visual2DComponent
    {
      public:
        SongPanelBorder(float width, float radius, Color color) : width_(width), radius_(radius), color_(color) {}
        void AppendDrawPackets(std::vector<DrawPacket> &output) const override
        {
            const auto *parent = Owner().Parent();
            if (!parent) return;
            const auto size = parent->NodeSize();
            if (!rectangles_ || size.width != size_.width || size.height != size_.height)
            {
                size_ = size;
                auto rectangles = std::make_shared<std::vector<RectanglePrimitive>>();
                const float radius = std::min({radius_, size.width / 2, size.height / 2});
                const float border = std::min(width_, radius);
                const auto add = [&](float x, float y, float width, float height) {
                    if (width > 0 && height > 0)
                        rectangles->push_back({{x - width_, y - width_, width, height}, color_, 0});
                };
                add(0, radius, border, size.height - radius * 2);
                add(size.width - border, radius, border, size.height - radius * 2);
                constexpr float step = .5F;
                for (float y = 0; y < radius; y += step)
                {
                    const float height = std::min(step, radius - y);
                    const float middle = y + height / 2;
                    const float outside = radius - std::sqrt(std::max(0.0F, radius * radius -
                        (radius - middle) * (radius - middle)));
                    if (middle < border)
                    {
                        add(outside, y, size.width - 2 * outside, height);
                        add(outside, size.height - y - height, size.width - 2 * outside, height);
                    }
                    else
                    {
                        const float innerRadius = radius - border;
                        const float inside = radius - std::sqrt(std::max(0.0F, innerRadius * innerRadius -
                            (radius - middle) * (radius - middle)));
                        for (const auto row : {y, size.height - y - height})
                        {
                            add(outside, row, inside - outside, height);
                            add(size.width - inside, row, inside - outside, height);
                        }
                    }
                }
                rectangles_ = std::move(rectangles);
            }
            DrawPacket packet;
            packet.type = DrawPacketType::RectangleBatch;
            packet.rectangles = rectangles_;
            output.push_back(std::move(packet));
        }
      private:
        float width_, radius_;
        Color color_;
        mutable Size size_{};
        mutable std::shared_ptr<const std::vector<RectanglePrimitive>> rectangles_;
    };
}

void MusicSelectView::CreateCategoryBar()
{
    const auto &text = MusicSelect(texts_);
    auto &bar = AddBorderedPanel(*board_, layout::CategoryBar, PanelWhite, BorderBlue, 3.0F, 20.0F,
                                 "Categories");
    categoryBar_ = &bar;
    auto &all = AddPanel(bar, layout::AllCategory, AccentBlue, "AllCategory", 19.0F);
    categoryLabel_ = &AddLabel(
        all, {0.0F, 0.0F, 86.0F, 38.0F}, std::wstring(text.all), 13.0F, PureWhite,
        "AllCategoryText", TextAlignment::Center);
}

void MusicSelectView::CreateRecordPanel()
{
    recordPanel_ = &AddBorderedPanel(*board_, layout::RecordPanel, PanelWhite, BorderBlue, 3.0F,
                                    24.0F, "RecordPanel");
    records_.InitializeContent(*recordPanel_);
}

void MusicSelectView::CreateSongInformationPanel()
{
    const auto &text = MusicSelect(texts_);
    auto &panel = AddBorderedPanel(*board_, layout::InformationPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "SongInformation");
    informationPanel_ = &panel;
    auto &preview = AddPanel(panel, layout::Preview, {0.765F, 0.906F, 0.980F, 1.0F},
                             "BackgroundPreview", 12.0F);
    auto &previewImage = preview.CreateChild("PreviewImage");
    previewImage.AddComponent<SpriteVisualComponent>();
    SetCornerRadius(previewImage, 12.0F);
    previewImage.SetVisible(false);
    auto &previewTitle = AddLabel(preview, layout::PreviewTitle, std::wstring(text.backgroundPreview), 13.0F,
                                  MutedBlue, "PreviewTitle", TextAlignment::Center);
    auto &previewEmpty = AddLabel(preview, layout::PreviewEmpty, std::wstring(text.noImage), 10.0F, SoftTextBlue,
                                  "PreviewEmpty", TextAlignment::Center);
    artwork_.BindPreview(preview, previewImage, previewTitle, previewEmpty);

    selectedSongLabel_ = &AddScrollableLabel(panel, layout::SelectedSong, std::wstring(text.noSongSelected), 28.0F,
                                   DeepBlue, "SelectedSong");
    selectedArtistLabel_ =
        &AddScrollableLabel(panel, layout::SelectedArtist, L"", 16.0F, MutedBlue, "SelectedArtist");

    auto &information = AddBorderedPanel(panel, layout::DifficultyInformation, PaleBlue, PaleBorder,
                                         1.0F, 12.0F, "DifficultyInformation");
    difficultyInformation_ = &information;
    difficultyHeading_ = &AddLabel(information, layout::DifficultyHeading, std::wstring(text.difficulty), 9.0F,
                                   SoftTextBlue, "DifficultyHeading");
    creatorHeading_ = &AddLabel(information, layout::CreatorHeading, std::wstring(text.creator), 9.0F, SoftTextBlue,
                                "CreatorHeading");
    selectedPatternLabel_ = &AddLabel(information, layout::SelectedDifficulty, L"—", 18.0F,
                                      AccentBlue, "SelectedDifficulty");
    selectedCreatorLabel_ =
        &AddScrollableLabel(information, layout::SelectedCreator, L"—", 18.0F, DeepBlue, "SelectedCreator");
    informationDivider_ =
        &AddPanel(information, layout::InformationDivider, PaleBorder, "InformationDivider");
    selectedDetailsLabel_ =
        &AddLabel(information, layout::SelectedDetails, std::wstring(text.emptyDetails), 9.0F,
                  MutedBlue, "SelectedDetails", TextAlignment::Center);
    for (std::size_t i = 0; i < extraDetailsLabels_.size(); ++i)
        extraDetailsLabels_[i] = &AddLabel(information,
            {16.0F, layout::ExtraDetails.y + 24.0F * static_cast<float>(i), 299.43F, 22.0F},
            L"", 15.0F, DeepBlue, "ExtraDetails" + std::to_string(i));
}

void MusicSelectView::CreateSongBrowser()
{
    const auto &text = MusicSelect(texts_);
    auto &panel = AddBorderedPanel(*board_, layout::BrowserPanel, PanelWhite, BorderBlue, 3.0F,
                                   24.0F, "SongBrowser");
    browserPanel_ = &panel;

    auto &search = CreateButton(
        panel, ScaleTopLeftBounds(layout::SearchField, panel.NodeSize().height),
        std::wstring(text.searchSongs),
        "SearchField");
    searchField_ = &search;
    searchFieldId_ = search.Id();
    ApplyButtonStyle(search, PureWhite, PaleBlue, SoftTextBlue);
    SetCornerRadius(search, 12.0F);
    BindSkinImage(search, screenVisuals_, "MusicSelect", "SearchField");
    auto &searchText = RequireComponent<TextVisualComponent>(search);
    searchText.SetFontSize(CanvasFontSize(11.0F));
    texts_.ApplyFont(searchText);
    searchText.SetHorizontalAlignment(TextAlignment::Leading);
    searchText.SetContentBounds(
        ScaleTopLeftBounds({14.0F, 0.0F, 340.0F, 54.0F}, search.NodeSize().height));
    searchCountLabel_ = &AddLabel(search, layout::SearchCount, L"", 9.0F, SoftTextBlue,
                                  "SearchCount", TextAlignment::Trailing);

    auto &sort = CreateComboBox(
        panel, ScaleTopLeftBounds(layout::SortSelector, panel.NodeSize().height),
        {std::wstring(text.sortModes[0]), std::wstring(text.sortModes[1]),
         std::wstring(text.sortModes[2]), std::wstring(text.sortModes[3])}, "SortSelector");
    sortSelector_ = &sort;
    sortSelectorId_ = sort.Id();
    sort.SetZIndex(10);
    ApplySpriteStyle(sort, PaleBlue, FocusBlue);
    SetCornerRadius(sort, 10.0F);
    BindSkinImage(sort, screenVisuals_, "MusicSelect", "SortSelector");
    auto &sortBehavior = RequireComponent<ComboBoxBehaviorComponent>(sort);
    sortBehavior.SetSelectedIndex(static_cast<std::size_t>(selection_.sortMode_));
    sortBehavior.SetItemHeight(38.0F * DesignToCanvasScale);
    sortBehavior.SetMaxVisibleItems(4);
    sortBehavior.SetFontSize(CanvasFontSize(12.0F));
    texts_.ApplyFont(sortBehavior);
    sortBehavior.SetTextColor(DeepBlue);
    sortBehavior.SetSelectedTextColor(PureWhite);
    sortBehavior.SetPopupBackgroundColor(PureWhite);
    filterButton_ = &CreateButton(panel, {}, L"", "FilterButton");
    RequireComponent<TextVisualComponent>(*filterButton_).SetFontSize(CanvasFontSize(12.0F));
    SetCornerRadius(*filterButton_, 10.0F);
    RefreshFilterButton();

    auto &viewport = AddBorderedPanel(panel, layout::SongViewport, ViewportBlue, SubtleBorder, 1.5F,
                                      14.0F, "SongViewport");
    songList_.Attach(*canvas_, inputRouter_, viewport);

    browserHint_ = &AddLabel(panel, layout::BrowserHint, std::wstring(text.browserHint), 9.0F,
                             SoftTextBlue, "BrowserHint", TextAlignment::Center);
}

void MusicSelectView::CreateFooter()
{
    const auto &text = MusicSelect(texts_);
    auto &option = AddBorderedPanel(*board_, layout::OptionButton, PureWhite, BorderBlue, 3.0F,
                                    12.0F, "OptionSelect");
    optionButton_ = &option;
    option.AddComponent<RectangleCollider2DComponent>();
    option.AddComponent<PointerReceiverComponent>();
    option.AddComponent<ButtonBehaviorComponent>();
    optionLabel_ = &AddLabel(option, {0.0F, 0.0F, 696.0F, 60.0F},
                             std::wstring(text.optionSelect), 14.0F,
                             MutedBlue, "OptionLabel", TextAlignment::Center);
    optionButtonId_ = option.Id();
    if (!IsEditorSongSelect())
    {
        std::vector<std::wstring> modes;
        for (const auto &mode : AvailableInputLayouts())
            modes.push_back(std::wstring(texts_.Get("PlayOptions.gameMode")) + L": " + std::wstring(mode.modeId.begin(), mode.modeId.end()));
        auto &selector = CreateComboBox(*board_, ScaleTopLeftBounds(layout::OptionButton, board_->NodeSize().height), std::move(modes), "GameModeSelect");
        modeSelector_ = &selector;
        modeSelectorId_ = selector.Id();
        auto &modeCombo = RequireComponent<ComboBoxBehaviorComponent>(selector);
        modeCombo.SetFontSize(CanvasFontSize(14.0F));
        texts_.ApplyFont(modeCombo);
        modeCombo.SetTextColor(DeepBlue);
        ApplySpriteStyle(selector, PureWhite, PureWhite);


    }

    struct EdgeButtonNodes
    {
        NodeId id{};
        Visual2DNode *button{};
        Visual2DNode *fill{};
        Visual2DNode *label{};
    };
    auto createEdgeButton = [this](const Rect bounds,
                                   const Rect labelBounds, const std::wstring &text,
                                   const std::string &name) -> EdgeButtonNodes {
        auto &button = CreateButton(
            *board_, ScaleTopLeftBounds(bounds, board_->NodeSize().height), L"", name);
        ApplyButtonStyle(button, PureWhite, PureWhite, PureWhite);
        SetCornerRadius(button, 28.0F);
        auto &fill = AddPanel(button, {4.0F, 4.0F, bounds.width - 8.0F, bounds.height - 8.0F},
                              AccentBlue, name + ".Fill", 24.0F);
        auto &label = AddLabel(button, labelBounds, text, 27.0F, PureWhite, name + ".Label",
                               TextAlignment::Center);
        BindSkinImage(button, screenVisuals_, "MusicSelect", name, name + ".Fill");
        return {button.Id(), &button, &fill, &label};
    };
    const EdgeButtonNodes back =
        createEdgeButton(layout::BackButton, layout::BackLabel, std::wstring(text.back), "Back");
    backButtonId_ = back.id;
    backButton_ = back.button;
    backFill_ = back.fill;
    backLabel_ = back.label;
    const EdgeButtonNodes go =
        createEdgeButton(layout::GoButton, layout::GoLabel, std::wstring(text.go), "Go");
    goButtonId_ = go.id;
    goButton_ = go.button;
    goFill_ = go.fill;
    goLabel_ = go.label;
}

Visual2DNode &MusicSelectView::AddPanel(Visual2DNode &parent,
                                                       const Rect penpotBounds,
                                                       const Color color,
                                                       std::string name,
                                                       const float penpotCornerRadius)
{
    return CreateSongPanel(screenVisuals_, parent, penpotBounds, color, std::move(name), penpotCornerRadius);
}

Visual2DNode &MusicSelectView::AddBorderedPanel(
    Visual2DNode &parent, const Rect penpotBounds,
    const Color fill, const Color border,
    const float penpotBorderWidth, const float penpotCornerRadius, std::string name)
{
    const bool translucent = name == "RecordPanel" || name == "SongInformation" || name == "SongBrowser";
    Color background = translucent ? fill : border;
    if (translucent) background.alpha = .9F;
    auto &outer = AddPanel(parent, penpotBounds, background, std::move(name), penpotCornerRadius);
    auto innerFill = fill;
    if (translucent) innerFill.alpha = 0;
    auto &surface = AddPanel(outer,
             {penpotBorderWidth, penpotBorderWidth,
              std::max(penpotBounds.width - penpotBorderWidth * 2.0F, 0.0F),
              std::max(penpotBounds.height - penpotBorderWidth * 2.0F, 0.0F)},
             innerFill, "Surface", std::max(penpotCornerRadius - penpotBorderWidth, 0.0F));
    if (translucent)
    {
        // Screen rectangles use the engine's opaque material. A white image
        // selects its existing alpha-blended image material without tinting RGB.
        const auto image = screenVisuals_.RegisterImage(
            ResolveBuiltInAsset(L"assets/skins/Default Skin/MusicSelect/PanelFill.png"));
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = background;
        style.normalImage = style.hoveredImage = style.pressedImage = style.disabledImage = image;
        SetSkinBaseStyle(outer, style);
        surface.AddComponent<SongPanelBorder>(penpotBorderWidth * DesignToCanvasScale,
                                              penpotCornerRadius * DesignToCanvasScale, border);
    }
    BindSkinImage(outer, screenVisuals_, "MusicSelect", outer.Name(), "Surface");
    return outer;
}

Visual2DNode &MusicSelectView::AddLabel(
    Visual2DNode &parent, const Rect penpotBounds, std::wstring text,
    const float penpotFontSize, const Color color, std::string name,
    const TextAlignment alignment)
{
    return CreateSongLabel(parent, penpotBounds, std::move(text), penpotFontSize, color,
                           std::move(name), texts_, alignment);
}

Visual2DNode &MusicSelectView::AddScrollableLabel(Visual2DNode &parent, Rect bounds, std::wstring text,
                                                float fontSize, Color color, std::string name)
{
    return CreateScrollableSongLabel(parent, bounds, std::move(text), fontSize, color, std::move(name), texts_);
}

void MusicSelectView::ApplyTexts()
{
    const auto &text = MusicSelect(texts_);
    const auto setLabel = [this](Visual2DNode *node,
                                 const std::wstring_view value) {
        if (node == nullptr)
            return;
        auto &component = RequireComponent<TextVisualComponent>(*node);
        component.SetText(std::wstring(value));
        texts_.ApplyFont(component);
    };

    setLabel(categoryLabel_, text.all);
    records_.RefreshTexts();
    artwork_.ApplyTexts(texts_);
    setLabel(difficultyHeading_, text.difficulty);
    setLabel(creatorHeading_, text.creator);
    for (auto *node : {selectedSongLabel_, selectedArtistLabel_, selectedCreatorLabel_})
        if (node) RequireComponent<ScrollableTextComponent>(*node).SetFont(texts_.CurrentProfile().font);
    setLabel(browserHint_, text.browserHint);
    setLabel(filterButton_, texts_.Get("SongFilter.button"));
    setLabel(optionLabel_, IsEditorSongSelect() ? text.optionSelect : texts_.Get("PlayOptions.title"));
    if (modeSelector_)
    {
        std::vector<std::wstring> items;
        for (const auto &mode : AvailableInputLayouts())
            items.push_back(std::wstring(texts_.Get("PlayOptions.gameMode")) + L": " + std::wstring(mode.modeId.begin(), mode.modeId.end()));
        auto &combo = RequireComponent<ComboBoxBehaviorComponent>(*modeSelector_);
        combo.SetItems(std::move(items));
        texts_.ApplyFont(combo);
    }
    setLabel(backLabel_, text.back);
    setLabel(goLabel_, text.go);
    setLabel(newDifficultyItem_, text.newDifficulty);
    setLabel(convertOsuItem_, text.convertOsu);

    if (sortSelector_ != nullptr)
    {
        auto &combo = RequireComponent<ComboBoxBehaviorComponent>(*sortSelector_);
        const std::size_t selected = combo.SelectedIndex();
        combo.SetItems({std::wstring(text.sortModes[0]), std::wstring(text.sortModes[1]),
                        std::wstring(text.sortModes[2]), std::wstring(text.sortModes[3])});
        combo.SetSelectedIndex(selected);
        texts_.ApplyFont(combo);
    }
    if (searchField_ != nullptr)
    {
        texts_.ApplyFont(
            RequireComponent<TextVisualComponent>(*searchField_));
    }
    options_.RefreshTexts();
    playOptionsPanel_.RefreshTexts();
    filterPanel_.RefreshTexts();
    records_.SetViewport(height_);
    filterPanel_.SetViewport(width_, height_);
    favoriteTabs_.Refresh(selection_.favorites_, selection_.favoriteFolder_);
    favoritesPanel_.RefreshTexts();
    RefreshSelectionPresentation();
    RefreshSearchPresentation();
    textRevision_ = texts_.Revision();
}

void MusicSelectView::RefreshSelectionPresentation()
{
    if (selectedSongLabel_ == nullptr || selectedArtistLabel_ == nullptr ||
        selectedPatternLabel_ == nullptr || selectedCreatorLabel_ == nullptr ||
        selectedDetailsLabel_ == nullptr)
    {
        return;
    }

    auto &songText = RequireComponent<ScrollableTextComponent>(*selectedSongLabel_);
    auto &artistText = RequireComponent<ScrollableTextComponent>(*selectedArtistLabel_);
    auto &creatorText = RequireComponent<ScrollableTextComponent>(*selectedCreatorLabel_);
    const auto &text = MusicSelect(texts_);
    const std::wstring launchError = LaunchErrorText(selection_, text);
    for (auto *label : extraDetailsLabels_)
        if (label) RequireComponent<TextVisualComponent>(*label).SetText(L"");
    if (selection_.visibleSongIndices_.empty())
    {
        artwork_.SetImage({}, canvas_->LogicalSize());
        songText.SetText(std::wstring(text.noSongSelected));
        artistText.SetText(L"");
        RequireComponent<TextVisualComponent>(*selectedPatternLabel_).SetText(L"—");
        creatorText.SetText(L"—");
        RequireComponent<TextVisualComponent>(*selectedDetailsLabel_)
            .SetText(launchError.empty() ? std::wstring(text.emptyDetails) : launchError);
        return;
    }

    selection_.focusedSongPosition_ =
        std::min(selection_.focusedSongPosition_, selection_.visibleSongIndices_.size() - 1);
    const auto &song =
        selection_.catalog_.songs[selection_.visibleSongIndices_[selection_.focusedSongPosition_]];
    songText.SetText(DisplayLines(song.music.names, song.metadataPath.stem().wstring()));
    artistText.SetText(DisplayLines(song.music.artists));
    if (song.patterns.empty())
    {
        artwork_.SetImage({}, canvas_->LogicalSize());
        RequireComponent<TextVisualComponent>(*selectedPatternLabel_)
            .SetText(std::wstring(text.noDifficulty));
        creatorText.SetText(L"—");
        RequireComponent<TextVisualComponent>(*selectedDetailsLabel_)
            .SetText(std::wstring(text.emptyDetails));
        return;
    }

    selection_.selectedPatternIndex_ =
        std::min(selection_.selectedPatternIndex_, song.patterns.size() - 1);
    const auto &selected = song.patterns[selection_.selectedPatternIndex_];
    artwork_.SetImage(selected.pattern.backgroundFile.empty()
                          ? std::filesystem::path{}
                          : settings_.SongsRoot() / L"BG" / selected.pattern.backgroundFile,
                      canvas_->LogicalSize(), selected.pattern.backgroundOffsetX,
                      selected.pattern.backgroundOffsetY);
    RequireComponent<TextVisualComponent>(*selectedPatternLabel_)
        .SetText(PatternName(selected));
    creatorText.SetText(DisplayLines(selected.pattern.makers));
    std::wstring mode =
        selected.pattern.mode.empty() ? L"—" : DecodeDisplayText(selected.pattern.mode);
    std::ranges::transform(mode, mode.begin(), [](const wchar_t character) {
        return static_cast<wchar_t>(std::towupper(character));
    });
    if (!launchError.empty())
    {
        RequireComponent<TextVisualComponent>(*selectedDetailsLabel_)
            .SetText(launchError);
        return;
    }
    const std::wstring bpm = BpmText(selected.pattern.baseBpm);
    const std::wstring noteCount = selected.noteCount
        ? std::to_wstring(*selected.noteCount) : L"—";
    RequireComponent<TextVisualComponent>(*selectedDetailsLabel_)
        .SetText(std::vformat(text.detailsFormat,
                              std::make_wformat_args(bpm, noteCount, mode)));
    const auto range = !selected.bpmRange ? L"—"
        : selected.bpmRange->first == selected.bpmRange->second ? BpmText(selected.bpmRange->first)
        : BpmText(selected.bpmRange->first) + L" – " + BpmText(selected.bpmRange->second);
    std::wstring duration = L"—";
    if (selection_.selectedDurationMicroseconds_)
    {
        const auto seconds = *selection_.selectedDurationMicroseconds_ / 1'000'000;
        duration = std::format(L"{}:{:02}", seconds / 60, seconds % 60);
    }
    const std::array values{std::format(L"{}  {:.2f}", texts_.Get("MusicSelect.judgeLevelLabel"),
                                        selected.pattern.judgementLevel.Value()),
                            std::format(L"{}  {:.2f}", texts_.Get("MusicSelect.hpLevelLabel"),
                                        selected.pattern.hpLevel.Value()),
                            std::wstring(texts_.Get("MusicSelect.bpmRangeLabel")) + L"  " + range,
                            std::wstring(texts_.Get("MusicSelect.durationLabel")) + L"  " + duration};
    for (std::size_t i = 0; i < values.size(); ++i)
        if (extraDetailsLabels_[i])
        {
            auto &label = RequireComponent<TextVisualComponent>(*extraDetailsLabels_[i]);
            label.SetText(values[i]);
            texts_.ApplyFont(label);
        }
}
