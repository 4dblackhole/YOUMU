#pragma once
#include "PhantomCore.h"
#include "Mode/Results/IPlayResult.h"
#include "Presentation/UiAlert.h"
#include "Submodules/IResultView.h"
#include "Texts/TextCatalog.h"
#include <filesystem>
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
    // The record failure alert, for regression tests.
    [[nodiscard]] const UiAlert &RecordAlert() const noexcept
    {
        return recordAlert_;
    }

  private:
    void StartRecordWrite();
    void PollRecordWrite();
    void UpdateRecordStatus();
    void UpdateRecordAlert(const PhantomCore::InputState &input);

    // The view observes immutable data owned by this Scene. It is destroyed first.
    std::unique_ptr<IPlayResult> result_;
    std::unique_ptr<IResultView> view_;
    RetryHandler retry_;
    std::future<std::string> recordWrite_;
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    PhantomCore::ScreenCanvasHandle recordStatusCanvas_;
    PhantomCore::Visual2DNode *recordStatus_{};
    std::filesystem::path recordPath_;
    std::string recordError_;
    // A failed save asks on the record status Canvas, above the result, until it is answered.
    UiAlert recordAlert_;
    PhantomCore::Visual2DInputRouter recordAlertInput_;
    std::uint32_t width_{1280}, height_{720};
};
