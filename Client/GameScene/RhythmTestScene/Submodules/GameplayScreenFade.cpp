#include "GameplayScreenFade.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    // Larger than any supported aspect, centred on the Canvas, so every edge stays covered.
    constexpr float CoverExtent = 20000.0F;
} // namespace

void GameplayScreenFade::Present(const double opacity, const std::uint32_t width, const std::uint32_t height)
{
    const float clamped = std::isfinite(opacity) ? static_cast<float>(std::clamp(opacity, 0.0, 1.0)) : 0.0F;
    if (clamped == shown_)
        return;
    if (!handle_)
    {
        handle_ = visuals_.CreateOwnedCanvas({{1280.0F, 720.0F}, CanvasScaleMode::FixedHeight, {}, CanvasOrder});
        auto *canvas = handle_.Get();
        if (canvas == nullptr)
            throw std::runtime_error("Failed to create the gameplay screen fade Canvas.");
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        cover_ = &CreatePanel(canvas->CreateNode(Anchor::Center, "Gameplay.ScreenFade"),
                              {-CoverExtent * 0.5F, -CoverExtent * 0.5F, CoverExtent, CoverExtent},
                              "Gameplay.ScreenFade.Cover");
    }
    shown_ = clamped;
    cover_->GetComponent<SpriteVisualComponent>()->SetTint({0.0F, 0.0F, 0.0F, clamped});
    static_cast<void>(handle_.SetVisible(clamped > 0.0F));
}

void GameplayScreenFade::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    if (auto *canvas = handle_.Get())
        canvas->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
}

void GameplayScreenFade::Shutdown() noexcept
{
    cover_ = nullptr;
    shown_ = 0.0F;
    handle_.Reset();
}
