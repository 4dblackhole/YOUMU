#include "GameplayHpView.h"
#include <algorithm>

using namespace PhantomCore;

void GameplayHpView::Initialize(Visual2DNode &parent)
{
    track_ = &CreatePanel(parent, {}, "Hud.HP.Track");
    track_->SetZIndex(5);
    track_->GetComponent<SpriteVisualComponent>()->SetTint({203.0F / 255, 213.0F / 255, 225.0F / 255, 1});
    fill_ = &CreatePanel(*track_, {}, "Hud.HP.Fill");
    fill_->GetComponent<SpriteVisualComponent>()->SetTint({50.0F / 255, 203.0F / 255, 224.0F / 255, 1});
    Update(0);
}
void GameplayHpView::SetLayout(float logicalWidth)
{
    if (!track_)
        return;
    constexpr float scale = 2.0F / 3.0F;
    track_->SetBounds({-logicalWidth * .5F + 231 * scale, 360 - (251 + 20) * scale,
                       std::min(620 * scale, std::max(0.0F, logicalWidth - 2 * 231 * scale)), 20 * scale});
    Update(fraction_ * 100.0);
}
void GameplayHpView::Update(double percent)
{
    fraction_ = static_cast<float>(std::clamp(percent / 100.0, 0.0, 1.0));
    if (!fill_)
        return;
    const auto size = track_->NodeSize();
    fill_->SetBounds({0, 0, size.width * fraction_, size.height});
    fill_->SetVisible(fraction_ > 0 && size.width > 0);
}
