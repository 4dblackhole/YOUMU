#include "EditorTestControls.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Presentation/UiColors.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr double MinimumRate = GameplayLaunchRequest::EditorTestMinimumRate,
                     MaximumRate = GameplayLaunchRequest::EditorTestMaximumRate;
}
void EditorTestControls::Initialize(std::uint32_t width, std::uint32_t height)
{
    canvas_ = visuals_.CreateOwnedCanvas({{1280, 720}, CanvasScaleMode::FixedHeight, {}, 30});
    auto &canvas = *canvas_.Get();
    panel_ = &CreatePanel(canvas.AnchorNode(Anchor::TopLeft), {12, -122, 550, 70}, "EditorTest.Controls");
    panel_->GetComponent<SpriteVisualComponent>()->SetTint(ui_colors::editor::Paper);
    help_ = &CreateLabel(*panel_, {12, 37, 526, 25}, L"", "EditorTest.Help");
    caption_ = &CreateLabel(*panel_, {12, 9, 190, 25}, L"", "EditorTest.RateLabel");
    for (auto *node : {help_, caption_})
    {
        auto &text = *node->GetComponent<TextVisualComponent>();
        texts_.ApplyFont(text);
        text.SetFontSize(node == help_ ? 15.0F : 17.0F);
        text.SetTextColor(ui_colors::editor::Ink);
    }
    slider_ = &CreateSlider(*panel_, {210, 10, 325, 24},
        static_cast<float>((1.0 - MinimumRate) / (MaximumRate - MinimumRate)), "EditorTest.Rate");
    Resize(width, height);
}
void EditorTestControls::Resize(std::uint32_t width, std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (auto *canvas = canvas_.Get())
    {
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        const float scale = std::min(1.0F, std::max(canvas->LogicalSize().width - 24, 1.0F) / 550);
        panel_->Transform().SetScale(scale, scale, 1);
        input_.InvalidateHitTest();
    }
}
std::optional<double> EditorTestControls::Update(const InputState &input, double rate, bool paused)
{
    auto &canvas = *canvas_.Get();
    if (textRevision_ != texts_.Revision())
    {
        for (auto *node : {help_, caption_}) texts_.ApplyFont(*node->GetComponent<TextVisualComponent>());
        textRevision_ = texts_.Revision();
    }
    if (input_.CapturedNode() != slider_->Id())
        slider_->GetComponent<SliderBehaviorComponent>()->SetValue(
            static_cast<float>((rate - MinimumRate) / (MaximumRate - MinimumRate)));
    const auto point = MapScreenPointer(
        {static_cast<float>(input.MousePositionX()), static_cast<float>(input.MousePositionY())},
        {static_cast<float>(width_), static_cast<float>(height_)}, canvas);
    PointerInput pointer;
    pointer.available = input.IsMouseInsideWindow() && point.has_value();
    pointer.position = point.value_or(Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(MouseButton::Left);
    input_.Process(canvas, pointer);
    std::optional<double> changed;
    for (const auto &action : canvas.TakeActions())
        if (action.source == slider_->Id() && action.type == ActionType::ValueChanged)
            changed = std::round(std::lerp(MinimumRate, MaximumRate,
                static_cast<double>(slider_->GetComponent<SliderBehaviorComponent>()->Value())) * 20) / 20;
    help_->GetComponent<TextVisualComponent>()->SetText(std::wstring(texts_.Get(paused ? "Editor.testPaused" : "Editor.testControls")));
    caption_->GetComponent<TextVisualComponent>()->SetText(texts_.Format("Editor.playbackRateFormat", changed.value_or(rate)));
    return changed;
}
void EditorTestControls::Shutdown() noexcept
{
    if (canvas_.Get()) input_.Reset(*canvas_.Get());
    canvas_.Reset();
    panel_ = help_ = caption_ = slider_ = nullptr;
}
