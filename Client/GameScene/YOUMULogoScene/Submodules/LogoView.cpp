#include "LogoView.h"
#include "App/AssetPaths.h"
#include "LogoStyle.h"
#include "Texts/GameScene/YOUMULogoScene/LogoTexts.h"
#include <limits>
using namespace PhantomCore;

using namespace logo_ui;

LogoView::LogoView(ScreenVisual2DManager &screenVisuals,
                   TextCatalog &texts, OptionSettings &options) noexcept
    : screenVisuals_(screenVisuals), texts_(texts), options_(texts, options, &screenVisuals), player_(screenVisuals)
{
}

void LogoView::Initialize(const EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;

    // FixedHeight keeps the title measured against a 720-unit vertical
    // reference while expanding the logical width for wider screens.
    canvasHandle_ = screenVisuals_.CreateOwnedCanvas(
        {{1280.0F, 720.0F}, CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (canvas_ == nullptr)
    {
        throw std::runtime_error("Failed to create the logo screen Canvas.");
    }

    CreateSky();
    CreateMenu();
    player_.Create(*board_);
    skinRevision_ = (std::numeric_limits<std::uint64_t>::max)();
    RefreshSkinImages();
    options_.Initialize(*canvas_, services.audio, services.windowControl);
    tooltip_.Initialize(*canvas_);
    ApplyTexts();
    UpdateLayout();
}

void LogoView::BeginScene()
{
    RefreshSkinImages();
    if (textRevision_ != texts_.Revision())
    {
        ApplyTexts();
    }
    static_cast<void>(canvasHandle_.SetVisible(true));
}

void LogoView::EndScene() noexcept
{
    tooltip_.Hide();
    if (canvas_ != nullptr)
        CollapseDropdowns(*canvas_);
    static_cast<void>(canvasHandle_.SetVisible(false));
}

void LogoView::SetPlayer(const TitlePlayerDisplay &display)
{
    player_.SetDisplay(display);
}

LogoCommand LogoView::Update(const InputState &input, const double deltaSeconds)
{
    command_ = {};
    if (!canvas_)
        return command_;
    if (ProcessDropdownKeys(*canvas_, input))
    {
        tooltip_.Hide();
        return command_;
    }
    RefreshSkinImages();
    if (textRevision_ != texts_.Revision())
        ApplyTexts();
    options_.Update(deltaSeconds);
    AnimateFeathers(deltaSeconds);
    const bool controlDown =
        input.IsKeyDown(VK_LCONTROL) || input.IsKeyDown(VK_RCONTROL);
    if (!options_.OwnsKeyboard() && controlDown && input.WasKeyPressed(static_cast<std::uint16_t>('O')))
        options_.Toggle();
    ProcessPointer(input);
    tooltip_.Update(deltaSeconds, input, inputRouter_.HoveredNode(), !options_.IsVisible());
    if (options_.ProcessInput(input, canvasPointer_, inputRouter_.HoveredNode(), inputRouter_))
    {
        player_.CancelSeek();
        static_cast<void>(canvas_->TakeActions());
        return command_;
    }
    const bool handled = ApplyMenuActions();
    if (options_.IsVisible())
    {
        player_.CancelSeek();
        return command_;
    }
    if (auto seek = player_.ProcessSeekPointer(input, canvasPointer_, inputRouter_.HoveredNode()))
        command_.player = seek;
    UpdateSelectionFromPointer(input);
    if (!handled)
        HandleKeyboard(input);
    return command_;
}

void LogoView::OnResize(const std::uint32_t width, const std::uint32_t height)
{
    width_ = width;
    height_ = height;
    if (canvas_ == nullptr)
    {
        return;
    }

    UpdateLayout();
    inputRouter_.InvalidateHitTest();
}

void LogoView::Shutdown() noexcept
{
    if (canvas_ != nullptr)
    {
        inputRouter_.Reset(*canvas_);
    }

    tooltip_.Shutdown();
    options_.Shutdown();
    player_.Reset();
    canvasPointer_.reset();
    // Nodes are owned by the Canvas. Clear observers before its tree is
    // destroyed so this Scene never retains dangling node pointers.
    selectionCursor_ = nullptr;
    menuButtons_ = {};
    menuButtonIds_ = {};
    feathers_ = {};
    letterbox_ = {};
    background_ = nullptr;
    board_ = nullptr;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}
