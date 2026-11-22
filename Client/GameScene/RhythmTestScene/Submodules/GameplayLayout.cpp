#include "GameplayPresenter.h"
#include "GameplaySupport.h"
#include "Presentation/BackgroundLayout.h"
#include "Presentation/AccuracyText.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include <algorithm>

using namespace PhantomCore;

void GameplayPresenter::ConfigureBackgroundBrightness(const int percent)
{
    backgroundBrightness_ = static_cast<float>(std::clamp(percent, 0, 100)) / 100.0F;
    ApplyBackgroundBrightness();
}

void GameplayPresenter::ApplyBackgroundBrightness()
{
    if (background_ == nullptr)
        return;
    // The chart image, or the plain sky colour without one, darkened toward black.
    const Color base = backgroundImage_ ? Color{1, 1, 1, 1} : Color{0.941F, 0.973F, 1.0F, 1.0F};
    const float b = backgroundBrightness_;
    // A tint keeps the chart image; a whole style would reset it.
    RequireComponent<SpriteVisualComponent>(*background_).SetTint({base.red * b, base.green * b, base.blue * b, 1.0F});
}

void GameplayPresenter::CreatePresentation()
{
    auto &root = canvas_->Root();

    background_ =
        &CreatePanel(root,
                                    {-CanvasReferenceWidth * 0.5F, -CanvasReferenceHeight * 0.5F,
                                     CanvasReferenceWidth, CanvasReferenceHeight},
                                    "Background");
    if (!backgroundPath_.empty())
    {
        try
        {
            backgroundImage_ = screenVisuals_.RegisterImage(backgroundPath_);
            backgroundSize_ = screenVisuals_.GetImageSize(backgroundImage_);
            if (backgroundSize_.width <= 0 || backgroundSize_.height <= 0)
                throw std::runtime_error("Background image has no size.");
            RequireComponent<SpriteVisualComponent>(*background_).SetImage(backgroundImage_);
        }
        catch (const std::exception &error)
        {
            backgroundWarning_ = error.what();
            backgroundImage_ = {};
        }
    }
    ApplyBackgroundBrightness();

    progressView_.Initialize(screenVisuals_, root, *session_, texts_);
    hpView_.Initialize(screenVisuals_, root);

    // Accuracy is fixed in the top-right panel, a number without a label.
    accuracyIndicator_ = &CreateLabel(root, {}, std::wstring(EmptyAccuracyText), "Hud.Accuracy");
    accuracyIndicator_->SetZIndex(5);
    auto &accuracyText = RequireComponent<TextVisualComponent>(*accuracyIndicator_);
    auto accuracyFace = ui_fonts::Number(800);
    accuracyText.SetFont(std::move(accuracyFace.font));
    accuracyText.SetFontWeight(accuracyFace.weight);
    accuracyText.SetFontSize(32.0F);
    accuracyText.SetTextColor(ui_colors::theme::Ink);
    accuracyText.SetHorizontalAlignment(TextAlignment::Trailing);

#if defined(_DEBUG)
    noteDebugLabel_ = &CreateLabel(
        root, {-CanvasReferenceWidth * 0.5F + 12.0F, 160.0F, CanvasReferenceWidth - 24.0F, 160.0F},
        L"", "Debug.NoteState");
    noteDebugLabel_->SetZIndex(30);
    auto &debugText = RequireComponent<TextVisualComponent>(*noteDebugLabel_);
    texts_.ApplyFont(debugText);
    debugText.SetFontSize(16.0F);
    debugText.SetTextColor({0.12F, 0.16F, 0.24F, 1.0F});
#endif

    // All gear parts inherit one transform. Final bounds depend on skin sizes.
    scrollGearRoot_ = &root.CreateChild("ScrollGear");
    // The cobalt band with its hatch runs from the screen's left edge to the right edge; the input
    // panel and the lane cover all of it except the 4 px border and the strip left of the panel.
    scrollGearBorder_ = &CreateSprite(*scrollGearRoot_, {}, screenVisuals_.RegisterImage(InGameSkinAssetPath(L"Gear.Band")),
                                      "ScrollGear.Border");

    CreateLaneVisuals(*scrollGearRoot_);
    inputPresentationRoot_ = &scrollGearRoot_->CreateChild("InputPresentation");
    inputPresentationRoot_->SetZIndex(2);
    const auto inputPanelImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"InputPanel"));
    inputPanelSize_ = ScaledImageSize(screenVisuals_, inputPanelImage);
    inputPanel_ = &CreateSprite(
        *inputPresentationRoot_, {0.0F, 0.0F, inputPanelSize_.width, inputPanelSize_.height},
        inputPanelImage, "Input.Panel");
    inputPanel_->SetZIndex(0);
    inputView_.Initialize(screenVisuals_, *inputPanel_);
    statisticsView_.Initialize(screenVisuals_, root, *inputPanel_, texts_);

    UpdatePresentationLayout();
}

void GameplayPresenter::CreateLaneVisuals(Visual2DNode &gearRoot)
{
    // Author the lane in its reusable default orientation: local +Y is the
    // future-note direction and every lane-owned visual is a descendant.
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength =
        std::max(canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin, 1.0F);
    laneRoot_ = &gearRoot.CreateChild("ScrollGear.Lane");
    laneRoot_->SetPivot({0.5F, 0.5F});
    laneRoot_->SetSize({laneWidth_, laneLength});
    laneRoot_->SetZIndex(1);

    // Taiko is a presentation of the same vertical lane rotated clockwise.
    // Notes continue to update only local Y, so changing this one transform
    // redirects the background, effects, judgement line and notes together.
    laneRoot_->Transform().SetRotationRollPitchYaw(0.0F, 0.0F, -HalfPi);

    CreateLaneSurface();

    const auto beamImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"LaneLight"));
    keyBeam_.Initialize(*laneRoot_, beamImage, ScaledImageSize(screenVisuals_, beamImage));

    noteView_.Attach(*canvas_, *laneRoot_);

    const auto judgementImage = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"JudgementCircle"));
    const auto judgementSize = ScaledImageSize(screenVisuals_, judgementImage);
    auto &judgementLine = CreateSprite(
        *laneRoot_,
        {(laneWidth_ - judgementSize.width) * 0.5F, judgementLocalY_ - judgementSize.height * 0.5F,
         judgementSize.width, judgementSize.height},
        judgementImage, "Lane.JudgementCircle");
    judgementLine.SetZIndex(3);
    highlightView_.Initialize(screenVisuals_, *laneRoot_, judgementLine, judgementImage);
    JudgeLevel level;
    const auto &lanes = session_->Gear().Lanes();
    if (!lanes.empty() && !lanes.front()->Notes().empty())
        level = lanes.front()->Notes().front()->Profile().Level();
    // Keep the guide fixed for this play session. In particular, do not use
    // the current note's padded syncopation range to resize the HUD mid-play.
    const AccuracyRange baseRange{"Hud.Default", level};
    judgementView_.Initialize(screenVisuals_, *canvas_, judgementLine, baseRange, texts_);
}

void GameplayPresenter::CreateLaneSurface()
{
    if (laneRoot_ == nullptr)
    {
        throw std::logic_error("Lane background requires a Lane root.");
    }

    laneImage_ = screenVisuals_.RegisterImage(InGameSkinAssetPath(L"Lane"));
    const auto tileSize = ScaledImageSize(screenVisuals_, laneImage_);
    laneWidth_ = tileSize.width;
    laneTileLength_ = tileSize.height;
    judgementLocalY_ = laneWidth_ * 0.5F;
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    UpdateLaneSurfaceLayout(
        std::max(canvas_->LogicalSize().width - laneScreenLeft - GearRightMargin, 1.0F));
}

void GameplayPresenter::UpdateLaneSurfaceLayout(const float laneLength)
{
    keyBeam_.SetLayout(laneWidth_, laneLength);
    if (laneRoot_ == nullptr || !laneImage_ || laneTileLength_ <= 0.0F)
    {
        return;
    }

    const std::size_t requiredTiles =
        static_cast<std::size_t>(std::ceil(laneLength / laneTileLength_));
    while (laneTiles_.size() < requiredTiles)
    {
        auto &tile = CreateSprite(*laneRoot_, {}, laneImage_, "Lane.SurfaceTile");
        tile.SetZIndex(0);
        laneTiles_.push_back(&tile);
    }
    for (std::size_t index = 0; index < laneTiles_.size(); ++index)
    {
        Visual2DNode &tile = *laneTiles_[index];
        const float start = static_cast<float>(index) * laneTileLength_;
        const float visibleLength = std::clamp(laneLength - start, 0.0F, laneTileLength_);
        tile.SetVisible(visibleLength > 0.0F);
        if (visibleLength <= 0.0F)
        {
            continue;
        }
        tile.SetBounds({0.0F, start, laneWidth_, visibleLength});
        RequireComponent<SpriteVisualComponent>(tile).SetUvTransform(
            {1.0F, visibleLength / laneTileLength_}, {0.0F, 0.0F});
    }
}

void GameplayPresenter::UpdatePresentationLayout()
{
    if (canvas_ == nullptr)
    {
        return;
    }

    const float logicalWidth = canvas_->LogicalSize().width;
#if defined(_DEBUG)
    if (noteDebugLabel_ != nullptr)
    {
        noteDebugLabel_->SetBounds(
            {-logicalWidth * 0.5F + 12.0F, 160.0F, std::max(logicalWidth - 24.0F, 1.0F), 160.0F});
    }
#endif
    const float laneScreenLeft = GearMargin + inputPanelSize_.width;
    const float laneLength = std::max(logicalWidth - laneScreenLeft - GearRightMargin, 1.0F);
    const float gearHeight = std::max(laneWidth_, inputPanelSize_.height);
    // Canvas +Y points up. Only the shared parent positions the entire gear:
    // its outer frame's bottom touches the screen's horizontal centre.
    if (scrollGearRoot_ != nullptr)
        scrollGearRoot_->SetPosition({0.0F, gearHeight * 0.5F + GearPadding});

    if (background_ != nullptr)
    {
        // Cover the viewport at the authored centre offset without blank edges.
        const bool image = backgroundImage_.value != 0;
        background_->SetBounds(ChartBackgroundBounds({logicalWidth, CanvasReferenceHeight},
                                                     image ? backgroundSize_ : Size{}, image ? backgroundOffset_ : Point{}));
    }
    // The progress strip spans the bottom edge of the screen.
    progressView_.SetLayout(
        {-logicalWidth * 0.5F, -CanvasReferenceHeight * 0.5F, std::max(logicalWidth, 1.0F), progressView_.Height()});
    // The timing guide is centred on the screen below the gear, whose bottom is the screen's centre line.
    judgementView_.SetGuidePlacement(0.0F);
    hpView_.SetLayout(logicalWidth, gearHeight + GearPadding * 2.0F);
    statisticsView_.SetLayout(logicalWidth, inputPanelSize_);
    if (accuracyIndicator_ != nullptr)
        accuracyIndicator_->SetBounds(statisticsView_.AccuracyBounds());
    if (scrollGearBorder_ != nullptr)
    {
        scrollGearBorder_->SetBounds({-logicalWidth * 0.5F, -gearHeight * 0.5F - GearPadding, std::max(logicalWidth, 1.0F),
                                      gearHeight + GearPadding * 2.0F});
    }
    if (laneRoot_ != nullptr)
    {
        laneRoot_->SetSize({laneWidth_, laneLength});
        laneRoot_->SetPosition(
            {-logicalWidth * 0.5F + laneScreenLeft + laneLength * 0.5F, 0.0F});
        UpdateLaneSurfaceLayout(laneLength);
        noteView_.SetLaneLayout(laneWidth_, judgementLocalY_);
        highlightView_.SetLaneLayout(laneWidth_, laneLength, judgementLocalY_);
    }
    if (inputPresentationRoot_ != nullptr)
    {
        inputPresentationRoot_->SetPosition(
            {-logicalWidth * 0.5F + GearMargin, -inputPanelSize_.height * 0.5F});
    }
}
