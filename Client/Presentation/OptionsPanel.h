#pragma once

#include "App/OptionSettings.h"
#include "App/SkinSetSelection.h"
#include "PhantomCore.h"
#include "Mode/ModeInputLayout.h"
#include "Texts/TextCatalog.h"

#include <optional>
#include <array>
#include <vector>

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
        return captureRow_.has_value() || editingOffset_;
    }
    [[nodiscard]] bool IsVisible() const noexcept;

  private:
    void CreateDisplayControls();
    void CreateTabs();
    void SelectTab(std::size_t index);
    void RefreshTabs();
    void UpdateContentLayout();
    void CollapseCombos();
    bool CanProcessAction(PhantomCore::NodeId source) const noexcept;
    void RefreshDisplayControls();
    void SelectResolution(std::size_t index);
    void CreateKeyControls();
    void RefreshKeyControls();
    void RefreshKeyImages();
    void CaptureKey(std::uint16_t key);
    void CreateOffsetControls();
    void CreateSongsControls();
    void RefreshSongsControls();
    void ChooseSongsFolder();
    void RefreshOffsetControls();
    void EditOffset(std::uint16_t key);
    void ApplyOffset();
    void CancelKeyboardEdit();
    bool ProcessFieldAction(const PhantomCore::Action &action);
    void ApplyAnimationPosition();
    void ApplyScrollOffset(PhantomCore::Visual2DInputRouter &inputRouter);
    void CreateLanguageControls();
    void CreateSkinControls();
    void CreateAudioControls();
    void RefreshAudioControls();
    void RefreshStatus();
    void RefreshSkinSets();
    bool SaveOptions(const OptionValues &candidate);
    void SelectOutput(PhantomCore::AudioOutputBackend output);
    void SelectDriver(int index);
    void RestoreAudio(PhantomCore::AudioOutputBackend output, int driver, std::string &error);
    [[nodiscard]] std::string DriverName(int index) const;

    static constexpr float PanelWidth = 320.0F;
    static constexpr float PanelHeight = 720.0F;
    static constexpr float AnimationSeconds = 0.2F;
    static constexpr float ContentViewportHeight = 568.0F;
    static constexpr std::size_t TabCount = 4;
    std::array<PhantomCore::Visual2DNode *, TabCount> tabButtons_{}, tabContents_{};
    std::array<float, TabCount> contentHeights_{}, scrollOffsets_{};
    std::size_t activeTab_{};
    PhantomCore::Visual2DNode *viewport_{};
    float viewportHeight_{ContentViewportHeight};
    float contentHeight_{};
    struct KeyRow
    {
        std::string mode, binding, displayName;
        std::size_t slot{};
        PhantomCore::Visual2DNode *label{}, *button{};
        PhantomCore::Visual2DNode *clear{};
    };
    std::vector<ModeInputLayout> inputLayouts_;
    std::vector<KeyRow> keyRows_;
    std::vector<PhantomCore::Visual2DNode *> modeLabels_;
    std::vector<PhantomCore::WindowResolution> resolutions_;
    PhantomCore::IWindowControl *window_{};
    PhantomCore::Visual2DNode *resolutionLabel_{}, *resolutionCombo_{};
    PhantomCore::Visual2DNode *offsetLabel_{}, *offsetField_{}, *offsetHint_{};
    PhantomCore::Visual2DNode *songsLabel_{}, *songsField_{}, *songsDefault_{};
    std::optional<std::size_t> captureRow_;
    std::wstring offsetDraft_;
    bool editingOffset_{}, replaceOffsetDraft_{};

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
    PhantomCore::Visual2DNode *panel_{};
    PhantomCore::Visual2DNode *keyPanel_{}, *openKeyPanel_{}, *closeKeyPanel_{}, *keyPanelStatus_{};
    PhantomCore::Visual2DNode *keyPanelImage_{};
    PhantomCore::Visual2DNode *keyPanelTitle_{};
    std::array<PhantomCore::Visual2DNode *, 4> keyFaceImages_{};
    PhantomCore::Visual2DNode *title_{};
    PhantomCore::Visual2DNode *languageLabel_{};
    PhantomCore::Visual2DNode *skinLabel_{};
    PhantomCore::Visual2DNode *statusLabel_{};
    PhantomCore::Visual2DNode *middlewareLabel_{};
    PhantomCore::Visual2DNode *outputLabel_{};
    PhantomCore::Visual2DNode *driverLabel_{};
    PhantomCore::Visual2DNode *content_{};
    PhantomCore::Visual2DNode *languageCombo_{};
    PhantomCore::Visual2DNode *skinCombo_{};
    PhantomCore::Visual2DNode *middlewareCombo_{};
    PhantomCore::Visual2DNode *outputCombo_{};
    PhantomCore::Visual2DNode *driverCombo_{};
    PhantomCore::NodeId panelId_{};
    PhantomCore::NodeId languageComboId_{};
    PhantomCore::NodeId skinComboId_{};
    PhantomCore::NodeId outputComboId_{};
    PhantomCore::NodeId driverComboId_{};
    std::vector<int> driverIndices_;
    std::vector<std::wstring> skinNames_;
    std::optional<PhantomCore::Point> previousDragPoint_;
    float animationProgress_{};
    float scrollOffset_{};
    bool targetOpen_{};
    bool dragging_{};
    std::uint64_t optionRevision_{};
};
