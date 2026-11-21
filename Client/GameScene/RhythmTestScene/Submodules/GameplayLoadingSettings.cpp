#include "GameplayLoadingSettings.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

using namespace PhantomCore;

namespace
{
    // Logical 1280x720 layout from the rhythmgameUI proposal (play-settings A): a navy plate
    // 304 wide, 40 from the right edge, centred vertically. Rows are measured from the plate top.
    constexpr float PlateWidth = 304.0F, PlateHeight = 182.0F, RightMargin = 40.0F, Padding = 20.0F;
    constexpr float LabelWidth = 76.0F, RowHeight = 36.0F, ValueWidth = 46.0F;
    constexpr float TitleTop = 16.0F, FirstRowTop = 42.0F, SecondRowTop = 80.0F, HintTop = 126.0F, HoldTop = 148.0F;
    constexpr float TrackHeight = 6.0F, KnobSize = 12.0F, StepHeight = 28.0F, StepButtonWidth = 32.0F;
    constexpr Color Caption{0.737F, 0.816F, 0.933F, 1.0F}; // #BCD0EE

    Visual2DNode &Label(Visual2DNode &parent, const Rect bounds, std::wstring text, std::string name, const float size,
                        const std::uint16_t weight, const Color color, const TextAlignment alignment)
    {
        auto &node = CreateLabel(parent, bounds, L"", std::move(name));
        auto &label = *node.GetComponent<TextVisualComponent>();
        label.SetFont(ui_fonts::Text(text));
        label.SetText(std::move(text));
        label.SetFontSize(size);
        label.SetFontWeight(weight);
        label.SetTextColor(color);
        label.SetHorizontalAlignment(alignment);
        return node;
    }

    Visual2DNode &NumberLabel(Visual2DNode &parent, const Rect bounds, std::string name, const TextAlignment alignment)
    {
        auto &node = CreateLabel(parent, bounds, L"", std::move(name));
        auto &label = *node.GetComponent<TextVisualComponent>();
        auto face = ui_fonts::Number(700, true);
        label.SetFont(std::move(face.font));
        label.SetFontWeight(face.weight);
        label.SetFontSize(17.0F);
        label.SetTextColor(ui_colors::White);
        label.SetHorizontalAlignment(alignment);
        return node;
    }

    void Tint(Visual2DNode &node, const Color color)
    {
        node.GetComponent<SpriteVisualComponent>()->SetTint(color);
    }

    std::wstring SignedMilliseconds(const std::int32_t value)
    {
        // An explicit plus for late, a typographic minus for early, no sign at zero.
        const std::wstring sign = value > 0 ? L"+" : (value < 0 ? L"−" : L"");
        return sign + std::to_wstring(value < 0 ? -value : value) + L" ms";
    }
} // namespace

void GameplayLoadingSettings::Create(const std::uint32_t width, const std::uint32_t height)
{
    handle_ = visuals_.CreateOwnedCanvas({{1280.0F, 720.0F}, CanvasScaleMode::FixedHeight, {}, CanvasOrder});
    auto *canvas = handle_.Get();
    if (canvas == nullptr)
        throw std::runtime_error("Failed to create the loading play settings Canvas.");
    canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
    auto &root = canvas->AnchorNode(Anchor::MiddleRight);
    // +Y is up. The plate hangs left of the right-middle anchor; its children use the plate's
    // own space (origin at its bottom-left), so a row measured from the top sits at y(...).
    plate_ = &CreatePanel(root, {-RightMargin - PlateWidth, -PlateHeight * 0.5F, PlateWidth, PlateHeight},
                          "Loading.Settings");
    auto plate = ui_colors::theme::BrandDeep;
    plate.alpha = 0.86F;
    Tint(*plate_, plate);
    auto &panel = *plate_;
    const auto y = [](const float fromTop, const float height) { return PlateHeight - fromTop - height; };
    const auto &text = texts_;
    static_cast<void>(Label(panel, {Padding, y(TitleTop, 16), PlateWidth - Padding * 2, 16},
                            std::wstring(text.Get("Gameplay.playSettings")), "Loading.Settings.Title", 13, 800, Caption,
                            TextAlignment::Leading));

    // Background brightness: label, track with the gold fill and a white knob, value.
    static_cast<void>(Label(panel, {Padding, y(FirstRowTop, RowHeight), LabelWidth, RowHeight},
                            std::wstring(text.Get("Options.backgroundBrightness")), "Loading.Settings.BrightnessLabel",
                            13, 700, ui_colors::White, TextAlignment::Leading));
    const float trackLeft = Padding + LabelWidth + 10.0F;
    const float trackWidth = PlateWidth - Padding - ValueWidth - 6.0F - trackLeft;
    const float trackY = y(FirstRowTop, RowHeight) + (RowHeight - TrackHeight) * 0.5F;
    track_ = &CreatePanel(panel, {trackLeft, trackY, trackWidth, TrackHeight}, "Loading.Settings.BrightnessTrack");
    Tint(*track_, {1.0F, 1.0F, 1.0F, 0.22F});
    fill_ = &CreatePanel(*track_, {0.0F, 0.0F, trackWidth, TrackHeight}, "Loading.Settings.BrightnessFill");
    Tint(*fill_, ui_colors::theme::Sun);
    knob_ = &CreatePanel(*track_, {0.0F, (TrackHeight - KnobSize) * 0.5F, KnobSize, KnobSize},
                         "Loading.Settings.BrightnessKnob");
    Tint(*knob_, ui_colors::White);
    brightnessValue_ = &NumberLabel(panel, {PlateWidth - Padding - ValueWidth, y(FirstRowTop, RowHeight), ValueWidth, RowHeight},
                                    "Loading.Settings.BrightnessValue", TextAlignment::Trailing);

    // Local offset: label and a -/value/+ stepper.
    static_cast<void>(Label(panel, {Padding, y(SecondRowTop, RowHeight), LabelWidth, RowHeight},
                            std::wstring(text.Get("Gameplay.localOffset")), "Loading.Settings.OffsetLabel", 13, 700,
                            ui_colors::White, TextAlignment::Leading));
    const float stepLeft = trackLeft;
    const float stepWidth = PlateWidth - Padding - stepLeft;
    const float stepY = y(SecondRowTop, RowHeight) + (RowHeight - StepHeight) * 0.5F;
    auto &stepper = CreatePanel(panel, {stepLeft, stepY, stepWidth, StepHeight}, "Loading.Settings.Offset");
    Tint(stepper, {1.0F, 1.0F, 1.0F, 0.10F});
    minus_ = &CreatePanel(stepper, {0.0F, 0.0F, StepButtonWidth, StepHeight}, "Loading.Settings.OffsetMinus");
    plus_ = &CreatePanel(stepper, {stepWidth - StepButtonWidth, 0.0F, StepButtonWidth, StepHeight},
                         "Loading.Settings.OffsetPlus");
    for (auto *button : {minus_, plus_})
        Tint(*button, {1.0F, 1.0F, 1.0F, 0.16F});
    static_cast<void>(Label(*minus_, {0, 0, StepButtonWidth, StepHeight}, L"−", "Loading.Settings.OffsetMinus.Text", 16, 700,
                            ui_colors::White, TextAlignment::Center));
    static_cast<void>(Label(*plus_, {0, 0, StepButtonWidth, StepHeight}, L"+", "Loading.Settings.OffsetPlus.Text", 16, 700,
                            ui_colors::White, TextAlignment::Center));
    offsetValue_ = &NumberLabel(stepper, {StepButtonWidth, 0, stepWidth - StepButtonWidth * 2, StepHeight},
                                "Loading.Settings.OffsetValue", TextAlignment::Center);

    static_cast<void>(Label(panel, {Padding, y(HintTop, 16), PlateWidth - Padding * 2, 16},
                            std::wstring(text.Get("Gameplay.localOffsetKeys")), "Loading.Settings.Keys", 11, 600, Caption,
                            TextAlignment::Leading));
    static_cast<void>(Label(panel, {Padding, y(HoldTop, 16), PlateWidth - Padding * 2, 16},
                            std::wstring(text.Get("Gameplay.holdToWait")), "Loading.Settings.Hold", 11, 600,
                            ui_colors::theme::Sun, TextAlignment::Leading));
}

void GameplayLoadingSettings::Show(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (!handle_)
        Create(width, height);
    if (!visible_)
        static_cast<void>(handle_.SetVisible(true));
    visible_ = true;
}

void GameplayLoadingSettings::Hide() noexcept
{
    if (visible_)
        static_cast<void>(handle_.SetVisible(false));
    visible_ = false;
    dragging_ = false;
}

void GameplayLoadingSettings::ShowBrightness(const int percent)
{
    const int clamped = std::clamp(percent, 0, 100);
    if (clamped == shownBrightness_ || track_ == nullptr)
        return;
    shownBrightness_ = clamped;
    const float width = track_->NodeSize().width;
    const float filled = width * static_cast<float>(clamped) / 100.0F;
    fill_->SetSize({filled, TrackHeight});
    knob_->SetPosition({filled - KnobSize * 0.5F, (TrackHeight - KnobSize) * 0.5F});
    brightnessValue_->GetComponent<TextVisualComponent>()->SetText(std::to_wstring(clamped) + L"%");
}

void GameplayLoadingSettings::SetValues(const int brightnessPercent, const std::int32_t offsetMs)
{
    if (!handle_)
        return;
    if (!dragging_)
        ShowBrightness(brightnessPercent);
    if (shownOffset_ != offsetMs)
    {
        shownOffset_ = offsetMs;
        offsetValue_->GetComponent<TextVisualComponent>()->SetText(SignedMilliseconds(offsetMs));
    }
}

std::optional<Point> GameplayLoadingSettings::Pointer(const InputState &input) const
{
    auto *canvas = handle_.Get();
    if (canvas == nullptr || !input.IsMouseInsideWindow())
        return std::nullopt;
    return MapScreenPointer({static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
                            {static_cast<float>(width_), static_cast<float>(height_)}, *canvas);
}

int GameplayLoadingSettings::BrightnessAt(const Point pointer) const
{
    const auto bounds = track_->BoundsInCanvas();
    if (bounds.width <= 0.0F)
        return shownBrightness_;
    const float fraction = std::clamp((pointer.x - bounds.x) / bounds.width, 0.0F, 1.0F);
    return static_cast<int>(std::lround(fraction * 100.0F));
}

GameplayLoadingSettings::Interaction GameplayLoadingSettings::Update(const InputState &input)
{
    Interaction result;
    if (!visible_ || plate_ == nullptr)
        return result;
    const auto pointer = Pointer(input);
    const bool pressed = input.WasMouseButtonPressed(MouseButton::Left);
    if (pointer && plate_->BoundsInCanvas().Contains(*pointer))
    {
        result.holdStart = true;
        // The slider takes a press anywhere on its row height, not only on the 6px line.
        auto track = track_->BoundsInCanvas();
        track.y -= RowHeight * 0.5F;
        track.height += RowHeight;
        if (pressed && track.Contains(*pointer))
            dragging_ = true;
        std::int32_t step = 5;
        if (input.IsKeyDown(VK_MENU))
            step = 1;
        else if (input.IsKeyDown(VK_SHIFT))
            step = 20;
        if (pressed && minus_->BoundsInCanvas().Contains(*pointer))
            result.offsetStepMs = -step;
        if (pressed && plus_->BoundsInCanvas().Contains(*pointer))
            result.offsetStepMs = step;
    }
    if (dragging_)
    {
        if (pointer)
        {
            const int percent = BrightnessAt(*pointer);
            if (percent != shownBrightness_)
            {
                ShowBrightness(percent);
                result.brightness = percent;
            }
        }
        if (!input.IsMouseButtonDown(MouseButton::Left))
        {
            dragging_ = false;
            result.brightnessCommitted = true;
            result.brightness = shownBrightness_;
        }
        else
            result.holdStart = true; // A drag that left the plate still holds the start.
    }
    return result;
}

void GameplayLoadingSettings::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (auto *canvas = handle_.Get())
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
}

void GameplayLoadingSettings::Shutdown() noexcept
{
    plate_ = track_ = fill_ = knob_ = brightnessValue_ = minus_ = plus_ = offsetValue_ = nullptr;
    shownBrightness_ = -1;
    shownOffset_.reset();
    visible_ = false;
    dragging_ = false;
    handle_.Reset();
}
