#include "RhythmTestScene.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameFlow/GameplaySessionController.h"
#include "Submodules/GameplayPauseMenu.h"
#include "Submodules/GameplayPresenter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

RhythmTestScene::RhythmTestScene(std::shared_ptr<GameplayLaunchStore> request,
                                 AudioPlaybackManager &playback,
                                 ScreenVisual2DManager &visuals, bool debug,
                                 TextCatalog &texts,
                                 std::unique_ptr<IPlayResult> &pendingResult)
    : pendingResult_(pendingResult),
      controller_(std::make_unique<GameplaySessionController>(
          request ? request->Snapshot() : throw std::invalid_argument("Gameplay requires a launch store."), playback,
          debug, request ? request->TakeValidatedSession() : nullptr)),
      view_(std::make_unique<GameplayPresenter>(visuals, texts)),
      pauseMenu_(std::make_unique<GameplayPauseMenu>(visuals, texts)),
      visuals_(visuals), texts_(texts), launch_(request->Snapshot())
{
    const auto launch = request->Snapshot();
    view_->ConfigureBackground(launch.backgroundPath, launch.backgroundOffsetX, launch.backgroundOffsetY);
}
RhythmTestScene::~RhythmTestScene() = default;
void RhythmTestScene::Initialize(const EngineServices &services)
{
    services_.emplace(services);
    loadingCanvas_ = visuals_.CreateOwnedCanvas({{1280, 720}, CanvasScaleMode::FixedHeight});
    auto *canvas = loadingCanvas_.Get();
    if (!canvas) throw std::runtime_error("Failed to create loading canvas.");
    auto &root = canvas->AnchorNode(Anchor::Center);
    const auto logical = canvas->LogicalSize();
    loadingBackground_ = &CreatePanel(root, {-logical.width * .5F, -logical.height * .5F,
                                              logical.width, logical.height}, "Loading.Background");
    loadingBackground_->GetComponent<SpriteVisualComponent>()->SetTint({.94F, .97F, 1, 1});
    auto &plate = CreatePanel(root, {-550, -70, 1100, 155}, "Loading.TitlePlate");
    plate.GetComponent<SpriteVisualComponent>()->SetTint({.97F, .99F, 1, .94F});
    auto &title = CreateLabel(root, {-520, 0, 1040, 65}, launch_.songTitle, "Loading.Song");
    auto &pattern = CreateLabel(root, {-520, -55, 1040, 45},
                                launch_.patternTitle.empty() ? launch_.patternPath.stem().wstring() : launch_.patternTitle,
                                "Loading.Pattern");
    for (auto *node : {&title, &pattern})
    {
        auto &text = *node->GetComponent<TextVisualComponent>();
        texts_.ApplyFont(text);
        text.SetHorizontalAlignment(TextAlignment::Center);
        text.SetTextColor({.16F, .32F, .46F, 1});
    }
    title.GetComponent<TextVisualComponent>()->SetFontSize(40);
    pattern.GetComponent<TextVisualComponent>()->SetFontSize(26);
    static_cast<void>(PreparePresentation());
}
void RhythmTestScene::PrepareLoadingBackground()
{
    if (loadingBackgroundReady_ || launch_.backgroundPath.empty() || !services_) return;
    std::string error;
    const std::array paths{launch_.backgroundPath};
    const bool ready = services_->visual2DRendering.PrepareImages(paths, error);
    if (!error.empty()) { loadingBackgroundReady_ = true; return; }
    if (!ready) return;
    try
    {
        const auto image = visuals_.RegisterImage(launch_.backgroundPath);
        const auto size = visuals_.GetImageSize(image);
        if (size.width <= 0 || size.height <= 0)
            throw std::runtime_error("Loading background image has no size.");
        auto &sprite = *loadingBackground_->GetComponent<SpriteVisualComponent>();
        sprite.SetTint({1, 1, 1, 1});
        sprite.SetImage(image);
        loadingBackgroundSize_ = size;
        UpdateLoadingBackgroundLayout();
    }
    catch (const std::exception &) {}
    loadingBackgroundReady_ = true;
}
void RhythmTestScene::UpdateLoadingBackgroundLayout()
{
    if (!loadingBackground_ || !loadingCanvas_.Get()) return;
    const auto logical = loadingCanvas_.Get()->LogicalSize();
    float width = logical.width, height = logical.height;
    Point offset{};
    if (loadingBackgroundSize_.width > 0 && loadingBackgroundSize_.height > 0)
    {
        offset = {static_cast<float>(launch_.backgroundOffsetX), static_cast<float>(launch_.backgroundOffsetY)};
        const auto scale = std::max((logical.width + 2 * std::abs(offset.x)) / loadingBackgroundSize_.width,
                                    (logical.height + 2 * std::abs(offset.y)) / loadingBackgroundSize_.height);
        width = loadingBackgroundSize_.width * scale;
        height = loadingBackgroundSize_.height * scale;
    }
    loadingBackground_->SetBounds({-width * .5F + offset.x, -height * .5F + offset.y, width, height});
}
bool RhythmTestScene::PreparePresentation()
{
    if (presentationReady_)
        return true;
    if (!controller_->PrepareSession())
        return false;
    // Start both preparations before polling either. The playable screen must
    // never appear while cold hit samples are still being decoded.
    const auto audio = controller_->PrepareAudioResources(services_->audio);
    const bool images = view_->PrepareAssets(*services_);
    if (!images || audio == AudioPreparationState::Loading)
        return false;
    view_->Initialize(*services_, controller_->Session());
    controller_->Start(*services_, view_->InitialTimelineTime());
    view_->PresentErrorMessage(controller_->ErrorMessage());
    view_->SetVisible(false);
    presentationReady_ = true;
    return true;
}
void RhythmTestScene::BeginScene()
{
    view_->SetVisible(presentationReady_ && controller_->CurrentState() != PlayState::Loading);
    static_cast<void>(loadingCanvas_.SetVisible(true));
}
void RhythmTestScene::EndScene() noexcept
{
    view_->SetVisible(false);
    static_cast<void>(loadingCanvas_.SetVisible(false));
}
void RhythmTestScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    PrepareLoadingBackground();
    if (!presentationReady_ && (context.totalSeconds < nextPreparationPoll_ || !PreparePresentation()))
    {
        if (context.totalSeconds >= nextPreparationPoll_)
            nextPreparationPoll_ = context.totalSeconds + 0.002;
        if (controller_->SessionReady() && context.input.WasKeyPressed(VK_ESCAPE))
            controller_->OpenPauseMenu(context);
        UpdatePauseMenu(context, scenes);
        return;
    }
    controller_->Update(context);
    view_->SetVisible(controller_->CurrentState() != PlayState::Loading);
    static_cast<void>(loadingCanvas_.SetVisible(controller_->CurrentState() == PlayState::Loading));
    UpdatePauseMenu(context, scenes);
    if (controller_->CurrentState() == PlayState::Gameplay)
        view_->AdvanceEffects(context.deltaSeconds);
    for (const auto &feedback : controller_->Feedback())
        view_->ApplyFeedback(feedback);
    if (controller_->CurrentState() == PlayState::Gameplay)
        view_->UpdateInputPresentation(context.input, context.deltaSeconds);
    view_->PresentErrorMessage(controller_->ErrorMessage());
    if (controller_->ShouldShowResult())
        ShowResult(scenes);
}
void RhythmTestScene::UpdatePauseMenu(const UpdateContext &context, SceneManager &scenes)
{
    if (!controller_->PauseMenuOpen())
    {
        pauseMenu_->Hide();
        return;
    }
    pauseMenu_->Show(services_->windowWidth, services_->windowHeight, controller_->CanContinue(),
                     controller_->Session().Timeline().MeasureSpans().size());
    const auto choice = pauseMenu_->Update(context.input);
    switch (choice.action)
    {
    case GameplayPauseAction::Resume:
        if (controller_->ResumeFromPause(context, choice.leadIn) && !controller_->PauseMenuOpen())
            pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Rewind:
        if (controller_->RewindFromPause(context, choice.measures) && !controller_->PauseMenuOpen())
            pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Retry:
        controller_->RetryFromPause(context);
        pauseMenu_->Hide();
        break;
    case GameplayPauseAction::Retire:
        controller_->RetireFromPause();
        pauseMenu_->Hide();
        ShowResult(scenes);
        break;
    default:
        break;
    }
}
void RhythmTestScene::ShowResult(SceneManager &scenes)
{
    pendingResult_ = controller_->TakeResult();
    if (!scenes.ChangeScene(ResultSceneId))
        throw std::runtime_error("Failed to enter the Result Scene.");
}
void RhythmTestScene::Render(const RenderContext &)
{
    if (presentationReady_)
        view_->UpdatePresentation(controller_->Time());
}
void RhythmTestScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (services_)
    {
        services_->windowWidth = width;
        services_->windowHeight = height;
    }
    view_->OnResize(width, height);
    UpdateLoadingBackgroundLayout();
    pauseMenu_->OnResize(width, height);
}
void RhythmTestScene::Shutdown() noexcept
{
    pauseMenu_->Shutdown();
    loadingBackground_ = nullptr;
    loadingBackgroundSize_ = {};
    loadingCanvas_.Reset();
    loadingBackgroundReady_ = false;
    view_->Shutdown(); // Release session observers before the session.
    controller_->Shutdown();
    services_.reset();
    presentationReady_ = false;
    nextPreparationPoll_ = 0;
}
