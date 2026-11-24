#include "RhythmTestScene.h"
#include "App/AssetPaths.h"
#include "App/GameDataPaths.h"
#include "App/OptionSettings.h"
#include "Records/LocalOffsetStore.h"
#include "Presentation/BackgroundLayout.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include <cmath>
#include "GameFlow/YOUMUSceneIds.h"
#include "GameFlow/GameplaySessionController.h"
#include "Submodules/GameplayPauseMenu.h"
#include "Submodules/GameplayPresenter.h"
#include "Submodules/GameplaySupport.h"
#include "Texts/GameScene/RhythmTestScene/GameplayTexts.h"
#include "Gauge/BasicHpGauge.h"
#include "Gauge/HpGaugeKind.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    // Builds the gauge chosen in Play options. Only the default gauge exists (HP-02).
    std::unique_ptr<IHpGauge> CreateHpGauge(const HpGaugeKind kind, const HpLevel level,
                                            const std::span<const HpNoteDefinition> notes)
    {
        switch (kind)
        {
        case HpGaugeKind::Default:
            return std::make_unique<BasicHpGauge>(level, notes);
        }
        throw std::invalid_argument("Unknown HP gauge.");
    }

    constexpr float LoadingBarWidth = 420.0F;
    constexpr float LoadingBarHeight = 6.0F;

    // The sky-blue piece fills the bar from the left by the share of files ready (user decision 2026-11-25);
    // its gradient is revealed, not stretched.
    void FillLoadingBar(Visual2DNode *piece, const float fraction)
    {
        if (!piece)
            return;
        const float shown = std::clamp(fraction, 0.0F, 1.0F);
        piece->SetSize({LoadingBarWidth * shown, LoadingBarHeight});
        piece->GetComponent<SpriteVisualComponent>()->SetUvTransform({shown, 1.0F}, {});
    }
} // namespace

RhythmTestScene::RhythmTestScene(std::shared_ptr<GameplayLaunchStore> request,
                                 AudioPlaybackManager &playback,
                                 ScreenVisual2DManager &visuals, bool debug,
                                 TextCatalog &texts,
                                 std::unique_ptr<IPlayResult> &pendingResult,
                                 OptionSettings *options)
    : pendingResult_(pendingResult),
      controller_(std::make_unique<GameplaySessionController>(
          request ? request->Snapshot() : throw std::invalid_argument("Gameplay requires a launch store."), playback,
          debug, request ? request->TakeValidatedSession() : nullptr)),
      view_(std::make_unique<GameplayPresenter>(visuals, texts)),
      pauseMenu_(std::make_unique<GameplayPauseMenu>(visuals, texts)),
      visuals_(visuals), texts_(texts), launch_(request->Snapshot()), appliedVolumes_(launch_.volumes), errorLine_(visuals),
      screenFade_(visuals), offsetNotice_(visuals), loadingSettings_(visuals, texts), options_(options)
{
    view_->ConfigureBackground(launch_.backgroundPath, launch_.backgroundOffsetX, launch_.backgroundOffsetY);
    view_->ConfigureKeyBeamOpacity(launch_.keyBeamOpacity);
    view_->ConfigureBackgroundBrightness(launch_.backgroundBrightness);
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
    // Attach the selection's cached BG before the first loading frame, and start
    // cold BG preparation before the larger gameplay/audio preparation batch.
    PrepareLoadingBackground();
    // Redesigned loading screen (UI-02): one band across the screen introduces the song
    // and chart. The band is wider than any supported aspect so it always reaches both edges.
    const auto loadingImage = [this](const wchar_t *key) { return visuals_.RegisterImage(Loading(key)); };
    loadingCard_ = &root.CreateChild("Loading.Card");
    auto &card = *loadingCard_;
    static_cast<void>(CreateSprite(card, {-2000, -80, 4000, 160}, loadingImage(L"Card"), "Loading.TitlePlate"));
    auto &title = CreateLabel(card, {-608, 6, 1216, 40}, launch_.songTitle, "Loading.Song");
    // Chart line: name, difficulty value (a dash until a formula exists) and the chart creator.
    auto patternLine = launch_.patternTitle.empty() ? launch_.patternPath.stem().wstring() : launch_.patternTitle;
    patternLine += L" · —";
    if (!launch_.patternCreator.empty())
        patternLine += L" · Chart by " + launch_.patternCreator;
    auto &pattern = CreateLabel(card, {-608, -22, 1216, 24}, patternLine, "Loading.Pattern");
    const auto style = [](Visual2DNode &node, float size, std::uint16_t weight, Color color)
    {
        auto &text = *node.GetComponent<TextVisualComponent>();
        text.SetFont(ui_fonts::Text(text.Text()));
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetHorizontalAlignment(TextAlignment::Center);
        text.SetTextColor(color);
    };
    style(title, 32, 900, ui_colors::theme::Ink);
    style(pattern, 16, 700, ui_colors::theme::Caption);
    // The bar fills with the share of files ready and stays full once they are.
    auto &bar = CreateSprite(card, {-LoadingBarWidth * .5F, -48, LoadingBarWidth, LoadingBarHeight},
                             loadingImage(L"BarTrack"), "Loading.Bar");
    bar.SetClipRect({0, 0, LoadingBarWidth, LoadingBarHeight});
    loadingBarPiece_ = &CreateSprite(bar, {0, 0, 0, LoadingBarHeight}, loadingImage(L"BarFill"), "Loading.BarPiece");
    FillLoadingBar(loadingBarPiece_, 0.0F);
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
        // The loading screen shows the background at the play screen's brightness.
        const float brightness = static_cast<float>(std::clamp(launch_.backgroundBrightness, 0, 100)) / 100.0F;
        sprite.SetTint({brightness, brightness, brightness, 1});
        loadingBackgroundImage_ = true;
        sprite.SetImage(image);
        loadingBackgroundSize_ = size;
        UpdateLoadingBackgroundLayout();
    }
    catch (const std::exception &)
    {
        // The loading Canvas already has a solid-color fallback.
    }
    loadingBackgroundReady_ = true;
}
void RhythmTestScene::UpdateLoadingBackgroundLayout()
{
    if (!loadingBackground_ || !loadingCanvas_.Get()) return;
    const auto logical = loadingCanvas_.Get()->LogicalSize();
    const Point offset{static_cast<float>(launch_.backgroundOffsetX), static_cast<float>(launch_.backgroundOffsetY)};
    // Same cover rule as the play screen; without a sized image the offset does not apply.
    loadingBackground_->SetBounds(ChartBackgroundBounds(logical, loadingBackgroundSize_,
                                                        loadingBackgroundSize_.width > 0 ? offset : Point{}));
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
    const auto &session = controller_->Session();
    std::vector<HpNoteDefinition> hpNotes;
    hpNotes.reserve(session.PlayableNoteCount());
    for (const auto &lane : session.Gear().Lanes())
        for (const auto &note : lane->Notes())
            hpNotes.push_back({note->Id(), note->Accuracy().target,
                note->Profile().HalfWindow(JudgementGrade::Great),
                note->Profile().HalfWindow(JudgementGrade::Good)});
    hpGauges_.push_back(CreateHpGauge(launch_.playOptions.hpGauge, session.HealthLevel(), hpNotes));
    std::vector<IHpGauge *> borrowedGauges;
    for (const auto &gauge : hpGauges_) borrowedGauges.push_back(gauge.get());
    controller_->SetHpGauges(borrowedGauges);
    controller_->Start(*services_, view_->InitialTimelineTime());
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
    SaveLocalOffset();
    view_->SetVisible(false);
    errorLine_.Hide();
    screenFade_.Present(0.0, 0, 0);
    static_cast<void>(loadingCanvas_.SetVisible(false));
}
void RhythmTestScene::FollowSavedVolumes()
{
    if (options_ == nullptr || options_->Values().volumes == appliedVolumes_)
        return;
    appliedVolumes_ = options_->Values().volumes;
    controller_->SetVolumes(appliedVolumes_);
}

void RhythmTestScene::Update(const UpdateContext &context, SceneManager &scenes)
{
    PrepareLoadingBackground();
    FollowSavedVolumes();
    if (!presentationReady_ && (context.totalSeconds < nextPreparationPoll_ || !PreparePresentation()))
    {
        if (context.totalSeconds >= nextPreparationPoll_)
            nextPreparationPoll_ = context.totalSeconds + 0.002;
        if (controller_->SessionReady() && context.input.WasKeyPressed(VK_ESCAPE))
            controller_->OpenPauseMenu(context);
        UpdatePauseMenu(context, scenes);
        // A Retry chosen before the play screen exists still fades out first.
        static_cast<void>(controller_->UpdatePendingRetry(context));
        UpdateLoadingSettings(context);
        PresentLoadingScreen();
        PresentScreenError();
        PresentScreenFade();
        return;
    }
    UpdateLoadingSettings(context);
    controller_->Update(context);
    PresentLocalOffset(context.deltaSeconds);
    PresentLoadingScreen();
    UpdatePauseMenu(context, scenes);
    if (controller_->CurrentState() == PlayState::Gameplay)
        view_->AdvanceEffects(context.deltaSeconds);
    for (const auto &feedback : controller_->Feedback())
        view_->ApplyFeedback(feedback);
    if (controller_->CurrentState() == PlayState::Gameplay)
        view_->UpdateInputPresentation(context.input, context.deltaSeconds);
    PresentScreenError();
    PresentScreenFade();
    if (controller_->ShouldShowResult())
        ShowResult(scenes);
}
void RhythmTestScene::PresentLocalOffset(const double deltaSeconds)
{
    const auto revision = controller_->LocalOffsetRevision();
    if (revision != shownOffsetRevision_ && services_)
    {
        shownOffsetRevision_ = revision;
        const auto value = controller_->LocalOffsetMs();
        // An explicit plus for late, a typographic minus for early, no sign at zero.
        const auto sign = value > 0 ? std::wstring(L"+") : (value < 0 ? std::wstring(L"−") : std::wstring());
        const auto number = sign + std::to_wstring(value < 0 ? -value : value);
        offsetNotice_.Show(std::vformat(Gameplay(texts_).localOffsetFormat, std::make_wformat_args(number)),
                           services_->windowWidth, services_->windowHeight);
        return;
    }
    offsetNotice_.Update(deltaSeconds);
}
void RhythmTestScene::UpdateLoadingSettings(const UpdateContext &context)
{
    const auto &input = context.input;
    const bool started = controller_->AttemptStarted();
    if (attemptWasStarted_ && !started)
        loadingWait_.Begin(false);
    attemptWasStarted_ = started;
    // Only the loading screen (before the attempt's clock first runs) shows the plate, and not
    // under the pause menu; elsewhere the start is never held.
    const bool loading = !started && !controller_->PauseMenuOpen();
    if (!loading || !services_ || !launch_.IsValid())
    {
        if (controller_->PauseMenuOpen())
            loadingWait_.Interrupt();
        loadingSettings_.Hide();
        controller_->SetStartHeld(false);
        return;
    }
    loadingSettings_.Show(services_->windowWidth, services_->windowHeight);
    const auto interaction = loadingSettings_.Update(input);
    // The game window is active when this thread's window is the active one (another program takes it away).
    const LoadingStartWait::Frame frame{context.deltaSeconds, GetActiveWindow() != nullptr, interaction.holdStart};
    controller_->SetStartHeld(loadingWait_.Update(input, frame));
    if (interaction.brightness)
        ApplyBackgroundBrightness(*interaction.brightness);
    if (interaction.brightnessCommitted)
        SaveBackgroundBrightness();
    // The buttons take the same step as the -/+ keys: 5 ms, Alt 1 ms, Shift 20 ms.
    if (interaction.offsetDirection != 0)
        static_cast<void>(controller_->AdjustLocalOffset(interaction.offsetDirection *
                                                         GameplaySessionController::LocalOffsetStepMs(input)));
    loadingSettings_.SetValues(launch_.backgroundBrightness, controller_->LocalOffsetMs());
}
void RhythmTestScene::ApplyBackgroundBrightness(const int percent)
{
    launch_.backgroundBrightness = std::clamp(percent, 0, 100);
    view_->ConfigureBackgroundBrightness(launch_.backgroundBrightness);
    if (loadingBackground_ != nullptr && loadingBackgroundImage_)
    {
        const float brightness = static_cast<float>(launch_.backgroundBrightness) / 100.0F;
        loadingBackground_->GetComponent<SpriteVisualComponent>()->SetTint({brightness, brightness, brightness, 1});
    }
}
void RhythmTestScene::SaveBackgroundBrightness() noexcept
{
    if (options_ == nullptr || options_->Values().backgroundBrightness == launch_.backgroundBrightness)
        return;
    try
    {
        auto values = options_->Values();
        values.backgroundBrightness = launch_.backgroundBrightness;
        std::string error;
        if (!options_->Save(values, error))
            OutputDebugStringA(("Background brightness was not saved: " + error + "\n").c_str());
    }
    catch (const std::exception &failure)
    {
        OutputDebugStringA((std::string("Background brightness was not saved: ") + failure.what() + "\n").c_str());
    }
}
void RhythmTestScene::SaveLocalOffset() noexcept
{
    if (!controller_ || !controller_->LocalOffsetChanged() || !launch_.IsValid())
        return;
    try
    {
        std::string error;
        if (!LocalOffsetStore(LocalOffsetFilePath()).Set(launch_.patternPath, controller_->LocalOffsetMs(), error))
            OutputDebugStringA(("Local offset was not saved: " + error + "\n").c_str());
    }
    catch (const std::exception &failure)
    {
        OutputDebugStringA((std::string("Local offset was not saved: ") + failure.what() + "\n").c_str());
    }
}
void RhythmTestScene::PresentScreenFade()
{
    if (services_)
        screenFade_.Present(controller_->ScreenFade(), services_->windowWidth, services_->windowHeight);
}
void RhythmTestScene::PresentLoadingScreen()
{
    const bool loading = !controller_->AttemptStarted();
    if (presentationReady_)
        view_->SetVisible(!loading);
    static_cast<void>(loadingCanvas_.SetVisible(loading));
    if (loadingCard_)
        loadingCard_->SetVisible(!controller_->PauseMenuOpen());
    FillLoadingBar(loadingBarPiece_, LoadingFraction());
}
float RhythmTestScene::LoadingFraction() const
{
    if (presentationReady_)
        return 1.0F;
    // The sounds and images to prepare are known once the chart is read, so until then nothing is ready.
    if (!controller_->SessionReady())
        return 0.0F;
    FileProgress files{1, 1};
    files += view_->PreparedImages();
    files += controller_->LoadedAudioFiles();
    return files.Fraction();
}
void RhythmTestScene::PresentScreenError()
{
    if (!services_)
        return;
    const auto &text = Gameplay(texts_);
    ScreenErrorText line;
    switch (controller_->ErrorSource())
    {
    case GameplayErrorSource::Effect:
        line = {std::wstring(text.errorEffect),
                Utf8ToWide(controller_->Session().AutomationErrorLocation(), text.audioInitializationFailed),
                Utf8ToWide(controller_->Session().AutomationErrorCause(), text.audioInitializationFailed)};
        break;
    case GameplayErrorSource::Audio:
        line = {std::wstring(text.errorAudio), {},
                Utf8ToWide(controller_->ErrorMessage(), text.audioInitializationFailed)};
        break;
    default:
        // The chart's BG is optional: without it the screen keeps its plain colour and says why.
        if (presentationReady_ && !view_->BackgroundWarning().empty())
            line = {std::wstring(text.errorBackground), launch_.backgroundPath.wstring(),
                    Utf8ToWide(view_->BackgroundWarning(), text.audioInitializationFailed)};
        break;
    }
    errorLine_.Present(line, services_->windowWidth, services_->windowHeight);
}
void RhythmTestScene::UpdatePauseMenu(const UpdateContext &context, SceneManager &scenes)
{
    if (!controller_->PauseMenuOpen())
    {
        pauseMenu_->Hide();
        return;
    }
    pauseMenu_->SetPreparationFailed(controller_->PreparationFailed());
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
    {
        const auto hp = hpGauges_.front()->State();
        view_->UpdateHp({hp.percent, hp.clearPercent, controller_->HpDisabled()});
        view_->UpdatePresentation(controller_->Time());
    }
}
void RhythmTestScene::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (services_)
    {
        services_->windowWidth = width;
        services_->windowHeight = height;
    }
    view_->OnResize(width, height);
    errorLine_.OnResize(width, height);
    screenFade_.OnResize(width, height);
    offsetNotice_.OnResize(width, height);
    loadingSettings_.OnResize(width, height);
    UpdateLoadingBackgroundLayout();
    pauseMenu_->OnResize(width, height);
}
void RhythmTestScene::Shutdown() noexcept
{
    pauseMenu_->Shutdown();
    errorLine_.Shutdown();
    screenFade_.Shutdown();
    offsetNotice_.Shutdown();
    loadingSettings_.Shutdown();
    loadingBackground_ = nullptr;
    loadingCard_ = nullptr;
    loadingBarPiece_ = nullptr;
    loadingBackgroundSize_ = {};
    loadingCanvas_.Reset();
    loadingBackgroundReady_ = false;
    view_->Shutdown(); // Release session observers before the session.
    controller_->Shutdown();
    hpGauges_.clear();
    services_.reset();
    presentationReady_ = false;
    nextPreparationPoll_ = 0;
}
