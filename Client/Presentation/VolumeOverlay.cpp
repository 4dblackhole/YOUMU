#include "VolumeOverlay.h"
#include "Presentation/SceneWheel.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"

#include <stdexcept>
#include <string>

using namespace PhantomCore;

namespace
{
    // A navy plate (like the F7 band) above the band's corner: three meters side by side, each a caption, a
    // vertical track with its fill and the percentage. Coordinates are from the plate's bottom-left, Y up.
    constexpr std::uint32_t CanvasOrder = 1001;
    constexpr float Margin = 16, BottomMargin = 40, Padding = 12, ColumnWidth = 64, PlateHeight = 150;
    constexpr float TrackWidth = 8, TrackHeight = 80, TrackBottom = 34, LabelHeight = 18, ValueHeight = 22;
    constexpr float PlateWidth = Padding * 2 + ColumnWidth * 3;
    constexpr auto PlateColour = ui_colors::Rgb(0x142F5E, .86F);
    constexpr Color TrackColour{1, 1, 1, .18F}, CaptionColour{1, 1, 1, .62F};
    // Captions are English, like the options' item names.
    constexpr std::array<const wchar_t *, 3> Captions{L"Master", L"Music", L"Effects"};

    void Paint(Visual2DNode &node, const Color colour)
    {
        auto *sprite = node.GetComponent<SpriteVisualComponent>();
        if (sprite == nullptr)
            throw std::logic_error("A volume overlay part is missing its sprite.");
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = style.disabled = colour;
        sprite->SetStyle(style);
    }
    TextVisualComponent &Text(Visual2DNode &node)
    {
        auto *text = node.GetComponent<TextVisualComponent>();
        if (text == nullptr)
            throw std::logic_error("A volume overlay label is missing its text.");
        return *text;
    }
    int Percent(const AudioVolumes &volumes, const VolumeTarget target)
    {
        if (target == VolumeTarget::Music)
            return volumes.music;
        return target == VolumeTarget::Effects ? volumes.effects : volumes.master;
    }
} // namespace

void VolumeOverlay::Initialize(ScreenVisual2DManager &visuals, OptionSettings &options, IWindowControl *window)
{
    options_ = &options;
    window_ = window;
    canvas_ = visuals.CreateOwnedCanvas({{1280, 720}, CanvasScaleMode::FixedHeight, {}, CanvasOrder});
    auto *canvas = canvas_.Get();
    if (canvas == nullptr)
        throw std::runtime_error("Failed to create the volume overlay canvas.");
    panel_ = &CreateSprite(canvas->AnchorNode(Anchor::BottomRight),
                           {-Margin - PlateWidth, BottomMargin, PlateWidth, PlateHeight}, {}, "Volume.Overlay");
    Paint(*panel_, PlateColour);
    const auto numberFace = ui_fonts::Number(800);
    for (std::size_t i = 0; i < meters_.size(); ++i)
    {
        auto &meter = meters_[i];
        const std::string name = std::string("Volume.") + (i == 0 ? "Master" : i == 1 ? "Music" : "Effects");
        meter.column = &panel_->CreateChild(name);
        meter.column->SetBounds({Padding + ColumnWidth * static_cast<float>(i), 0, ColumnWidth, PlateHeight});
        meter.label = &CreateLabel(*meter.column, {0, PlateHeight - 8 - LabelHeight, ColumnWidth, LabelHeight}, L"",
                                   "Label");
        auto &label = Text(*meter.label);
        label.SetFontSize(11);
        label.SetFontWeight(700);
        label.SetHorizontalAlignment(TextAlignment::Center);
        meter.track = &CreateSprite(*meter.column, {(ColumnWidth - TrackWidth) * 0.5F, TrackBottom, TrackWidth, TrackHeight},
                                    {}, "Track");
        Paint(*meter.track, TrackColour);
        meter.fill = &CreateSprite(*meter.column, {}, {}, "Fill");
        meter.value = &CreateLabel(*meter.column, {0, 8, ColumnWidth, ValueHeight}, L"", "Value");
        auto &value = Text(*meter.value);
        value.SetFont(numberFace.font);
        value.SetFontSize(16);
        value.SetFontWeight(numberFace.weight);
        value.SetTextColor(ui_colors::White);
        value.SetHorizontalAlignment(TextAlignment::Center);
    }
    static_cast<void>(canvas_.SetVisible(false));
}

void VolumeOverlay::Shutdown() noexcept
{
    meters_ = {};
    panel_ = nullptr;
    options_ = nullptr;
    window_ = nullptr;
    visibleSeconds_ = 0;
    canvas_.Reset();
}

std::optional<Point> VolumeOverlay::CanvasPointer(const InputState &input) const
{
    const auto *canvas = canvas_.Get();
    if (canvas == nullptr || window_ == nullptr || !input.IsMouseInsideWindow())
        return std::nullopt;
    const auto size = window_->ClientSize();
    return MapScreenPointer(
        {static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
        {static_cast<float>(size.width), static_cast<float>(size.height)}, *canvas);
}

VolumeTarget VolumeOverlay::TargetAt(const std::optional<Point> pointer) const
{
    // Only a shown meter can be pointed at; otherwise the master volume changes, as in osu!.
    if (!pointer || !IsVisible())
        return VolumeTarget::Master;
    for (std::size_t i = 0; i < meters_.size(); ++i)
        if (meters_[i].column != nullptr && meters_[i].column->BoundsInCanvas().Contains(*pointer))
            return static_cast<VolumeTarget>(i);
    return VolumeTarget::Master;
}

void VolumeOverlay::Update(const InputState &input, const double deltaSeconds)
{
    if (panel_ == nullptr)
        return;
    const float wheel = input.MouseWheelDelta();
    if (IsAltDown(input) && wheel != 0.0F)
    {
        const auto target = TargetAt(CanvasPointer(input));
        auto candidate = options_->Values();
        candidate.volumes = StepVolume(candidate.volumes, target, wheel);
        std::string error;
        // A failed save keeps the saved volumes; the meters show what is in effect.
        if (candidate.volumes != options_->Values().volumes)
            static_cast<void>(options_->Save(candidate, error));
        visibleSeconds_ = ShowSeconds;
        Refresh(target);
        static_cast<void>(canvas_.SetVisible(true));
        return;
    }
    if (visibleSeconds_ <= 0)
        return;
    // Held Alt over the meters keeps them up, so the next turn can pick one.
    const auto pointer = CanvasPointer(input);
    const bool pointed = IsAltDown(input) && pointer && panel_->BoundsInCanvas().Contains(*pointer);
    if (pointed)
        visibleSeconds_ = ShowSeconds;
    else
        visibleSeconds_ -= std::max(deltaSeconds, 0.0);
    Refresh(TargetAt(pointer));
    if (visibleSeconds_ <= 0)
        static_cast<void>(canvas_.SetVisible(false));
}

void VolumeOverlay::Refresh(const VolumeTarget target)
{
    const auto &volumes = options_->Values().volumes;
    for (std::size_t i = 0; i < meters_.size(); ++i)
    {
        auto &meter = meters_[i];
        const bool active = static_cast<VolumeTarget>(i) == target;
        const auto percent = Percent(volumes, static_cast<VolumeTarget>(i));
        auto &label = Text(*meter.label);
        const std::wstring caption(Captions[i]);
        if (label.Text() != caption)
        {
            label.SetText(caption);
            label.SetFont(ui_fonts::Text(caption));
        }
        label.SetTextColor(active ? ui_colors::theme::Sun : CaptionColour);
        const float height = TrackHeight * static_cast<float>(percent) / AudioVolumes::Maximum;
        meter.fill->SetVisible(percent > 0);
        meter.fill->SetBounds({(ColumnWidth - TrackWidth) * 0.5F, TrackBottom, TrackWidth, height});
        Paint(*meter.fill, active ? ui_colors::theme::Sun : ui_colors::White);
        const auto value = std::to_wstring(percent) + L"%";
        if (Text(*meter.value).Text() != value)
            Text(*meter.value).SetText(value);
    }
}
