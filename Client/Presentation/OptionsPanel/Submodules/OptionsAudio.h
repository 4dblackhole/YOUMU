#pragma once
#include "OptionsCategory.h"
#include "OptionsPercentSlider.h"

#include <array>

// Audio: the master, music and effects volumes, the middleware, output, driver and mixer buffer, and the
// global offset (typed or dragged). Output and driver changes are applied to the audio system first and put
// back when the save fails; the buffer is read by the engine at the next start.
class OptionsAudio final : public OptionsCategory
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
    [[nodiscard]] bool OwnsKeyboard() const noexcept override { return editingOffset_; }
    void HandleKey(std::uint16_t virtualKey) override;
    void CancelKeyboardEdit() override;
    void AppendCombos(std::vector<PhantomCore::Visual2DNode *> &combos) const override;
    void OnToggle() override;
    void OnClosed() override;
    [[nodiscard]] std::wstring Notice() const override;

  private:
    static constexpr std::size_t VolumeSliderCount = 3;

    void CreateVolumeControls();
    void CreateDeviceControls();
    void CreateOffsetControls();
    void RefreshDeviceControls();
    void RefreshOffsetControls();
    void StopDrags() noexcept;
    bool SelectDevice(const PhantomCore::Action &action);
    void SelectOutput(PhantomCore::AudioOutputBackend output);
    void SelectDriver(int index);
    // Saves the mixer buffer length; the engine reads it at the next start.
    void SelectBuffer(std::size_t index);
    // The saved buffer differs from the running mixer's, so a restart is still needed.
    [[nodiscard]] bool BufferAwaitsRestart() const noexcept;
    void RestoreAudio(PhantomCore::AudioOutputBackend output, int driver, std::string &error);
    [[nodiscard]] std::string DriverName(int index) const;
    void StartOffsetEdit();
    void EditOffset(std::uint16_t key);
    void ApplyOffset();
    // Pressing the offset field and moving left/right changes the offset by 1 ms per Canvas pixel; the
    // release saves it. A press without movement stays a click that starts keyboard editing.
    bool ProcessOffsetDrag(const PhantomCore::InputState &input, std::optional<PhantomCore::Point> canvasPointer,
                           PhantomCore::NodeId hoveredNode);

    std::array<OptionsPercentSlider, VolumeSliderCount> volumes_{};
    PhantomCore::Visual2DNode *middlewareLabel_{}, *outputLabel_{}, *driverLabel_{}, *bufferLabel_{};
    PhantomCore::Visual2DNode *middlewareCombo_{}, *outputCombo_{}, *driverCombo_{}, *bufferCombo_{};
    std::vector<int> driverIndices_;
    PhantomCore::Visual2DNode *offsetLabel_{}, *offsetField_{}, *offsetUnit_{};
    std::wstring offsetDraft_;
    bool editingOffset_{}, replaceOffsetDraft_{};
    struct OffsetDrag
    {
        float startX{};
        std::int32_t start{}, value{};
        bool moving{}; // Past the threshold: the press is a drag, not a click.
    };
    std::optional<OffsetDrag> offsetDrag_;
};
