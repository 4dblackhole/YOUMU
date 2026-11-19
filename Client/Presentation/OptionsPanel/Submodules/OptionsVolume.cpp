#include "OptionsPanelStyle.h"
#include "Presentation/OptionsPanel.h"
#include <algorithm>
#include <cmath>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    // Slider row: a 256px track (6px rail, cobalt fill, navy knob) and the percentage on the right.
    constexpr float TrackWidth = 256, RailHeight = 6, KnobWidth = 10, KnobHeight = 22;
    constexpr float ValueLeft = ContentLeft + TrackWidth + 8, ValueWidth = FieldWidth - TrackWidth - 8;
    constexpr int PercentMaximum = 100;
    constexpr std::array VolumeNames{"Master", "Music", "Effects"};
} // namespace

void OptionsPanel::CreateVolumeControls()
{
    sliders_[0] = {[](const OptionValues &values) { return values.volumes.master; },
                   [](OptionValues &values, const int percent) { values.volumes.master = percent; },
                   "Options.masterVolume"};
    sliders_[1] = {[](const OptionValues &values) { return values.volumes.music; },
                   [](OptionValues &values, const int percent) { values.volumes.music = percent; },
                   "Options.musicVolume"};
    sliders_[2] = {[](const OptionValues &values) { return values.volumes.effects; },
                   [](OptionValues &values, const int percent) { values.volumes.effects = percent; },
                   "Options.effectsVolume"};
    for (std::size_t i = 0; i < VolumeSliderCount; ++i)
        CreatePercentSlider(i, std::string("Options.Volume.") + VolumeNames[i]);
}

void OptionsPanel::CreateKeyBeamOpacityControl()
{
    sliders_[KeyBeamOpacitySlider] = {
        [](const OptionValues &values) { return values.keyBeamOpacity; },
        [](OptionValues &values, const int percent) { values.keyBeamOpacity = percent; }, "Options.keyBeamOpacity"};
    CreatePercentSlider(KeyBeamOpacitySlider, "Options.KeyBeamOpacity");
}

void OptionsPanel::CreatePercentSlider(const std::size_t index, const std::string &name)
{
    auto &slider = sliders_[index];
    const float top = NextField(ControlHeight);
    const float controlTop = top + ControlOffset;
    slider.label = &CreateFieldLabel(*content_, top, name + ".Label");
    // The track is the click and drag target; fill and knob only show the value.
    slider.track =
        &CreatePanel(*content_, TopLeft({ContentLeft, controlTop, TrackWidth, ControlHeight}, contentHeight_), name);
    SetPanelColor(*slider.track, {1, 1, 1, 0});
    slider.track->AddComponent<RectangleCollider2DComponent>();
    slider.track->AddComponent<PointerReceiverComponent>();
    auto &rail = CreateSprite(*slider.track,
                              TopLeft({0, (ControlHeight - RailHeight) * 0.5F, TrackWidth, RailHeight}, ControlHeight),
                              {}, "Rail");
    SetPanelColor(rail, FieldLineColor);
    slider.fill = &CreateSprite(*slider.track, {}, {}, "Fill");
    SetPanelColor(*slider.fill, ui_colors::theme::Brand);
    slider.knob = &CreateSprite(*slider.track, {}, {}, "Knob");
    SetPanelColor(*slider.knob, ui_colors::theme::BrandDeep);
    auto &value = PhantomCore::CreateLabel(
        *content_, TopLeft({ValueLeft, controlTop, ValueWidth, ControlHeight}, contentHeight_), L"", name + ".Value");
    StyleNumber(RequireComponent<TextVisualComponent>(value), 15, 700, HeadingColor, TextAlignment::Trailing, false);
    slider.value = &value;
}

void OptionsPanel::ShowSlider(const PercentSlider &slider, const int percent)
{
    const float ratio = static_cast<float>(std::clamp(percent, 0, PercentMaximum)) / PercentMaximum;
    const float knobLeft = (TrackWidth - KnobWidth) * ratio;
    slider.fill->SetVisible(ratio > 0.0F);
    slider.fill->SetBounds(TopLeft({0, (ControlHeight - RailHeight) * 0.5F, knobLeft + KnobWidth * 0.5F, RailHeight},
                                   ControlHeight));
    slider.knob->SetBounds(TopLeft({knobLeft, (ControlHeight - KnobHeight) * 0.5F, KnobWidth, KnobHeight}, ControlHeight));
    RequireComponent<TextVisualComponent>(*slider.value).SetText(std::to_wstring(percent) + L"%");
}

void OptionsPanel::RefreshSliderControls()
{
    const auto &saved = options_.Values();
    for (std::size_t i = 0; i < sliders_.size(); ++i)
    {
        const auto &slider = sliders_[i];
        if (slider.track == nullptr)
            continue;
        SetCaption(*slider.label, std::wstring(texts_.Get(slider.textKey)));
        // A drag in progress shows its draft until the release saves it.
        ShowSlider(slider, sliderDrag_ == i ? sliderDraft_ : slider.read(saved));
    }
}

bool OptionsPanel::ProcessSliderPointer(const InputState &input, const std::optional<Point> canvasPointer,
                                        const NodeId hoveredNode)
{
    if (!sliderDrag_)
    {
        if (!targetOpen_ || !input.WasMouseButtonPressed(MouseButton::Left))
            return false;
        const auto pressed = std::ranges::find_if(sliders_, [hoveredNode](const PercentSlider &slider) {
            return slider.track != nullptr && slider.track->Id() == hoveredNode;
        });
        if (pressed == sliders_.end())
            return false;
        CollapseCombos();
        sliderDrag_ = static_cast<std::size_t>(pressed - sliders_.begin());
        sliderDraft_ = pressed->read(options_.Values());
    }
    const auto &slider = sliders_[*sliderDrag_];
    if (canvasPointer)
    {
        const auto track = slider.track->BoundsInCanvas();
        const float travel = track.width - KnobWidth;
        const float ratio = std::clamp((canvasPointer->x - track.x - KnobWidth * 0.5F) / travel, 0.0F, 1.0F);
        sliderDraft_ = static_cast<int>(std::lround(ratio * PercentMaximum));
        ShowSlider(slider, sliderDraft_);
    }
    // One save per drag: Option.ini is written when the button is released.
    if (input.WasMouseButtonReleased(MouseButton::Left) || !input.IsMouseButtonDown(MouseButton::Left))
        SaveSlider();
    return true;
}

void OptionsPanel::SaveSlider()
{
    const auto &slider = sliders_[*sliderDrag_];
    sliderDrag_.reset();
    if (sliderDraft_ != slider.read(options_.Values()))
    {
        auto candidate = options_.Values();
        slider.write(candidate, sliderDraft_);
        SaveOptions(candidate);
    }
    RefreshSliderControls();
    RefreshStatus();
}
