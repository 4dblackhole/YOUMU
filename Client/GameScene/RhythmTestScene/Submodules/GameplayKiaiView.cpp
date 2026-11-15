#include "GameplayKiaiView.h"
#include "GameplaySupport.h"

using namespace PhantomCore;

namespace
{
    // The circle image multiplied by pale gold, as in the mockup's Kiai circle.
    constexpr Color KiaiCircleGold{1.0F, 0.812F, 0.431F, 1.0F};
    // Glow opacity at the start of a beat and at its end; the ring grows by 3 of its 51 px radius.
    constexpr float GlowBase = 0.4F, GlowPulse = 0.25F, RingGrowth = 3.0F / 51.0F;
}

void GameplayKiaiView::Initialize(ScreenVisual2DManager &visuals, Visual2DNode &laneRoot,
                                  Visual2DNode &judgementCircle, const ImageHandle circleImage)
{
    Shutdown();
    const auto glowImage = visuals.RegisterImage(InGameSkinAssetPath(L"Kiai.Glow"));
    const auto ringImage = visuals.RegisterImage(InGameSkinAssetPath(L"Kiai.Ring"));
    glowSize_ = ScaledImageSize(visuals, glowImage);
    ringSize_ = ScaledImageSize(visuals, ringImage);
    circleSize_ = judgementCircle.NodeSize();

    // Between the measure lines and the judgement circle; a clipped branch keeps the glow inside the lane.
    clip_ = &laneRoot.CreateChild("Lane.Kiai");
    clip_->SetZIndex(2);
    glow_ = &CreateSprite(*clip_, {}, glowImage, "Lane.Kiai.Glow");
    // Over the circle but under the judgement ring and text, which are the circle's later children.
    gold_ = &CreateSprite(judgementCircle, {0.0F, 0.0F, circleSize_.width, circleSize_.height}, circleImage,
                          "Lane.JudgementCircle.Kiai");
    gold_->SetZIndex(-2);
    ring_ = &CreateSprite(judgementCircle, {}, ringImage, "Lane.JudgementCircle.KiaiRing");
    ring_->SetZIndex(-1);
    Present(0.0F);
}

void GameplayKiaiView::SetLaneLayout(const float laneWidth, const float laneLength, const float judgementLocalY)
{
    if (clip_ == nullptr)
        return;
    laneWidth_ = laneWidth;
    judgementLocalY_ = judgementLocalY;
    clip_->SetBounds({0.0F, 0.0F, laneWidth, laneLength});
    clip_->SetClipRect({0.0F, 0.0F, laneWidth, laneLength});
    glow_->SetBounds({(laneWidth - glowSize_.width) * 0.5F, judgementLocalY - glowSize_.height * 0.5F,
                      glowSize_.width, glowSize_.height});
}

void GameplayKiaiView::Update(const PlaySession &session, const RhythmTime time)
{
    if (clip_ == nullptr)
        return;
    const float target = session.IsKiaiActive(time) ? 1.0F : 0.0F;
    // Song time drives the fade, so a pause holds it and a seek backwards lands on the new state.
    const bool continuous = lastTime_.has_value() && time >= *lastTime_;
    if (!continuous)
        strength_ = target;
    else
    {
        const float step = static_cast<float>((time - *lastTime_).count()) / static_cast<float>(FadeDuration.count());
        strength_ = target > strength_ ? std::min(target, strength_ + step) : std::max(target, strength_ - step);
    }
    lastTime_ = time;
    const long double beats = session.Timeline().WholeNotesAtTime(time) * 4.0L;
    const float phase = static_cast<float>(beats - std::floor(beats));
    Present((1.0F - phase) * (1.0F - phase));
}

void GameplayKiaiView::Present(const float pulse)
{
    const bool shown = strength_ > 0.0F;
    for (auto *node : {glow_, gold_, ring_})
        node->SetVisible(shown);
    if (!shown)
        return;
    RequireComponent<SpriteVisualComponent>(*glow_).SetTint({1.0F, 1.0F, 1.0F, (GlowBase + GlowPulse * pulse) * strength_});
    auto gold = KiaiCircleGold;
    gold.alpha = strength_;
    RequireComponent<SpriteVisualComponent>(*gold_).SetTint(gold);
    const float scale = 1.0F + RingGrowth * pulse;
    const Size ring{ringSize_.width * scale, ringSize_.height * scale};
    ring_->SetBounds({(circleSize_.width - ring.width) * 0.5F, (circleSize_.height - ring.height) * 0.5F, ring.width,
                      ring.height});
    RequireComponent<SpriteVisualComponent>(*ring_).SetTint({1.0F, 1.0F, 1.0F, strength_});
}

void GameplayKiaiView::Reset() noexcept
{
    lastTime_.reset();
}

void GameplayKiaiView::Shutdown() noexcept
{
    clip_ = glow_ = gold_ = ring_ = nullptr;
    lastTime_.reset();
    strength_ = 0.0F;
}
