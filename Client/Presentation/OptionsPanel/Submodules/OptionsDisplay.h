#pragma once
#include "OptionsCategory.h"
#include "OptionsPercentSlider.h"

#include <array>

// Display: skin, resolution, window mode, screen position, song text notation, frame limit, key beam opacity
// and background brightness. Window options are applied to the window first and saved after; a failed save
// puts the window back.
class OptionsDisplay final : public OptionsCategory
{
  public:
    using OptionsCategory::OptionsCategory;

    [[nodiscard]] std::vector<float> FieldHeights() const override;
    void Create() override;
    void Refresh() override;
    void Shutdown() noexcept override;
    [[nodiscard]] bool ProcessAction(const PhantomCore::Action &action) override;
    [[nodiscard]] OptionsPointer ProcessPointer(const PhantomCore::InputState &input,
                                                std::optional<PhantomCore::Point> canvasPointer,
                                                PhantomCore::NodeId hoveredNode) override;
    [[nodiscard]] bool OwnsKeyboard() const noexcept override { return positionEdit_.has_value(); }
    void HandleKey(std::uint16_t virtualKey) override;
    void CancelKeyboardEdit() override;
    void AppendCombos(std::vector<PhantomCore::Visual2DNode *> &combos) const override;
    void OnToggle() override;
    void OnOpened() override;
    void OnClosed() override;

  private:
    static constexpr std::size_t SliderCount = 2; // Key beam opacity, background brightness.

    void CreateSkinControls();
    void RefreshSkinSets();
    void RefreshControls();
    bool SelectSkin(std::size_t index);
    void SelectResolution(std::size_t index);
    void SelectWindowMode(std::size_t index);
    void SelectNotation(std::size_t index);
    void SelectRenderRate(std::size_t index);

    // Screen position (OptionsDisplayPosition): a box with the monitor and the game picture inside it, X and Y
    // number fields and a Center button. Dragging the picture or entering a number moves the window when the
    // drag ends or Enter is pressed, and saves the position. Only a Borderless picture smaller than its monitor
    // moves.
    struct PositionView
    {
        PhantomCore::WindowResolution monitor, size;
        PhantomCore::WindowPosition position;
        bool adjustable{};
    };
    struct PositionDrag
    {
        PhantomCore::Point start;
        PhantomCore::WindowPosition from, value;
    };
    void CreatePositionControls();
    void RefreshPositionControls();
    [[nodiscard]] PositionView CurrentPosition() const;
    void ShowPosition(const PositionView &view, PhantomCore::WindowPosition position);
    bool ProcessPositionDrag(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                             PhantomCore::NodeId hoveredNode);
    bool ProcessPositionAction(const PhantomCore::Action &action);
    void StartPositionEdit(std::size_t axis);
    void EditPosition(std::uint16_t key);
    void ApplyPosition(std::optional<PhantomCore::WindowPosition> position);

    PhantomCore::Visual2DNode *skinLabel_{}, *skinCombo_{};
    std::vector<std::wstring> skinNames_;
    PhantomCore::Visual2DNode *resolutionLabel_{}, *resolutionCombo_{};
    std::vector<PhantomCore::WindowResolution> resolutions_;
    PhantomCore::Visual2DNode *windowModeLabel_{}, *windowModeCombo_{};
    PhantomCore::Visual2DNode *notationLabel_{}, *notationCombo_{};
    PhantomCore::Visual2DNode *renderRateLabel_{}, *renderRateCombo_{};
    std::array<OptionsPercentSlider, SliderCount> sliders_{};

    PhantomCore::Visual2DNode *positionLabel_{}, *positionBox_{}, *positionMonitor_{}, *positionPicture_{};
    PhantomCore::Visual2DNode *positionCenter_{};
    std::array<PhantomCore::Visual2DNode *, 2> positionFields_{}; // X, Y.
    std::optional<PositionDrag> positionDrag_;
    std::optional<std::size_t> positionEdit_; // The axis whose number is being typed.
    std::wstring positionDraft_;
    bool replacePositionDraft_{};
    float positionScale_{1}; // Box units per monitor pixel.
};
