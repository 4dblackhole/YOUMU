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
// mouse/keyboard menu and its movable selection cursor.
class LogoView;
class YOUMULogoScene final : public PhantomCore::GameScene
{
  public:
    YOUMULogoScene(PhantomCore::ScreenVisual2DManager &screenVisuals,
                        TextCatalog &texts,
                        OptionSettings &options);

    ~YOUMULogoScene() override;
    void Initialize(const PhantomCore::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<LogoView> view_;
};
