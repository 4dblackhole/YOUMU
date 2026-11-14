#pragma once
#include "GameScene/ResultScene/Submodules/IResultView.h"
#include "Taiko/Results/TaikoPlayResult.h"
#include "TaikoResultLayout.h"
#include "TaikoResultRing.h"
#include "Texts/TextCatalog.h"
namespace taiko_result_ui
{
    struct GraphData;
}
// Redesigned result screen (UI-02): banner, left HP/summary/judgement column,
// centre rank ring with the judgement share, right Timing graphs and a bottom bar.
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
    struct TextStyle
    {
        float size{13};
        std::uint16_t weight{400};
        PhantomCore::Color color{ui_colors::theme::Ink};
        PhantomCore::TextAlignment alignment{PhantomCore::TextAlignment::Leading};
    };
    void Build();
    void LoadAssets();
    void BuildBackground();
    void BuildSongBanner(PhantomCore::Visual2DNode &root);
    void BuildSummary(PhantomCore::Visual2DNode &root);
    void BuildHpGraph(PhantomCore::Visual2DNode &root);
    void BuildJudgements(PhantomCore::Visual2DNode &root);
    void BuildGroups(PhantomCore::Visual2DNode &root);
    void BuildHero(PhantomCore::Visual2DNode &root);
    void BuildOptions(PhantomCore::Visual2DNode &root);
    void BuildGraphs(PhantomCore::Visual2DNode &root);
    void BuildTimingStatistics(PhantomCore::Visual2DNode &root);
    void BuildGraphLabels(PhantomCore::Visual2DNode &root, const taiko_result_ui::GraphData &data);
    void BuildActions(PhantomCore::Visual2DNode &root);
    void UpdateRingTooltip(std::optional<PhantomCore::Point> pointer);
    // Latin captions and user text. The face follows the string (Nunito or Noto Sans KR).
    PhantomCore::Visual2DNode &Label(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                       std::wstring text, TextStyle style);
    // Numbers use the condensed numeric face; italic is the default numeric look.
    PhantomCore::Visual2DNode &NumberLabel(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                             std::wstring text, TextStyle style, bool italic = true);
    PhantomCore::Visual2DNode &Fill(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                      PhantomCore::Color color, std::string name = "Result.Fill");
    PhantomCore::Visual2DNode &Image(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                       PhantomCore::ImageHandle image, std::string name,
                                       PhantomCore::Color tint = {1, 1, 1, 1});
    PhantomCore::Visual2DNode &Button(PhantomCore::Visual2DNode &root, PhantomCore::Rect designBounds,
                                        std::string name, std::wstring title, float fontSize,
                                        PhantomCore::Color textColor, PhantomCore::ImageHandle icon,
                                        float contentLeft);
    void ProcessPointer(const PhantomCore::InputState &input);
    const TaikoPlayResult &result_;
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    ResultViewContext context_;
    PhantomCore::ImageHandle sky_, leftScrim_, rightScrim_, banner_, bottomBar_, modeBadge_, modeIcon_;
    PhantomCore::ImageHandle rankRing_, rankImage_;
    PhantomCore::ImageHandle backIcon_, replayIcon_, retryIcon_, creatorIcon_;
    PhantomCore::ImageHandle chartBackground_;
    PhantomCore::Size chartBackgroundSize_{};
    PhantomCore::ScreenCanvasHandle canvasHandle_;
    PhantomCore::Visual2DCanvas *canvas_{};
    PhantomCore::Visual2DInputRouter router_;
    PhantomCore::NodeId contentId_{}, backId_{}, retryId_{}, backgroundId_{}, frameId_{}, tooltipId_{};
    taiko_result_ui::layout::Columns columns_{};
    taiko_result_ui::JudgementRing ring_;
    // Content root placement: Canvas position of its bottom-left corner and its scale.
    PhantomCore::Point contentOrigin_{};
    float contentScale_{1};
    std::optional<std::size_t> hoveredSlice_;
    std::uint64_t textRevision_{};
    std::uint32_t width_{}, height_{};
};
