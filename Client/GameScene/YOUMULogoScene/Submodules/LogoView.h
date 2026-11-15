#pragma once

#include "PhantomCore.h"
#include "Presentation/OptionsPanel.h"
#include "Presentation/UiDropdown.h"
#include "Presentation/UiTooltip.h"
#include "Texts/TextCatalog.h"
#include "TitlePlayerView.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

// One frame's intents: a chosen menu item and/or a music player request.
struct LogoCommand
{
    std::optional<std::size_t> menu;
    std::optional<TitlePlayerRequest> player;
};

// Presents the YOUMU title screen: a sky with drifting feathers, the music player and the main menu.
// One Canvas tree owns the mouse/keyboard menu, its selection cursor and the player's controls.
class LogoView final
{
  public:
    LogoView(PhantomCore::ScreenVisual2DManager &screenVisuals,
             TextCatalog &texts, OptionSettings &options) noexcept;

    void Initialize(const PhantomCore::EngineServices &services);
    void BeginScene();
    void EndScene() noexcept;
    // Shows the shared song library's current song; call before Update each frame.
    void SetPlayer(const TitlePlayerDisplay &display);
    LogoCommand Update(const PhantomCore::InputState &input, double deltaSeconds);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    LogoCommand command_;
    void CreateSky();
    void CreateMenu();
    void ApplyTexts();
    void UpdateLayout();
    void LayoutMenu();
    void AnimateFeathers(double deltaSeconds);
    void RefreshSkinImages();
    void ProcessPointer(const PhantomCore::InputState &input);
    void UpdateSelectionFromPointer(const PhantomCore::InputState &input);
    void HandleKeyboard(const PhantomCore::InputState &input);
    void HandlePlayerKeyboard(const PhantomCore::InputState &input);
    [[nodiscard]] bool ApplyMenuActions();
    void SetSelectedMenuItem(std::size_t index);
    void ActivateMenuItem(std::size_t index);
    [[nodiscard]] static bool HasPointerActivity(const PhantomCore::InputState &input) noexcept;
    [[nodiscard]] static std::int64_t LatestPointerTimestamp(
        const PhantomCore::InputState &input) noexcept;

    std::uint32_t width_{1280};
    std::uint32_t height_{720};
    PhantomCore::ScreenVisual2DManager &screenVisuals_;
    TextCatalog &texts_;
    OptionsPanel options_;
    UiTooltip tooltip_;
    std::uint64_t textRevision_{};
    std::uint64_t skinRevision_{};
    PhantomCore::ScreenCanvasHandle canvasHandle_;
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DInputRouter inputRouter_;
    std::optional<PhantomCore::Point> canvasPointer_;
    PhantomCore::Visual2DNode *board_{};
    PhantomCore::Visual2DNode *background_{};
    std::array<PhantomCore::Visual2DNode *, 7> feathers_{};
    std::array<PhantomCore::Visual2DNode *, 4> letterbox_{};
    double featherSeconds_{};
    std::array<PhantomCore::Visual2DNode *, 4> menuButtons_{};
    std::array<PhantomCore::NodeId, 4> menuButtonIds_{};
    PhantomCore::Visual2DNode *selectionCursor_{};
    std::size_t selectedMenuIndex_{};
    TitlePlayerView player_;
};
