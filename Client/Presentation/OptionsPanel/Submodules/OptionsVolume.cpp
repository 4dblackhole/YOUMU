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
    constexpr std::array VolumeNames{"Master", "Music", "Effects"};
} // namespace

void OptionsPanel::CreateVolumeControls()
{
    volumes_ = {VolumeSlider{&AudioVolumes::master, "Options.masterVolume"},
                VolumeSlider{&AudioVolumes::music, "Options.musicVolume"},
                VolumeSlider{&AudioVolumes::effects, "Options.effectsVolume"}};
    for (std::size_t i = 0; i < volumes_.size(); ++i)
    {
        auto &slider = volumes_[i];
        const std::string name = std::string("Options.Volume.") + VolumeNames[i];
        const float top = NextField(ControlHeight);
        const float controlTop = top + ControlOffset;
        slider.label = &CreateFieldLabel(*content_, top, name + ".Label");
        // The track is the click and drag target; fill and knob only show the value.
        slider.track = &CreatePanel(*content_, TopLeft({ContentLeft, controlTop, TrackWidth, ControlHeight}, contentHeight_),
                                    name);
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
}

void OptionsPanel::ShowVolume(const VolumeSlider &slider, const int percent)
{
    const float ratio = static_cast<float>(std::clamp(percent, 0, AudioVolumes::Maximum)) / AudioVolumes::Maximum;
    const float knobLeft = (TrackWidth - KnobWidth) * ratio;
    slider.fill->SetVisible(ratio > 0.0F);
    slider.fill->SetBounds(TopLeft({0, (ControlHeight - RailHeight) * 0.5F, knobLeft + KnobWidth * 0.5F, RailHeight},
                                   ControlHeight));
    slider.knob->SetBounds(TopLeft({knobLeft, (ControlHeight - KnobHeight) * 0.5F, KnobWidth, KnobHeight}, ControlHeight));
    RequireComponent<TextVisualComponent>(*slider.value).SetText(std::to_wstring(percent) + L"%");
}

void OptionsPanel::RefreshVolumeControls()
{
    const auto &saved = options_.Values().volumes;
    for (std::size_t i = 0; i < volumes_.size(); ++i)
    {
        const auto &slider = volumes_[i];
        SetCaption(*slider.label, std::wstring(texts_.Get(slider.textKey)));
        // A drag in progress shows its draft until the release saves it.
        const auto &shown = volumeDrag_ == i ? volumeDraft_ : saved;
        ShowVolume(slider, shown.*slider.percent);
    }
}

bool OptionsPanel::ProcessVolumePointer(const InputState &input, const std::optional<Point> canvasPointer,
                                        const NodeId hoveredNode)
{
    if (!volumeDrag_)
    {
        if (!targetOpen_ || !input.WasMouseButtonPressed(MouseButton::Left))
            return false;
        const auto pressed = std::ranges::find_if(volumes_, [hoveredNode](const VolumeSlider &slider) {
            return slider.track != nullptr && slider.track->Id() == hoveredNode;
        });
        if (pressed == volumes_.end())
            return false;
        CollapseCombos();
        volumeDrag_ = static_cast<std::size_t>(pressed - volumes_.begin());
        volumeDraft_ = options_.Values().volumes;
    }
    const auto &slider = volumes_[*volumeDrag_];
    if (canvasPointer)
    {
        const auto track = slider.track->BoundsInCanvas();
        const float travel = track.width - KnobWidth;
        const float ratio = std::clamp((canvasPointer->x - track.x - KnobWidth * 0.5F) / travel, 0.0F, 1.0F);
        volumeDraft_.*slider.percent = static_cast<int>(std::lround(ratio * AudioVolumes::Maximum));
        ShowVolume(slider, volumeDraft_.*slider.percent);
    }
    // One save per drag: Option.ini is written when the button is released.
    if (input.WasMouseButtonReleased(MouseButton::Left) || !input.IsMouseButtonDown(MouseButton::Left))
        SaveVolume();
    return true;
}

void OptionsPanel::SaveVolume()
{
    volumeDrag_.reset();
    if (volumeDraft_ != options_.Values().volumes)
    {
        auto candidate = options_.Values();
        candidate.volumes = volumeDraft_;
        SaveOptions(candidate);
    }
    RefreshVolumeControls();
    RefreshStatus();
}
