#pragma once

#include "App/OptionSettings.h"
#include "App/SkinSetSelection.h"
#include "PhantomCore.h"
#include "Presentation/UiTooltip.h"
#include "Presentation/UiScrollbar.h"
#include "Mode/ModeInputLayout.h"
#include "Texts/TextCatalog.h"

#include <array>
#include <optional>
#include <vector>

struct ModeKeyOverlay;

// The left options drawer (UI-02): a category rail and one scrolling body in which the
// General, Display, Audio and Input sections follow each other. Every choice applies and
// saves at once; a mode with a key overlay edits its keys in a centred modal.
class OptionsPanel final
{
  public:
    OptionsPanel(TextCatalog &texts, OptionSettings &options, PhantomCore::ScreenVisual2DManager *visuals = nullptr) noexcept
        : texts_(texts), options_(options), visuals_(visuals) {}

    void Initialize(PhantomCore::Visual2DCanvas &canvas, PhantomCore::AudioSystem &audio,
                    PhantomCore::IWindowControl *window = nullptr);
    void Shutdown() noexcept;
    void Toggle();
    void Update(double deltaSeconds);
    // True consumes a panel keyboard command, including the closing frame.
    bool ProcessInput(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                      PhantomCore::NodeId hoveredNode, PhantomCore::Visual2DInputRouter &inputRouter);
    bool HandleKey(std::uint16_t virtualKey, PhantomCore::Visual2DInputRouter &inputRouter);
    [[nodiscard]] bool ProcessAction(const PhantomCore::Action &action);
    void RefreshTexts();
    [[nodiscard]] bool OwnsKeyboard() const noexcept
    {
        return captureRow_.has_value() || capturingRestartKey_ || editingOffset_;
    }
    [[nodiscard]] bool IsVisible() const noexcept;

  private:
    static constexpr std::size_t SectionCount = 4;
    struct Section
    {
        PhantomCore::Visual2DNode *railButton{}, *railIcon{}, *railLabel{};
        PhantomCore::Visual2DNode *headerIcon{}, *headerLabel{}, *rule{};
        float top{}, height{};
    };
    struct KeyRow
    {
        std::string mode, binding, displayName;
        std::size_t slot{};
        PhantomCore::Visual2DNode *label{}, *button{};
        PhantomCore::Visual2DNode *clear{};
        // Whether the slot holds a key, as last drawn; the clear button needs one.
        bool assigned{};
    };
    // A mode registered with a key overlay edits its keys in one centred modal opened
    // from its Input card; its key rows are children of that modal.
    struct KeyOverlay
    {
        const ModeKeyOverlay *description{};
        std::string mode;
        PhantomCore::Visual2DNode *scrim{}, *panel{}, *close{}, *status{}, *statusIcon{}, *title{};
        PhantomCore::Visual2DNode *open{}, *openTitle{}, *openSummary{};
        std::vector<PhantomCore::Visual2DNode *> faces;
        std::size_t firstRow{}, rowCount{};
    };
    // A 0..100% slider: the three volumes (Audio) and the key beam opacity (Display) share it.
    struct PercentSlider
    {
        int (*read)(const OptionValues &){};
        void (*write)(OptionValues &, int){};
        const char *textKey{};
        PhantomCore::Visual2DNode *label{}, *track{}, *fill{}, *knob{}, *value{};
        float trackWidth{}, trackHeight{};
    };
    static constexpr std::size_t VolumeSliderCount = 3, KeyBeamOpacitySlider = 3, BackgroundBrightnessSlider = 4,
                                 SliderCount = 5;

    // Drawer frame, rail and section navigation (OptionsRail).
    void CreateFrame();
    void CreateRail();
    void CreateSections();
    void LayoutSections();
    void CreateSectionHeader(std::size_t index);
    [[nodiscard]] float NextField(float controlHeight);
    // Reserves a block of inline slider rows and returns its top.
    [[nodiscard]] float NextInlineRows(std::size_t rows);
    void RefreshRail();
    void UpdateRailExpansion(double deltaSeconds);
    void ApplyRailWidth();
    void ScrollToSection(std::size_t index);
    void StepSection(int delta);
    void SpySection();
    void UpdateContentLayout();
    void UpdateScrollbar();
    void CollapseCombos();
    [[nodiscard]] std::array<PhantomCore::Visual2DNode *, 8> Combos() const noexcept;
    // An open drop-down owns the wheel: its list scrolls (or closes) and the body stays put.
    [[nodiscard]] bool AnyComboExpanded() const;
    bool CanProcessAction(PhantomCore::NodeId source) const noexcept;
    void RefreshIcons();
    // Icons come from the skin's Common/Icons; bounds are from the parent's top-left.
    PhantomCore::Visual2DNode &CreateIcon(PhantomCore::Visual2DNode &parent, PhantomCore::Rect bounds,
                                          std::string key, PhantomCore::Color tint, std::string name);
    void SetIconKey(PhantomCore::Visual2DNode &icon, std::string key);
    void ApplyIcon(PhantomCore::Visual2DNode &icon, const std::string &key);
    void BindPart(PhantomCore::Visual2DNode &node, std::string key);
    void LayoutFooter();
    [[nodiscard]] std::wstring StatusText(bool &error) const;

    // Fields.
    void CreateLanguageControls();
    void CreateSongsControls();
    void RefreshSongsControls();
    void ChooseSongsFolder();
    void CreateSkinControls();
    void CreateDisplayControls();
    void RefreshDisplayControls();
    void SelectResolution(std::size_t index);
    void SelectNotation(std::size_t index);
    void SelectRenderRate(std::size_t index);
    void CreateVolumeControls();
    void CreateKeyBeamOpacityControl();
    void CreateBackgroundBrightnessControl();
    // A captioned slider row (caption above), or with inlineTop an inline row at that top.
    void CreatePercentSlider(std::size_t index, const std::string &name, std::optional<float> inlineTop = {});
    void RefreshSliderControls();
    void ShowSlider(const PercentSlider &slider, int percent);
    bool ProcessSliderPointer(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                              PhantomCore::NodeId hoveredNode);
    void SaveSlider();
    void CreateAudioControls();
    void RefreshAudioControls();
    void CreateOffsetControls();
    void RefreshOffsetControls();
    void EditOffset(std::uint16_t key);
    // Pressing the offset field and moving left/right changes the offset by 1 ms per Canvas pixel; the
    // release saves it. A press without movement stays a click that starts keyboard editing.
    bool ProcessOffsetDrag(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                           PhantomCore::NodeId hoveredNode);
    void ApplyOffset();
    void CreateKeyControls();
    void CreateKeyOverlay(const ModeInputLayout &layout, const ModeKeyOverlay &description);
    void RefreshKeyControls();
    void RefreshKeyOverlay(KeyOverlay &overlay);
    void RefreshKeyImages();
    void ShowKeyOverlay(KeyOverlay &overlay);
    void UpdateKeyClearButtons(PhantomCore::NodeId hoveredNode);
    void CaptureKey(std::uint16_t key);
    // The restart key row heads the Input section; a play key or a fixed game control is refused.
    void CreateRestartKeyControl();
    void RefreshRestartKeyControl();
    void CaptureRestartKey(std::uint16_t key);
    [[nodiscard]] KeyOverlay *VisibleKeyOverlay() noexcept;
    [[nodiscard]] const KeyOverlay *VisibleKeyOverlay() const noexcept;
    void HideKeyOverlays() noexcept;

    void CancelKeyboardEdit();
    bool ProcessFieldAction(const PhantomCore::Action &action);
    void Close(PhantomCore::Visual2DInputRouter *inputRouter);
    void ApplyAnimationPosition();
    void ApplyScrollOffset(PhantomCore::Visual2DInputRouter &inputRouter);
    void RefreshStatus();
    void RefreshSkinSets();
    bool SaveOptions(const OptionValues &candidate);
    void SelectOutput(PhantomCore::AudioOutputBackend output);
    void SelectDriver(int index);
    void RestoreAudio(PhantomCore::AudioOutputBackend output, int driver, std::string &error);
    [[nodiscard]] std::string DriverName(int index) const;

    static constexpr float AnimationSeconds = 0.2F;
    static constexpr float RailExpandDelaySeconds = 0.3F;

    // Frame.
    PhantomCore::Visual2DNode *panel_{}, *rail_{}, *title_{}, *close_{}, *headerRule_{};
    PhantomCore::Visual2DNode *viewport_{}, *content_{};
    UiScrollbar scrollbar_;
    PhantomCore::Visual2DNode *footer_{}, *footerRule_{}, *statusBar_{}, *statusIcon_{}, *statusLabel_{};
    std::vector<std::pair<PhantomCore::Visual2DNode *, std::string>> icons_;
    std::array<Section, SectionCount> sections_{};
    std::size_t activeSection_{};
    std::optional<std::size_t> railSkinSection_;
    // A rail click keeps its section lit until the body is scrolled by hand.
    std::optional<std::size_t> heldSection_;
    float contentHeight_{}, viewportHeight_{}, footerHeight_{};
    float layoutCursor_{};
    float scrollOffset_{};
    bool railPointerInside_{}, railKeyboard_{}, railExpanded_{};
    float railHoverSeconds_{};
    // Layout changes outside ProcessInput invalidate the router's hit test on its next call.
    bool hitTestDirty_{};
    // Rail names and the close key; it runs in ProcessInput with the frame time Update saw.
    UiTooltip tooltip_;
    double frameSeconds_{};

    // Fields.
    std::vector<ModeInputLayout> inputLayouts_;
    std::vector<KeyRow> keyRows_;
    std::vector<KeyOverlay> keyOverlays_;
    std::vector<PhantomCore::Visual2DNode *> modeLabels_;
    std::vector<PhantomCore::WindowResolution> resolutions_;
    std::array<PercentSlider, SliderCount> sliders_{};
    std::optional<std::size_t> sliderDrag_;
    int sliderDraft_{}; // The dragged slider's shown percentage until the release saves it.
    PhantomCore::IWindowControl *window_{};
    PhantomCore::Visual2DNode *languageLabel_{}, *languageCombo_{};
    PhantomCore::Visual2DNode *songsLabel_{}, *songsField_{}, *songsBrowse_{}, *songsDefault_{};
    PhantomCore::Visual2DNode *skinLabel_{}, *skinCombo_{};
    PhantomCore::Visual2DNode *resolutionLabel_{}, *resolutionCombo_{};
    PhantomCore::Visual2DNode *notationLabel_{}, *notationCombo_{};
    PhantomCore::Visual2DNode *renderRateLabel_{}, *renderRateCombo_{};
    PhantomCore::Visual2DNode *middlewareLabel_{}, *outputLabel_{}, *driverLabel_{};
    PhantomCore::Visual2DNode *middlewareCombo_{}, *outputCombo_{}, *driverCombo_{};
    PhantomCore::Visual2DNode *offsetLabel_{}, *offsetField_{}, *offsetUnit_{};
    std::optional<std::size_t> captureRow_;
    PhantomCore::Visual2DNode *restartKeyLabel_{}, *restartKeyButton_{};
    bool capturingRestartKey_{};
    std::wstring offsetDraft_;
    bool editingOffset_{}, replaceOffsetDraft_{};
    struct OffsetDrag
    {
        float startX{};
        std::int32_t start{}, value{};
        bool moving{}; // Past the threshold: the press is a drag, not a click.
    };
    std::optional<OffsetDrag> offsetDrag_;

    TextCatalog &texts_;
    OptionSettings &options_;
    PhantomCore::ScreenVisual2DManager *visuals_{};
    PhantomCore::AudioSystem *audio_{};
    PhantomCore::Visual2DCanvas *canvas_{};
    enum class Status
    {
        None,
        SaveFailed,
        AudioFailed,
        WindowFailed,
        InvalidInput
    };
    Status status_{};
    std::string statusDetail_;
    PhantomCore::NodeId panelId_{};
    PhantomCore::NodeId languageComboId_{};
    PhantomCore::NodeId skinComboId_{};
    PhantomCore::NodeId outputComboId_{};
    PhantomCore::NodeId driverComboId_{};
    std::vector<int> driverIndices_;
    std::vector<std::wstring> skinNames_;
    std::optional<PhantomCore::Point> previousDragPoint_;
    float animationProgress_{};
    bool targetOpen_{};
    bool dragging_{};
    std::uint64_t optionRevision_{};
};
