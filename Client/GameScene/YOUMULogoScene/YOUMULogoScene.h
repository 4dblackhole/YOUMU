#pragma once

#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include "App/OptionSettings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

// Presents the YOUMU title screen. The authored image panels remain
// centered at every viewport aspect ratio, while one Canvas tree owns the
// mouse/keyboard menu, its movable selection cursor and the music player.
// The player plays the song library the game song select shares.
class LogoView;
class SongLibrary;
struct TitlePlayerRequest;
class YOUMULogoScene final : public PhantomCore::GameScene
{
  public:
    YOUMULogoScene(PhantomCore::ScreenVisual2DManager &screenVisuals,
                        TextCatalog &texts,
                        OptionSettings &options,
                        std::shared_ptr<SongLibrary> library);

    ~YOUMULogoScene() override;
    void Initialize(const PhantomCore::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void ApplyPlayerRequest(const TitlePlayerRequest &request);
    void ShowPlayer();
    void RunMenuItem(std::size_t index, PhantomCore::SceneManager &scenes);

    std::shared_ptr<SongLibrary> library_;
    OptionSettings &options_; // The title player shows the song in the chosen notation.
    std::unique_ptr<LogoView> view_;
    // Game Start hands the song to song select; every other exit silences it.
    bool keepPlaybackOnExit_{};
};
