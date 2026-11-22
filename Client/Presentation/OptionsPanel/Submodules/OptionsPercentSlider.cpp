#include "OptionsPercentSlider.h"
#include "OptionsPanelStyle.h"
#include <algorithm>
#include <cmath>

using namespace PhantomCore;

using namespace options_ui;
namespace
{
    // Captioned slider: the caption above, a 256px track (6px rail, cobalt fill, navy knob) and the percentage
    // on the right. Inline slider (volumes): caption, a shorter track and the percentage on one row.
    constexpr float TrackWidth = 256, RailHeight = 6, KnobWidth = 10, KnobHeight = 22;
    constexpr float ValueGap = 8, InlineCaptionWidth = 104, InlineValueWidth = 48;
    constexpr float InlineTrackWidth = FieldWidth - InlineCaptionWidth - ValueGap - InlineValueWidth;
    constexpr int PercentMaximum = 100;
} // namespace

void OptionsPercentSlider::Create(OptionsContext &context, const Binding binding, const std::string &name,
                                  const std::optional<float> inlineTop)
{
    binding_ = binding;
    auto &content = context.Content();
    const float contentHeight = context.ContentHeight();
    float controlTop = 0, trackLeft = ContentLeft;
    if (inlineTop)
    {
        controlTop = *inlineTop;
        trackWidth_ = InlineTrackWidth;
        trackHeight_ = InlineRowHeight;
        trackLeft = ContentLeft + InlineCaptionWidth;
        label_ = &CreateCaption(content, {ContentLeft, controlTop, InlineCaptionWidth, InlineRowHeight}, 13, 600,
                                LabelColor, name + ".Label");
    }
    else
    {
        const float top = context.NextField(ControlHeight);
        controlTop = top + ControlOffset;
        trackWidth_ = TrackWidth;
        trackHeight_ = ControlHeight;
        label_ = &CreateFieldLabel(content, top, name + ".Label");
    }
    const float valueLeft = trackLeft + trackWidth_ + ValueGap;
    const float valueWidth = ContentLeft + FieldWidth - valueLeft;
    track_ = &CreatePanel(content, TopLeft({trackLeft, controlTop, trackWidth_, trackHeight_}, contentHeight), name);
    SetPanelColor(*track_, {1, 1, 1, 0});
    track_->AddComponent<RectangleCollider2DComponent>();
    track_->AddComponent<PointerReceiverComponent>();
    auto &rail = CreateSprite(*track_, TopLeft({0, (trackHeight_ - RailHeight) * 0.5F, trackWidth_, RailHeight}, trackHeight_),
                              {}, "Rail");
    SetPanelColor(rail, FieldLineColor);
    fill_ = &CreateSprite(*track_, {}, {}, "Fill");
    SetPanelColor(*fill_, ui_colors::theme::Brand);
    knob_ = &CreateSprite(*track_, {}, {}, "Knob");
    SetPanelColor(*knob_, ui_colors::theme::BrandDeep);
    value_ = &PhantomCore::CreateLabel(content, TopLeft({valueLeft, controlTop, valueWidth, trackHeight_}, contentHeight),
                                       L"", name + ".Value");
    StyleNumber(RequireComponent<TextVisualComponent>(*value_), 15, 700, HeadingColor, TextAlignment::Trailing, false);
}

void OptionsPercentSlider::Show(const int percent)
{
    const float ratio = static_cast<float>(std::clamp(percent, 0, PercentMaximum)) / PercentMaximum;
    const float knobLeft = (trackWidth_ - KnobWidth) * ratio;
    fill_->SetVisible(ratio > 0.0F);
    fill_->SetBounds(TopLeft({0, (trackHeight_ - RailHeight) * 0.5F, knobLeft + KnobWidth * 0.5F, RailHeight}, trackHeight_));
    knob_->SetBounds(TopLeft({knobLeft, (trackHeight_ - KnobHeight) * 0.5F, KnobWidth, KnobHeight}, trackHeight_));
    RequireComponent<TextVisualComponent>(*value_).SetText(std::to_wstring(percent) + L"%");
}

void OptionsPercentSlider::Refresh(const OptionsContext &context)
{
    if (track_ == nullptr)
        return;
    SetCaption(*label_, std::wstring(context.Texts().Get(binding_.textKey)));
    // A drag in progress shows its draft until the release saves it.
    Show(draft_ ? *draft_ : binding_.read(context.Settings().Values()));
}

bool OptionsPercentSlider::ProcessPointer(OptionsContext &context, const InputState &input,
                                          const std::optional<Point> canvasPointer, const NodeId hoveredNode)
{
    if (!draft_)
    {
        if (!context.IsOpen() || !input.WasMouseButtonPressed(MouseButton::Left))
            return false;
        if (track_ == nullptr || track_->Id() != hoveredNode)
            return false;
        context.CollapseCombos();
        draft_ = binding_.read(context.Settings().Values());
    }
    if (canvasPointer)
    {
        const auto track = track_->BoundsInCanvas();
        const float travel = track.width - KnobWidth;
        const float ratio = std::clamp((canvasPointer->x - track.x - KnobWidth * 0.5F) / travel, 0.0F, 1.0F);
        draft_ = static_cast<int>(std::lround(ratio * PercentMaximum));
        Show(*draft_);
    }
    // One save per drag: Option.ini is written when the button is released.
    if (input.WasMouseButtonReleased(MouseButton::Left) || !input.IsMouseButtonDown(MouseButton::Left))
        Save(context);
    return true;
}

void OptionsPercentSlider::Save(OptionsContext &context)
{
    const int value = *draft_;
    draft_.reset();
    if (value != binding_.read(context.Settings().Values()))
    {
        auto candidate = context.Settings().Values();
        binding_.write(candidate, value);
        context.Save(candidate);
    }
    Refresh(context);
    context.RefreshStatus();
}

void OptionsPercentSlider::Shutdown() noexcept
{
    label_ = track_ = fill_ = knob_ = value_ = nullptr;
    trackWidth_ = trackHeight_ = 0.0F;
    draft_.reset();
}
