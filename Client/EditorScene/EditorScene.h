#pragma once
#include "App/OptionSettings.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <memory>

class EditorWorkspace;
class EditorView;

class EditorScene final : public PhantomCore::GameScene
{
  public:
    EditorScene(PhantomCore::ScreenVisual2DManager &visuals,
                std::shared_ptr<GameplayLaunchStore> request,
                TextCatalog &texts, const OptionSettings *options = nullptr);
    ~EditorScene() override;
    void Initialize(const PhantomCore::EngineServices &services) override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<EditorWorkspace> workspace_;
    std::unique_ptr<EditorView> view_;
    // Volume changes saved while editing (Alt+wheel) reach the playback; it started with the launch request's.
    const OptionSettings *options_{};
    AudioVolumes appliedVolumes_;
};
