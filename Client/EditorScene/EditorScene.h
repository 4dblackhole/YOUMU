#pragma once
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
                TextCatalog &texts);
    ~EditorScene() override;
    void Initialize(const PhantomCore::EngineServices &services) override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::unique_ptr<EditorWorkspace> workspace_;
    std::unique_ptr<EditorView> view_;
};
