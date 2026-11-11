#pragma once
#include "GameScene/ResultScene/Submodules/IResultView.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "Texts/TextCatalog.h"
namespace taiko_result_ui
{
    struct GraphData;
}
class TaikoResultView final : public IResultView
{
  public:
    TaikoResultView(const TaikoPlayResult &result, PhantomCore::ScreenVisual2DManager &visuals,
                    TextCatalog &texts, ResultViewContext context = {});
    ~TaikoResultView() override;
    void Initialize(const PhantomCore::EngineServices &services) override;
    void SetVisible(bool visible) noexcept override;
    ResultAction Update(const PhantomCore::InputState &input) override;
    void OnResize(std::uint32_t width, std::uint32_t height) override;
    void Shutdown() noexcept override;

  private:
    void Build();
    void LoadAssets();
    void BuildBackground();
    void BuildSongBanner(PhantomCore::Visual2DNode &root);
    void BuildSummary(PhantomCore::Visual2DNode &root);
    void BuildHpGraph(PhantomCore::Visual2DNode &root);
    void BuildJudgements(PhantomCore::Visual2DNode &root);
    void BuildGroups(PhantomCore::Visual2DNode &root);
    void BuildMetadata(PhantomCore::Visual2DNode &root);
    void BuildOptions(PhantomCore::Visual2DNode &root);
    void BuildGraphs(PhantomCore::Visual2DNode &root);
    void BuildHistogramLabels(PhantomCore::Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildScatterLabels(PhantomCore::Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildGraphLegend(PhantomCore::Visual2DNode &root);
    void BuildActions(PhantomCore::Visual2DNode &root);
    PhantomCore::Visual2DNode &Label(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                       std::wstring text, float size, std::uint16_t weight = 500,
                                       PhantomCore::TextAlignment alignment = PhantomCore::TextAlignment::Leading,
                                       PhantomCore::Color color = {40.0F / 255, 83.0F / 255, 116.0F / 255, 1});
    void Panel(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds, PhantomCore::Color fill,
               float radius, float border = 0,
               PhantomCore::Color borderColor = {79.0F / 255, 159.0F / 255, 223.0F / 255, 1});
    PhantomCore::Visual2DNode &Button(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                        std::string name, std::wstring title, bool outlined);
    void ProcessPointer(const PhantomCore::InputState &input);
    const TaikoPlayResult &result_;
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    ResultViewContext context_;
    PhantomCore::TextFont font_;
    PhantomCore::ImageHandle sky_, clouds_, modeIcon_, rankFrame_, rankImage_;
    PhantomCore::ImageHandle chartBackground_;
    PhantomCore::Size chartBackgroundSize_{};
    PhantomCore::ScreenCanvasHandle canvasHandle_;
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DInputRouter router_;
    PhantomCore::NodeId contentId_{}, backId_{}, retryId_{}, backgroundId_{}, footerId_{};
    std::uint64_t textRevision_{};
    std::uint32_t width_{}, height_{};
};
