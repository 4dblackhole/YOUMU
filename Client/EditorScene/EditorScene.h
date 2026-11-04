#pragma once
#include "GameFlow/GameplayLaunchStore.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <memory>

class EditorWorkspace;
class EditorView;
class EditorWorkspaceStore;

class EditorScene final : public GameScene
{
  public:
    EditorScene(ScreenVisual2DManager &visuals,
                std::shared_ptr<GameplayLaunchStore> request,
                TextCatalog &texts, std::shared_ptr<EditorWorkspaceStore> workspaceStore);
    ~EditorScene() override;
    void Initialize(const EngineServices &services) override;
    void Update(const UpdateContext &context, SceneManager &scenes) override;
    void Render(const RenderContext &) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    std::shared_ptr<GameplayLaunchStore> launchStore_;
    std::shared_ptr<EditorWorkspaceStore> workspaceStore_;
    std::unique_ptr<EditorWorkspace> workspace_;
    std::unique_ptr<EditorView> view_;
    bool testStarting_{};
};
