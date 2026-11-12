#include "GameplayPresenter.h"
#include "GameplaySupport.h"

using namespace PhantomCore;

void GameplayPresenter::Initialize(const EngineServices &services,
                                   const PlaySession &session)
{
    session_ = &session;
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas(
        {{1280.0F, 720.0F}, CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (!canvas_)
        throw std::runtime_error("Failed to create the gameplay screen Canvas.");
    CreatePresentation();
    noteView_.Initialize(screenVisuals_, texts_, session);
    noteView_.SetLaneLayout(laneWidth_, judgementLocalY_);
}

bool GameplayPresenter::PrepareAssets(const EngineServices &services)
{
    if (preparingImages_.empty())
    {
        GameplayNoteView::AppendImagePaths(preparingImages_);
        for (const auto file : {L"AccuracyIndicator",
                                L"HudDigits",
                                L"InputPanel",
                                L"LaneLight",
                                L"JudgementCircle",
                                L"Lane",
                                L"KeyButtonLeftKat",
                                L"KeyButtonLeftDon",
                                L"KeyButtonRightDon",
                                L"KeyButtonRightKat",
                                L"KeyLightStrong",
                                L"KeyLightWeak",
                                L"KeyPressFlash",
                                L"Progress.Background",
                                L"Progress.Track",
                                L"Progress.Fill",
                                L"Progress.Marker",
                                L"Judgements.MAX",
                                L"Judgements.PERFECT",
                                L"Judgements.GREAT",
                                L"Judgements.GOOD",
                                L"Judgements.BAD",
                                L"Judgements.MISS",
                                L"Judgements.POOR",
                                L"Judgements.OK"})
            preparingImages_.push_back(InGameSkinAssetPath(file));
    }
    std::string error;
    const bool ready = services.visual2DRendering.PrepareImages(preparingImages_, error);
    if (!error.empty())
        throw std::runtime_error(error);
    if (!ready || backgroundPath_.empty())
        return ready;
    const std::array background{backgroundPath_};
    const bool backgroundReady = services.visual2DRendering.PrepareImages(background, error);
    if (!error.empty())
    {
        backgroundWarning_ = "Background image: " + error;
        backgroundPath_.clear();
        return true;
    }
    return backgroundReady;
}

void GameplayPresenter::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ != nullptr)
    {
        UpdatePresentationLayout();
        judgementView_.OnResize(canvas_->LogicalSize().width);
    }
}

RhythmTime GameplayPresenter::InitialTimelineTime() const
{
    return noteView_.InitialTimelineTime();
}

void GameplayPresenter::Shutdown() noexcept
{
    keyBeam_.Shutdown();
    judgementView_.Shutdown();
    progressView_.Shutdown();
    hpView_.Shutdown();
    statisticsView_.Shutdown();
    noteView_.Shutdown();
    preparingImages_.clear();
    inputView_.Shutdown();
    background_ = nullptr;
    scrollGearRoot_ = nullptr;
    scrollGearBorder_ = nullptr;
    scrollGearSurface_ = nullptr;
    inputPresentationRoot_ = nullptr;
    inputPanel_ = nullptr;
    laneRoot_ = nullptr;
    laneTiles_.clear();
    laneImage_ = {};
    accuracyIndicator_ = nullptr;
#if defined(_DEBUG)
    noteDebugLabel_ = nullptr;
#endif
    errorLabel_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
    session_ = nullptr;
}

void GameplayPresenter::ApplyFeedback(const GameplayFeedback &feedback)
{
    if (feedback.reset)
    {
        keyBeam_.Reset();
        judgementView_.Reset();
    }
    if (feedback.keyPressed)
        keyBeam_.OnKeyPressed(feedback.result);
    PresentJudgementFeedback(feedback);
    noteView_.ApplyFeedback(feedback);
}

void GameplayPresenter::UpdatePresentation(const RhythmTime time)
{
    progressView_.Update(time);
    UpdateAccuracyPresentation();
    statisticsView_.Update(*session_);
#if defined(_DEBUG)
    UpdateDebugPresentation(time);
#endif
    noteView_.Update(time);
}

void GameplayPresenter::SetDebugVisible([[maybe_unused]] bool visible)
{
#if defined(_DEBUG)
    if (noteDebugLabel_) noteDebugLabel_->SetVisible(visible);
#endif
}

void GameplayPresenter::UpdateInputPresentation(const InputState& input, const double deltaSeconds)
{
    inputView_.Update(session_->InputLayout(), input, deltaSeconds);
}
