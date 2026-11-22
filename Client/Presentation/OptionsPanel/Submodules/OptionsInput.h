#pragma once
#include "Mode/ModeInputLayout.h"
#include "OptionsCategory.h"

struct ModeKeyOverlay;

// Input: the restart key, then each mode's keys. A mode registered with a key overlay edits its keys in one
// centred modal opened from its card; another mode edits each binding slot on its own row. A key is captured
// on the next key press and saved at once.
class OptionsInput final : public OptionsCategory
{
  public:
    using OptionsCategory::OptionsCategory;

    void Load() override;
    [[nodiscard]] std::vector<float> FieldHeights() const override;
    void Create() override;
    void Refresh() override;
    void Shutdown() noexcept override;
    [[nodiscard]] bool ProcessAction(const PhantomCore::Action &action) override;
    [[nodiscard]] bool OwnsKeyboard() const noexcept override
    {
        return captureRow_.has_value() || capturingRestartKey_;
    }
    void HandleKey(std::uint16_t virtualKey) override;
    void CancelKeyboardEdit() override;
    void OnToggle() override;
    void OnSkinChanged() override;
    void OnStatusChanged() override;
    [[nodiscard]] const PhantomCore::Visual2DNode *VisibleModal() const noexcept override;
    void EscapeModal() override;
    void UpdateModal(PhantomCore::NodeId hoveredNode) override;

  private:
    struct KeyRow
    {
        std::string mode, binding, displayName;
        std::size_t slot{};
        PhantomCore::Visual2DNode *label{}, *button{};
        PhantomCore::Visual2DNode *clear{};
        // Whether the slot holds a key, as last drawn; the clear button needs one.
        bool assigned{};
    };
    // The key rows of a mode with an overlay are children of its modal.
    struct KeyOverlay
    {
        const ModeKeyOverlay *description{};
        std::string mode;
        PhantomCore::Visual2DNode *scrim{}, *panel{}, *close{}, *status{}, *statusIcon{}, *title{};
        PhantomCore::Visual2DNode *open{}, *openTitle{}, *openSummary{};
        std::vector<PhantomCore::Visual2DNode *> faces;
        std::size_t firstRow{}, rowCount{};
    };

    void CreateRestartKeyControl();
    void CreateKeyOverlay(const ModeInputLayout &layout, const ModeKeyOverlay &description);
    void RefreshRestartKeyControl();
    void RefreshKeyControls();
    void RefreshKeyOverlay(KeyOverlay &overlay);
    void RefreshKeyImages();
    void ShowKeyOverlay(KeyOverlay &overlay);
    void HideKeyOverlays() noexcept;
    [[nodiscard]] KeyOverlay *VisibleKeyOverlay() noexcept;
    [[nodiscard]] const KeyOverlay *VisibleKeyOverlay() const noexcept;
    bool ProcessOverlayAction(KeyOverlay &overlay, const PhantomCore::Action &action);
    void StartCapture(std::size_t row);
    void CaptureKey(std::uint16_t key);
    // The restart key may be neither a play key of any mode nor a fixed game control.
    void CaptureRestartKey(std::uint16_t key);

    std::vector<ModeInputLayout> inputLayouts_;
    std::vector<KeyRow> keyRows_;
    std::vector<KeyOverlay> keyOverlays_;
    std::vector<PhantomCore::Visual2DNode *> modeLabels_;
    std::optional<std::size_t> captureRow_;
    PhantomCore::Visual2DNode *restartKeyLabel_{}, *restartKeyButton_{};
    bool capturingRestartKey_{};
};
