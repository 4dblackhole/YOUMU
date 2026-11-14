#include "GameplayHpView.h"
#include "App/AssetPaths.h"
#include <algorithm>

using namespace PhantomCore;

namespace
{
    // Logical sizes of the 1080p parts (x 2/3).
    constexpr float FrameWidth = 490, FrameHeight = 24, FrameGap = 16;
    // The white cell starts after the 32-wide lock rest and is 454x18.
    constexpr Rect Cell{32, 3, 454, 18};
    constexpr Size ClearMarkerSize{10, 27.333F}, LockSize{30, 30};
    // The marker's triangle rises 6 above the frame.
    constexpr float ClearMarkerRise = 6;
} // namespace

void GameplayHpView::Initialize(ScreenVisual2DManager &visuals, Visual2DNode &parent)
{
    const auto image = [&visuals](const wchar_t *key) { return visuals.RegisterImage(InGame(key)); };
    lowImage_ = image(L"Hud.HpGauge.FillLow");
    clearImage_ = image(L"Hud.HpGauge.FillClear");
    frame_ = &CreateSprite(parent, {0, 0, FrameWidth, FrameHeight}, image(L"Hud.HpGauge.Frame"), "Hud.HP.Frame");
    frame_->SetZIndex(5);
    fill_ = &CreateSprite(*frame_, {}, lowImage_, "Hud.HP.Fill");
    clearMarker_ = &CreateSprite(*frame_, {}, image(L"Hud.HpGauge.ClearMarker"), "Hud.HP.ClearMarker");
    clearMarker_->SetZIndex(1);
    lock_ = &CreateSprite(*frame_, {1, (FrameHeight - LockSize.height) * 0.5F, LockSize.width, LockSize.height},
                          image(L"Hud.HpGauge.Lock"), "Hud.HP.Lock");
    lock_->SetZIndex(2);
    Apply();
}
void GameplayHpView::SetLayout(float logicalWidth, float gearTop)
{
    if (!frame_)
        return;
    // The frame starts at the left screen edge like the timing guide below the gear.
    frame_->SetBounds({-logicalWidth * 0.5F, gearTop + FrameGap, FrameWidth, FrameHeight});
    Apply();
}
void GameplayHpView::Update(const GameplayHpState &state)
{
    state_ = state;
    Apply();
}
void GameplayHpView::Apply()
{
    if (!frame_)
        return;
    const auto fraction = static_cast<float>(std::clamp(state_.percent / 100.0, 0.0, 1.0));
    const auto clear = static_cast<float>(std::clamp(state_.clearPercent / 100.0, 0.0, 1.0));
    // The fill is the cell-sized picture cropped from the left, never stretched.
    fill_->SetVisible(fraction > 0);
    fill_->SetBounds({Cell.x, Cell.y, Cell.width * fraction, Cell.height});
    auto &sprite = *fill_->GetComponent<SpriteVisualComponent>();
    sprite.SetImage(state_.percent >= state_.clearPercent ? clearImage_ : lowImage_);
    sprite.SetUvTransform({fraction, 1.0F}, {});
    clearMarker_->SetBounds({Cell.x + Cell.width * clear - ClearMarkerSize.width * 0.5F,
                             FrameHeight + ClearMarkerRise - ClearMarkerSize.height, ClearMarkerSize.width,
                             ClearMarkerSize.height});
    lock_->SetVisible(state_.locked);
}
