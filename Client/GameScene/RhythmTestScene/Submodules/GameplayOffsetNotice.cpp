#include "GameplayOffsetNotice.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    constexpr float PlateWidth = 280.0F, PlateHeight = 36.0F, PlateTop = 64.0F;
    constexpr double HoldSeconds = 1.5, FadeSeconds = 0.3;
    constexpr float PlateAlpha = 0.85F;
} // namespace

void GameplayOffsetNotice::Show(std::wstring text, const std::uint32_t width, const std::uint32_t height)
{
    if (!handle_)
    {
        handle_ = visuals_.CreateOwnedCanvas({{1280.0F, 720.0F}, CanvasScaleMode::FixedHeight, {}, CanvasOrder});
        auto *canvas = handle_.Get();
        if (canvas == nullptr)
            throw std::runtime_error("Failed to create the local offset notice Canvas.");
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        // Centre anchor with +Y up, like the screen error line; the plate sits below that line.
        auto &root = canvas->CreateNode(Anchor::Center, "Gameplay.OffsetNotice");
        const float top = canvas->LogicalSize().height * 0.5F;
        plate_ = &CreatePanel(root, {-PlateWidth * 0.5F, top - PlateTop - PlateHeight, PlateWidth, PlateHeight},
                              "Gameplay.OffsetNotice.Plate");
        label_ = &CreateLabel(*plate_, {0.0F, 0.0F, PlateWidth, PlateHeight}, L"", "Gameplay.OffsetNotice.Text");
        auto &label = *label_->GetComponent<TextVisualComponent>();
        label.SetFontSize(17.0F);
        label.SetFontWeight(700);
        label.SetTextColor(ui_colors::White);
        label.SetHorizontalAlignment(TextAlignment::Center);
    }
    auto &label = *label_->GetComponent<TextVisualComponent>();
    label.SetFont(ui_fonts::Text(text));
    label.SetText(std::move(text));
    elapsed_ = 0.0;
    Update(0.0);
    static_cast<void>(handle_.SetVisible(true));
}

void GameplayOffsetNotice::Update(const double deltaSeconds)
{
    if (!handle_ || !std::isfinite(deltaSeconds))
        return;
    elapsed_ += std::max(deltaSeconds, 0.0);
    const double fade = std::clamp((elapsed_ - HoldSeconds) / FadeSeconds, 0.0, 1.0);
    const auto alpha = static_cast<float>(1.0 - fade);
    auto plate = ui_colors::theme::BrandDeep;
    plate.alpha = PlateAlpha * alpha;
    plate_->GetComponent<SpriteVisualComponent>()->SetTint(plate);
    auto text = ui_colors::White;
    text.alpha = alpha;
    label_->GetComponent<TextVisualComponent>()->SetTextColor(text);
    if (fade >= 1.0)
        static_cast<void>(handle_.SetVisible(false));
}

void GameplayOffsetNotice::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    if (auto *canvas = handle_.Get())
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
}

void GameplayOffsetNotice::Shutdown() noexcept
{
    plate_ = nullptr;
    label_ = nullptr;
    handle_.Reset();
}
