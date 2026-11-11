#pragma once
#include "PhantomCore.h"
#include "Mode/Results/IPlayResult.h"
#include "Submodules/IResultView.h"
#include "Texts/TextCatalog.h"
#include <functional>
#include <memory>
#include <future>
class IResultView;
class ResultScene final : public PhantomCore::GameScene
{
  public:
    using RetryHandler = std::function<bool(PhantomCore::SceneManager &)>;
    ResultScene(std::unique_ptr<IPlayResult> result, PhantomCore::ScreenVisual2DManager &visuals,
                TextCatalog &texts, ResultViewContext context = {}, RetryHandler retry = {});
    ~ResultScene() override;
    void Initialize(const PhantomCore::EngineServices &services) override;
    void BeginScene() override;
    void EndScene() noexcept override;
    void Update(const PhantomCore::UpdateContext &context, PhantomCore::SceneManager &scenes) override;
    void Render(const PhantomCore::RenderContext &context) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    // The view observes immutable data owned by this Scene. It is destroyed first.
    std::unique_ptr<IPlayResult> result_;
    std::unique_ptr<IResultView> view_;
    RetryHandler retry_;
    std::future<std::string> recordWrite_;
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    PhantomCore::ScreenCanvasHandle recordStatusCanvas_;
    PhantomCore::Visual2DNode *recordStatus_{};
    std::string recordError_;
};
