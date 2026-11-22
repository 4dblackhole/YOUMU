#pragma once
#include "OptionsCategory.h"

#include <optional>
#include <string>

// A 0..100% slider shared by the categories: the three volumes (Audio, inline rows) and the key beam opacity
// and background brightness (Display, captioned rows). The track is the click and drag target; the value is
// saved once, when the drag ends.
class OptionsPercentSlider final
{
  public:
    // Which saved value the slider shows and changes, and its caption's text key.
    struct Binding
    {
        int (*read)(const OptionValues &){};
        void (*write)(OptionValues &, int){};
        const char *textKey{};
    };

    // A captioned row takes the next field; an inline row is placed at inlineTop (caption, track and value).
    void Create(OptionsContext &context, Binding binding, const std::string &name,
                std::optional<float> inlineTop = std::nullopt);
    void Refresh(const OptionsContext &context);
    // True while this slider holds the pointer: from a press on its track to the release that saves.
    [[nodiscard]] bool ProcessPointer(OptionsContext &context, const PhantomCore::InputState &input,
                                      std::optional<PhantomCore::Point> canvasPointer, PhantomCore::NodeId hoveredNode);
    // Drops a drag without saving it.
    void Stop() noexcept { draft_.reset(); }
    void Shutdown() noexcept;

  private:
    void Show(int percent);
    void Save(OptionsContext &context);

    Binding binding_{};
    PhantomCore::Visual2DNode *label_{}, *track_{}, *fill_{}, *knob_{}, *value_{};
    float trackWidth_{}, trackHeight_{};
    // The dragged value until the release saves it; empty while no drag is in progress.
    std::optional<int> draft_;
};
