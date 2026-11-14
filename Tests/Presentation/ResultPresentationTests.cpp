#include <Windows.h>
#include "YOUMUAssets.h"
#include "GameFlow/YOUMUSceneIds.h"
#include "GameScene/ResultScene/ResultScene.h"
#include "GameScene/ResultScene/Submodules/ResultViewFactory.h"
#include "GameScene/ResultScene/Submodules/Taiko/TaikoResultGraphs.h"
#include "GameScene/ResultScene/Submodules/Taiko/TaikoResultLayout.h"
#include "GameScene/ResultScene/Submodules/Taiko/TaikoResultRing.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Presentation/AccuracyText.h"
#include "Taiko/TaikoMode.h"
#include "Records/PlayRecordStore.h"
#include <thread>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <stdexcept>

using namespace PhantomCore;

namespace
{

    void Check(bool condition, const char *message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "CHECK FAILED: %s\n", message);
            throw std::runtime_error(message);
        }
    }
    class NullRenderer final : public Visual2DRenderSystem
    {
      public:
        ImageHandle LoadImageFile(const std::filesystem::path &path) override
        {
            if (path.filename() == "chart-bg-fail.png") throw std::runtime_error("BG decode/upload failure fixture.");
            if (path.filename() == "chart-bg-zero.png")
            {
                const auto id = ++imageId_;
                images_.emplace(id, LoadedImage{Size{}, path});
                return {id};
            }
            std::ifstream file(path, std::ios::binary);
            std::array<unsigned char, 24> header{};
            file.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
            Check(file.gcount() == static_cast<std::streamsize>(header.size()) && header[0] == 137 &&
                      header[1] == 'P' && header[2] == 'N' && header[3] == 'G',
                  "Result assets must resolve to real embedded/fallback PNGs.");
            const auto dimension = [&header](const std::size_t offset)
            {
                return (static_cast<std::uint32_t>(header[offset]) << 24) |
                       (static_cast<std::uint32_t>(header[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(header[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(header[offset + 3]);
            };
            const auto id = ++imageId_;
            images_.emplace(id,
                            LoadedImage{{static_cast<float>(dimension(16)), static_cast<float>(dimension(20))}, path});
            return {id};
        }
        Size GetImageSize(ImageHandle image) const noexcept override
        {
            const auto found = images_.find(image.value);
            return found == images_.end() ? Size{} : found->second.size;
        }
        const std::filesystem::path &ImagePath(ImageHandle image) const
        {
            return images_.at(image.value).path;
        }
        void SubmitScreen(const Visual2DCanvas &, const RenderContext &, Point,
                          std::uint32_t) override
        {
        }
        void SubmitPlane(const Visual2DCanvas &, const RenderContext &, const Float4x4 &,
                         Size, const Float4x4 &) override
        {
        }
        RenderTargetTextureHandle CreateCanvasRenderTarget(std::uint32_t, std::uint32_t) override
        {
            return {};
        }
        void RenderToTexture(const Visual2DCanvas &, const RenderTargetTextureHandle &,
                             const RenderContext &) override
        {
        }

      private:
        struct LoadedImage
        {
            Size size;
            std::filesystem::path path;
        };
        std::map<std::uint64_t, LoadedImage> images_;
        std::uint64_t imageId_{};
    };
    bool HasText(const Visual2DCanvas &canvas, std::wstring_view text)
    {
        const auto packets = canvas.BuildDrawList();
        return std::ranges::any_of(packets, [&](const auto &packet)
                                   { return packet.text && packet.text->value.find(text) != std::wstring::npos; });
    }
    const Visual2DNode *FindNode(const Visual2DNode &node, std::string_view name)
    {
        if (node.Name() == name)
            return &node;
        for (const auto &child : node.Children())
            if (const auto *found = FindNode(*child, name))
                return found;
        return nullptr;
    }
    const Visual2DNode *FindText(const Visual2DNode &node, std::wstring_view value)
    {
        const auto *text = node.GetComponent<TextVisualComponent>();
        if (text && text->Text() == value)
            return &node;
        for (const auto &child : node.Children())
            if (const auto *found = FindText(*child, value))
                return found;
        return nullptr;
    }
    void TestFailureOverlay()
    {
        struct Case
        {
            PlayEndReason reason;
            HpGaugeState hp;
            bool paused{}, rewound{}, showFailure{}, hpAvailable{true};
        };
        const std::array cases{
            Case{PlayEndReason::Completed, {60, true, true}},
            Case{PlayEndReason::Completed, {59.999999, true, false}, false, false, true},
            Case{PlayEndReason::Interrupted, {90, true, true}},
            Case{PlayEndReason::Interrupted, {0, true, false}},
            Case{PlayEndReason::Completed, {30, true, false}, true},
            Case{PlayEndReason::Completed, {30, true, false}, false, true},
            Case{PlayEndReason::Completed, {0, false, false}},
            Case{PlayEndReason::Completed, {}, false, false, false, false},
        };
        for (const auto &fixture : cases)
        {
            NullRenderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1920, 1080});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            TaikoPlayResult result;
            result.summary.totalNotes = 1;
            result.summary.endReason = fixture.reason;
            result.summary.endTime = RhythmTime{4'000'000};
            result.summary.pauseMenuUsed = fixture.paused;
            if (fixture.hpAvailable) result.summary.hpGauges.push_back(fixture.hp);
            result.summary.hpHistory = std::vector<HpHistoryPoint>{{RhythmTime{1'000'000}, fixture.hp.percent}};
            if (fixture.rewound) result.summary.controls.push_back({PlayControlType::Rewind, RhythmTime{2'000'000}, {}});
            auto view = CreateResultView(result, visuals, texts);
            view->Initialize({meshes, textRendering, renderer, audio, 1920, 1080});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Failure-overlay Canvas missing.");
            for (const auto language : {Language::Korean, Language::English})
            {
                texts.SetLanguage(language);
                InputState input;
                static_cast<void>(view->Update(input));
                const auto *label = FindNode(canvas->Root(), "Result.Failure");
                Check((label != nullptr) == fixture.showFailure,
                      "Only an HP-eligible completed failure is labelled; clear, retire and HP-disabled plays are not.");
                Check(!FindNode(canvas->Root(), "Result.EndStatus"), "Do not add a separate end-state display.");
                if (label)
                {
                    const auto *text = label->GetComponent<TextVisualComponent>();
                    if (!text) throw std::runtime_error("Failure caption missing.");
                    Check(text->Text() == texts.Get("Result.failed"), "Failure caption follows the selected language.");
                    const auto b = label->Bounds();
                    Check(b.x >= 32 && b.x + b.width <= 372 && b.y >= 720 - 130 && b.y + b.height <= 720 - 96,
                          "Failure sits in the HP title row above the chart so it never covers the curve.");
                }
                if (fixture.hpAvailable)
                    Check(result.summary.hpGauges.front().percent == fixture.hp.percent,
                          "Displaying status must preserve actual HP, including high-HP retirement.");
                Check(result.summary.hpHistory->back().percent == fixture.hp.percent,
                      "Failure/retirement display never rewrites the HP curve.");
            }
            view->Shutdown();
        }
    }
    void TestControlUsageOverlay()
    {
        for (unsigned mask = 0; mask < 5; ++mask)
        {
            NullRenderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1920, 1080});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            TaikoPlayResult result;
            result.summary.pauseMenuUsed = (mask & 1U) != 0;
            result.summary.endTime = RhythmTime{4'000'000};
            result.summary.hpGauges = {{60, true, true}};
            result.summary.hpHistory = std::vector<HpHistoryPoint>{{RhythmTime{500'000}, 30}, {RhythmTime{1'000'000}, 60}};
            if ((mask & 1U) != 0)
                result.summary.controls.push_back({PlayControlType::Pause, RhythmTime{1'500'000}, RhythmTime{1'500'000}, 0, true});
            if ((mask & 2U) != 0)
                result.summary.controls.push_back({PlayControlType::Rewind, RhythmTime{2'000'000}, {}});
            if (mask == 4)
            {
                result.summary.controls.push_back({PlayControlType::Pause, {}, {}, 0, false});
                result.summary.controls.push_back({PlayControlType::Resume, RhythmTime{1'000'000}, {}});
            }
            auto view = CreateResultView(result, visuals, texts);
            view->Initialize({meshes, textRendering, renderer, audio, 1920, 1080});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Usage-overlay Canvas missing.");
            for (const auto language : {Language::Korean, Language::English})
            {
                texts.SetLanguage(language);
                InputState input;
                static_cast<void>(view->Update(input));
                const auto *overlay = FindNode(canvas->Root(), "Result.ControlUsage");
                Check((overlay != nullptr) == (mask > 0 && mask < 4),
                      "Only actual menu pause or Rewind adds an overlay; Debug pause and Resume do not.");
                Check(HasText(*canvas, texts.Get("Result.hp")) && FindNode(canvas->Root(), "Result.HpChart"),
                      "Adding usage must preserve the HP chart and its title.");
                Check(!HasText(*canvas, texts.Get("Result.noHpData")) &&
                          !taiko_result_ui::BuildHpCurve(result.summary, {122, 806, 426, 104}).empty(),
                      "The actual HP curve remains visible with and without usage overlays.");
                if (!overlay) continue;
                std::wstring expected;
                if ((mask & 1U) != 0) expected = texts.Get("Result.pauseUsed");
                if ((mask & 2U) != 0)
                {
                    if (!expected.empty()) expected += L" · ";
                    expected += texts.Get("Result.rewindUsed");
                }
                const auto *text = overlay->GetComponent<TextVisualComponent>();
                if (!text) throw std::runtime_error("Usage-overlay text missing.");
                Check(text->Text() == expected, "Used actions appear together in the selected language.");
                const auto bounds = overlay->Bounds();
                Check(bounds.x >= 32 && bounds.x + bounds.width <= 372.01F &&
                          bounds.y >= 720 - 130 && bounds.y + bounds.height <= 720 - 96,
                      "Usage sits in the HP title row, outside the plot.");
            }
            view->Shutdown();
        }
    }
    void TestHpCurveGeometry()
    {
        PlayResultSummary summary;
        summary.endTime = RhythmTime{4'000'000};
        summary.hpGauges = {{75, true, true}};
        const Rect plot{10, 20, 400, 100};
        Check(taiko_result_ui::BuildHpCurve(summary, plot).empty(), "Unavailable HP history is never invented.");
        summary.hpHistory.emplace();
        Check(!taiko_result_ui::BuildHpCurve(summary, plot).empty(), "An empty recording has a valid zero-start curve.");
        *summary.hpHistory = {{RhythmTime{1'000'000}, 25}, {RhythmTime{2'000'000}, 75}};
        auto curve = taiko_result_ui::BuildHpCurve(summary, plot);
        const auto right = [](const auto &rectangles)
        {
            float result = 0;
            for (const auto &rectangle : rectangles)
                result = std::max(result, rectangle.bounds.x + rectangle.bounds.width);
            return result;
        };
        Check(std::abs(right(curve) - 410) < .01F, "Active HP continues to normal play end.");
        summary.controls.push_back({PlayControlType::Pause, RhythmTime{2'500'000}, RhythmTime{2'500'000}, 0, true});
        summary.controls.push_back({PlayControlType::Rewind, RhythmTime{2'500'000}, RhythmTime{500'000}});
        curve = taiko_result_ui::BuildHpCurve(summary, plot);
        Check(std::abs(right(curve) - 260) < 1.01F, "HP history ends at deactivation and survives Rewind.");
        for (const auto &rectangle : curve)
        {
            const auto b = rectangle.bounds;
            Check(std::isfinite(b.x) && std::isfinite(b.y) && b.width > 0 && b.height > 0 &&
                      b.x >= plot.x && b.y >= plot.y && b.x + b.width <= 410.01F && b.y + b.height <= 120.01F,
                  "HP geometry stays finite and inside the original plot.");
        }
        summary.hpGauges.front().applicable = false;
        Check(taiko_result_ui::BuildHpCurve(summary, plot).empty(), "Empty/editor-only charts have no HP curve.");
    }
    void TestChartBackground()
    {
        for (const auto &path : std::array<std::filesystem::path, 5>{
            ResolveDefaultSkinAsset(L"Result/Rank/SS.png"), ResolveDefaultSkinAsset(L"InGame/InputPanel.png"),
            std::filesystem::path{"chart-bg-fail.png"}, std::filesystem::path{"chart-bg-zero.png"}, {}})
        {
            const bool assigned = !path.empty() && path.filename() != "chart-bg-fail.png" && path.filename() != "chart-bg-zero.png";
            NullRenderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1920, 1080});
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts("Client/Texts/Languages");
            TaikoPlayResult result;
            ResultViewContext context;
            context.backgroundPath = path;
            context.backgroundOffsetX = 70;
            context.backgroundOffsetY = -25;
            auto view = CreateResultView(result, visuals, texts, context);
            context.backgroundPath = "later-selection.png";
            context.backgroundOffsetX = 999;
            view->Initialize({meshes, textRendering, renderer, audio, 1920, 1080});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas) throw std::runtime_error("Chart result Canvas missing.");
            for (const auto viewport : std::array<Size, 3>{{{1920, 1080}, {2560, 1080}, {900, 720}}})
            {
                visuals.OnResize(static_cast<std::uint32_t>(viewport.width), static_cast<std::uint32_t>(viewport.height));
                view->OnResize(static_cast<std::uint32_t>(viewport.width), static_cast<std::uint32_t>(viewport.height));
                const auto *background = FindNode(canvas->Root(), "Result.Sky");
                if (!background) throw std::runtime_error("Result background missing.");
                const auto *sprite = background->GetComponent<SpriteVisualComponent>();
                if (!sprite) throw std::runtime_error("Result background sprite missing.");
                const auto &loaded = renderer.ImagePath(sprite->Style().normalImage);
                Check(loaded == (assigned ? path : ResolveDefaultSkinAsset(L"Result/Background/Sky.png")),
                      "Result uses the played chart BG snapshot, with skin fallback for absent or failed images.");
                const auto size = canvas->LogicalSize();
                const auto bounds = background->BoundsInCanvas();
                Check(bounds.x <= -size.width / 2 + .01F && bounds.x + bounds.width >= size.width / 2 - .01F &&
                          bounds.y <= -size.height / 2 + .01F && bounds.y + bounds.height >= size.height / 2 - .01F,
                      "Result BG covers narrow and wide viewports without exposing edges.");
                if (assigned)
                {
                    const auto native = renderer.GetImageSize(sprite->Style().normalImage);
                    Check(std::abs(bounds.width / bounds.height - native.width / native.height) < .001F &&
                              std::abs(bounds.x + bounds.width / 2 - 70) < .01F &&
                              std::abs(bounds.y + bounds.height / 2 + 25) < .01F,
                          "Result preserves image aspect and applies the 720-height authored offsets on its 720-height Canvas.");
                }
                const auto packets = canvas->BuildDrawList();
                Check(!packets.empty() && packets.front().image.value == sprite->Style().normalImage.value,
                      "Selected BG reaches the draw list behind result UI.");
            }
            texts.SetLanguage(Language::English);
            InputState input;
            static_cast<void>(view->Update(input));
            const auto *background = FindNode(canvas->Root(), "Result.Sky");
            if (!background) throw std::runtime_error("Rebuilt result BG missing.");
            Check(renderer.ImagePath(background->GetComponent<SpriteVisualComponent>()->Style().normalImage) ==
                      (assigned ? path : ResolveDefaultSkinAsset(L"Result/Background/Sky.png")),
                  "Language rebuild retains the original result background snapshot.");
            view->Shutdown();
            Check(visuals.CanvasCount() == 0, "Result BG releases its Canvas without retaining gameplay objects.");
        }
    }
    void TestAccuracyText()
    {
        const auto text = [](std::uint32_t units) { return AccuracyText(AccuracyScore::FromUnits(units)); };
        Check(text(100'000'000) == L"100.00%" && text(99'999'999) == L"99.99%" && text(99'990'000) == L"99.99%" &&
                  text(99'989'999) == L"99.98%" && text(5'009'999) == L"5.00%" && text(9'999) == L"0.00%" &&
                  text(0) == L"0.00%",
              "Displayed accuracy discards digits below two decimals and shows 100.00% only at exactly 100%.");
        Check(EmptyAccuracyText == L"--.--%", "The empty accuracy placeholder keeps the two-decimal width.");
    }
    void TestJudgementRing()
    {
        using namespace taiko_result_ui;
        std::array<Color, RingJudgementCount> colors{};
        for (std::size_t i = 0; i < colors.size(); ++i)
            colors[i] = {static_cast<float>(i + 1) / 10, 0, 0, 1};
        const Point center{100, 200};
        const auto empty = BuildJudgementRing({}, colors, center, 80, 90, 1);
        Check(empty.rectangles.empty() && empty.slices.empty() && !RingSliceAt(empty, {100, 285}),
              "A result without judgements leaves the ring band empty.");
        // 3 MAX, 1 GREAT and 4 POOR: 37.5%, 12.5% and 50% clockwise from 12 o'clock.
        const auto ring = BuildJudgementRing({3, 0, 1, 0, 0, 0, 0, 4}, colors, center, 80, 90, 0.5F);
        Check(ring.slices.size() == 3 && ring.slices[0].judgement == 0 && ring.slices[1].judgement == 2 &&
                  ring.slices[2].judgement == 7 && ring.slices[0].start == 0 && ring.slices[0].end == 0.375 &&
                  ring.slices[1].end == 0.5 && ring.slices[2].end == 1,
              "Slices follow MAX..POOR order and skip judgements without counts.");
        double area = 0;
        std::array<double, RingJudgementCount> share{};
        for (const auto &rectangle : ring.rectangles)
        {
            const auto b = rectangle.bounds;
            Check(b.width > 0 && b.height > 0 && b.x >= 10 - .01F && b.x + b.width <= 190.01F && b.y >= 110 - .01F &&
                      b.y + b.height <= 290.01F,
                  "Ring rows stay inside the outer circle's box.");
            const double cx = b.x + b.width / 2 - center.x, cy = b.y + b.height / 2 - center.y;
            const double distance = std::hypot(cx, cy);
            Check(distance > 79 && distance < 91, "Ring rows cover only the band between the radii.");
            area += static_cast<double>(b.width) * b.height;
            share[static_cast<std::size_t>(std::lround(rectangle.color.red * 10)) - 1] += static_cast<double>(b.width) * b.height;
        }
        const double band = 3.14159265358979 * (90.0 * 90.0 - 80.0 * 80.0);
        Check(std::abs(area - band) / band < .01 && std::abs(share[0] / area - .375) < .01 &&
                  std::abs(share[2] / area - .125) < .01 && std::abs(share[7] / area - .5) < .01,
              "Slice areas match the judgement shares over the whole band.");
        const auto at = [&](float x, float y) { return RingSliceAt(ring, {center.x + x, center.y + y}); };
        Check(at(10, 84) && at(10, 84)->judgement == 0 && at(-10, 84) && at(-10, 84)->judgement == 7 &&
                  at(-10, -84) && at(-10, -84)->judgement == 7 && at(60, -60) && at(60, -60)->judgement == 2 &&
                  !at(0, 0) && !at(0, 79) && !at(0, 91),
              "Pointer lookup returns the slice under the band and nothing inside or outside it.");
    }
    void TestAccuracyRanks()
    {
        struct RankCase
        {
            std::uint32_t units;
            std::wstring_view rank;
        };
        constexpr std::array<RankCase, 21> cases{{
            {100'000'000, L"Y"},  {99'999'999, L"X"},  {99'000'000, L"X"},
            {98'999'999, L"SS"},  {98'000'000, L"SS"}, {97'999'999, L"S"},
            {96'000'000, L"S"},   {95'999'999, L"AA"}, {93'000'000, L"AA"},
            {92'999'999, L"A"},   {90'000'000, L"A"},  {89'999'999, L"B"},
            {85'000'000, L"B"},   {84'999'999, L"C"},       {80'000'000, L"C"},
            {79'999'999, L"D"},   {75'000'000, L"D"},       {74'999'999, L"E"},
            {70'000'000, L"E"},   {69'999'999, L"F"},       {0, L"F"},
        }};
        for (const auto &test : cases)
        {
            TaikoPlayResult result;
            result.summary.totalNotes = result.summary.processedNotes = 1;
            result.summary.accuracy = AccuracyScore::FromUnits(test.units);
            NullRenderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            TextCatalog texts;
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            auto view = CreateResultView(result, visuals, texts);
            view->Initialize({meshes, textRendering, renderer, audio, 1280, 720});
            auto *canvas = visuals.FindCanvas(1);
            if (!canvas)
                throw std::runtime_error("Rank view must own a result Canvas.");
            const auto checkRank = [&]
            {
                const auto *rank = FindNode(canvas->Root(), "Result.Rank");
                if (!rank)
                    throw std::runtime_error("Rank must render an image instead of a placeholder.");
                const auto *sprite = rank->GetComponent<SpriteVisualComponent>();
                if (!sprite)
                    throw std::runtime_error("Rank must own an image component.");
                Check(renderer.ImagePath(sprite->Style().normalImage).stem().wstring() == test.rank,
                      "Rank PNG must match the exact six-decimal accuracy boundary.");
                auto accuracyNumber = AccuracyText(result.summary.accuracy);
                accuracyNumber.pop_back();
                const auto *accuracy = FindNode(canvas->Root(), "Result.Accuracy");
                Check(accuracy && accuracy->GetComponent<TextVisualComponent>()->Text() == accuracyNumber,
                      "Each rank case displays its accuracy truncated to two decimals.");
                const auto bounds = rank->Bounds();
                const auto native = renderer.GetImageSize(sprite->Style().normalImage);
                Check(bounds.width > 0 && bounds.height > 0 && bounds.x >= 485 && bounds.y >= 342 &&
                          bounds.x + bounds.width <= 721.01F && bounds.y + bounds.height <= 460.01F &&
                          std::abs(bounds.width / bounds.height - native.width / native.height) < .001F,
                      "Every rank must retain its aspect ratio inside the ring.");
            };
            checkRank();
            view->OnResize(960, 1080);
            checkRank();
            view->Shutdown();
            Check(visuals.CanvasCount() == 0, "Rank view must release its Canvas on shutdown.");
        }
    }
    std::unique_ptr<IPlayResult> FixtureResult()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.name = "Result regression";
        for (int i = 0; i < 3; ++i)
            pattern.notes.push_back(
                {.position = {0, {i, 4}}, .keyType = 1, .sourceOrder = static_cast<std::size_t>(i)});
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Result fixture must load.");
        loaded.session->SetResultInitialTime(RhythmTime{-2'000'000});
        for (const std::int64_t us : {0, 510'000, 990'000})
            static_cast<void>(loaded.session->ProcessInput('F', InputEdge::Pressed, RhythmTime{us}));
        return loaded.session->TakeResult(PlayEndReason::Completed, RhythmTime{1'000'000});
    }
    void TestMillisecondHistogram()
    {
        TaikoPlayResult result;
        for (const std::int64_t error : {-1500, -500, -499, 0, 499, 500, 1500})
            result.timingErrors.push_back({.error = RhythmDuration{error}, .grade = JudgementGrade::Max});
        const Rect scatter{0, 0, 400, 100}, histogram{0, 200, 400, 120};
        auto graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        const auto firstBar = 2 + result.timingErrors.size();
        Check(graph.maximumBinCount == 3 && graph.rectangles.size() == firstBar + 5,
              "Symmetric half-ms boundaries must produce five occupied 1ms bins and three zero-bin inputs.");
        const auto &zero = graph.rectangles[firstBar + 2].bounds;
        Check(zero.x < histogram.width / 2 && zero.x + zero.width > histogram.width / 2 &&
                  std::abs(zero.height - histogram.height) < .001F,
              "The zero-ms bin must be centered on zero and its height must represent the full count.");
        for (std::size_t i = firstBar + 1; i < graph.rectangles.size(); ++i)
            Check(std::abs(graph.rectangles[i].bounds.x - graph.rectangles[i - 1].bounds.x - 2) < .001F,
                  "Adjacent histogram bars must be exactly 1ms apart on the shared error axis.");
        const auto narrow = taiko_result_ui::BuildGraphs(result, scatter, histogram, 3);
        Check(narrow.maximumBinCount == graph.maximumBinCount && narrow.rectangles.size() == graph.rectangles.size(),
              "A minimum pixel width must never merge the 1ms bins or change their counts.");
        for (std::size_t i = firstBar; i < narrow.rectangles.size(); ++i)
            Check(narrow.rectangles[i].bounds.width >= 3 && narrow.rectangles[i].bounds.x >= histogram.x &&
                      narrow.rectangles[i].bounds.x + narrow.rectangles[i].bounds.width <=
                          histogram.x + histogram.width,
                  "Small viewports must keep bars visible and inside the plot.");
        result.timingErrors = {{.error = RhythmDuration{-125'000}}, {.error = RhythmDuration{126'000}}};
        graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        Check(graph.halfRangeMilliseconds == 130 && graph.maximumBinCount == 1 && graph.rectangles.size() == 6 &&
                  graph.rectangles.back().bounds.width < 2,
              "Expanding the error axis must preserve 1ms bin resolution instead of keeping 41 wide bins.");
        result.timingErrors = {{.error = RhythmDuration{-1'000'000'000'000}},
                               {.error = RhythmDuration{1'000'000'000'000}}};
        graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        Check(graph.rectangles.size() == 6 && graph.maximumBinCount == 1,
              "Large outliers must allocate only the occupied bins.");
        for (const auto &rectangle : graph.rectangles)
            Check(std::isfinite(rectangle.bounds.x) && std::isfinite(rectangle.bounds.width) &&
                      rectangle.bounds.width > 0,
                  "Sparse outlier graphs must keep finite positive geometry.");
    }
    bool SameColor(Color left, Color right)
    {
        return left.red == right.red && left.green == right.green && left.blue == right.blue &&
               left.alpha == right.alpha;
    }
    void TestHistogramJudgementColors()
    {
        TaikoPlayResult result;
        result.timingErrors = {{.error = RhythmDuration{4500}, .grade = JudgementGrade::Max},
                               {.error = RhythmDuration{4501}, .grade = JudgementGrade::Perfect},
                               {.error = RhythmDuration{4900}, .grade = JudgementGrade::Perfect},
                               {.error = RhythmDuration{15000}, .grade = JudgementGrade::Great},
                               {.error = RhythmDuration{35000}, .grade = JudgementGrade::Good},
                               {.error = RhythmDuration{70000}, .grade = JudgementGrade::Bad}};
        const Rect scatter{0, 0, 400, 100}, histogram{0, 200, 400, 120};
        const auto graph = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        const auto firstBar = 2 + result.timingErrors.size();
        Check(graph.maximumBinCount == 3 && graph.rectangles.size() == firstBar + 5,
              "Peak frequency must sum every judgement in the same 1ms bin.");
        const auto &max = graph.rectangles[firstBar];
        const auto &perfect = graph.rectangles[firstBar + 1];
        Check(max.bounds.x == perfect.bounds.x && max.bounds.width == perfect.bounds.width &&
                  std::abs(max.bounds.y - histogram.y) < .001F &&
                  std::abs(max.bounds.height - histogram.height / 3) < .001F &&
                  std::abs(perfect.bounds.y - max.bounds.y - max.bounds.height) < .001F &&
                  std::abs(perfect.bounds.y + perfect.bounds.height - histogram.y - histogram.height) < .001F,
              "Mixed MAX/PERFECT must stack in one bin, preserving each count and the total height.");
        for (const auto [bar, sample] :
             {std::pair{0, 0}, std::pair{1, 1}, std::pair{2, 3}, std::pair{3, 4}, std::pair{4, 5}})
            Check(SameColor(graph.rectangles[firstBar + bar].color, graph.rectangles[2 + sample].color),
                  "Every histogram judgement color must match the corresponding scatter sample.");
        std::ranges::reverse(result.timingErrors);
        const auto reversed = taiko_result_ui::BuildGraphs(result, scatter, histogram);
        for (std::size_t i = firstBar; i < graph.rectangles.size(); ++i)
            Check(SameColor(graph.rectangles[i].color, reversed.rectangles[i].color) &&
                      graph.rectangles[i].bounds.x == reversed.rectangles[i].bounds.x &&
                      graph.rectangles[i].bounds.y == reversed.rectangles[i].bounds.y &&
                      graph.rectangles[i].bounds.height == reversed.rectangles[i].bounds.height,
                  "Histogram stack order must be stable regardless of the input record order.");
    }
    void TestGraphsAndView()
    {
        auto result = FixtureResult();
        auto *taiko = dynamic_cast<TaikoPlayResult *>(result.get());
        if (!taiko)
            throw std::runtime_error("Taiko result must retain its concrete data.");
        // Give the view a score distinct from accuracy to catch accidental recomputation.
        taiko->summary.score = 12'345'678;
        const Rect scatter{0, 0, 500, 150}, histogram{550, 0, 500, 150};
        auto data = taiko_result_ui::BuildGraphs(*taiko, scatter, histogram);
        Check(data.maximumBinCount == 1 && data.halfRangeMilliseconds == 100 && data.startSeconds == -2 &&
                  data.endSeconds == 1,
              "Graph axes must cover real time/error samples and retain signed timing.");
        Check(data.rectangles.size() == 8,
              "Three timing inputs must yield three scatter points and three histogram bars.");
        for (const auto &rect : data.rectangles)
            Check(std::isfinite(rect.bounds.x) && std::isfinite(rect.bounds.y) && rect.bounds.width > 0 &&
                      rect.bounds.height > 0,
                  "Graph primitives must have valid bounded geometry.");
        // Stress only graph data, without creating thousands of widget nodes.
        const auto sample = taiko->timingErrors.front();
        taiko->timingErrors.insert(taiko->timingErrors.end(), 10'000, sample);
        NullRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        TextCatalog texts;
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        auto &options=taiko->summary.playOptions;
        options.scrollSpeed=2;options.ignoreScrollEffects=true;options.metronome=true;options.mirror=true;
        options.random=true;options.randomSeed=987654321;options.randomAttemptProbability=.35;
        options.accelerate=1.5;options.judgeLevel=60;
        auto view = CreateResultView(*result, visuals, texts,
                                     {L"Recorded song", L"Recorded creator", true, L"Recorded artist"});
        view->Initialize({meshes, textRendering, renderer, audio, 1280, 720});
        view->SetVisible(true);
        auto *canvas = visuals.FindCanvas(1);
        if (!canvas)
            throw std::runtime_error("Result Canvas must exist.");
        const auto textOf = [&](std::string_view name) -> std::wstring
        {
            const auto *node = FindNode(canvas->Root(), name);
            const auto *text = node ? node->GetComponent<TextVisualComponent>() : nullptr;
            return text ? std::wstring(text->Text()) : std::wstring{};
        };
        Check(HasText(*canvas, texts.Get("Result.maximumCombo")) && textOf("Result.Score") == L"12,345,678" &&
                  textOf("Result.MaximumCombo") == std::to_wstring(taiko->summary.maximumCombo),
              "The result must display the carried score inside the ring and the maximum combo beside accuracy.");
        for(const auto key:{"scrollSpeed","metronome","mirror","random","accelerate","judgeLevel"})
            Check(FindNode(canvas->Root(),std::string("Result.Option.")+key)!=nullptr,"Applied play option renders a result icon.");
        Check(HasText(*canvas, L"Accuracy") && HasText(*canvas, L"Recorded song") &&
                  HasText(*canvas, L"Recorded creator") && HasText(*canvas, L"Recorded artist") &&
                  textOf("Result.Accuracy") + L"%" == AccuracyText(taiko->summary.accuracy),
              "Result view must display the recorded accuracy truncated to two decimals.");
        const auto packets = canvas->BuildDrawList();
        const auto *banner = FindNode(canvas->Root(), "Result.SongBanner");
        const auto *song = FindText(canvas->Root(), L"Recorded song");
        const auto *artist = FindText(canvas->Root(), L"Recorded artist");
        const auto *creator = FindText(canvas->Root(), L"Recorded creator");
        const auto *rank = FindNode(canvas->Root(), "Result.RankRing");
        if (!banner || !song || !artist || !creator || !rank)
            throw std::runtime_error("Result must retain each metadata element independently.");
        namespace layout = taiko_result_ui::layout;
        const auto columns = layout::ColumnsFor(1280);
        Check(banner->Bounds().width == 1280 && banner->Bounds().height == 72 && song->Bounds().x == 145 &&
                  song->Bounds().y == 720 - 41 && artist->Bounds().x == 145 && artist->Bounds().y == 720 - 61 &&
                  creator->Bounds().x + creator->Bounds().width <= 1248.01F && creator->Bounds().y >= 720 - 72 &&
                  std::abs(rank->Bounds().x + 172 - (columns.centerX + columns.sideWidth / 2)) < .01F &&
                  columns.sideWidth == 414,
              "Song, artist and creator share the top banner; the rank ring is centred in the middle column.");
        const auto inside = [](Rect b, Rect area)
        {
            return b.x >= area.x - .01F && b.x + b.width <= area.x + area.width + .01F && b.y >= area.y - .01F &&
                   b.y + b.height <= area.y + area.height + .01F;
        };
        const Rect ringBox{columns.centerX + columns.sideWidth / 2 - layout::RingOuterRadius,
                           720 - layout::RingCenterY - layout::RingOuterRadius, layout::RingOuterRadius * 2,
                           layout::RingOuterRadius * 2};
        for (const auto &packet : packets)
            if (packet.type == DrawPacketType::RectangleBatch)
                for (const auto &primitive : *packet.rectangles)
                    Check(inside(primitive.bounds, layout::Bounds(layout::HistogramPlot(columns))) ||
                              inside(primitive.bounds, layout::Bounds(layout::ScatterPlot(columns))) ||
                              inside(primitive.bounds, layout::Bounds(layout::HpPlot)) || inside(primitive.bounds, ringBox),
                          "Graph samples, the HP clear line and ring slices must stay inside their plots.");
        Check(std::ranges::count(packets, DrawPacketType::RectangleBatch, &DrawPacket::type) == 2 &&
                  packets.size() < 260,
              "Large result graphs use one batch and the judgement ring another, with constant widget count.");
        InputState input;
        for (int i = 0; i < 20; ++i)
            Check(view->Update(input) == ResultAction::None, "Idle result view must not leave the Scene.");
        Check(canvas->BuildDrawList().size() == packets.size(), "Idle updates must not rebuild graph nodes.");
        const auto *retry = FindNode(canvas->Root(), "Result.Retry");
        const auto *replay = FindNode(canvas->Root(), "Result.Replay");
        if (!retry || !replay)
            throw std::runtime_error("Result retry/replay nodes must exist.");
        Check(retry->IsEnabled() && !replay->IsEnabled(),
              "Retry is available for a played selection; replay remains disabled.");
        Check(retry->Bounds().x == 1040 && retry->Bounds().width == 240 && retry->Bounds().height == 84 &&
                  retry->Bounds().y == 0 && replay->Bounds().x == 915 && replay->Bounds().width == 125 &&
                  replay->Bounds().height == 64,
              "Retry rises above the bottom bar at the right edge with the Replay text button beside it.");
        const auto click = [&](const Visual2DNode &button)
        {
            const auto b = button.BoundsInCanvas();
            Visual2DInputRouter router;
            PointerInput p;
            p.available = true;
            p.position = {b.x + b.width / 2, b.y + b.height / 2};
            p.leftButtonDown = p.leftButtonPressed = true;
            router.Process(*canvas, p);
            p.leftButtonDown = p.leftButtonPressed = false;
            p.leftButtonReleased = true;
            router.Process(*canvas, p);
            router.Reset(*canvas);
        };
        click(*retry);
        Check(view->Update(input) == ResultAction::Retry, "A retry click must emit the neutral retry action.");
        click(*replay);
        Check(view->Update(input) == ResultAction::None, "Disabled replay must not emit any action.");
        texts.SetLanguage(Language::English);
        Check(view->Update(input) == ResultAction::None && HasText(*canvas, L"Accuracy by note group"),
              "Result strings must follow the shared language catalog.");
        for (const Size size : {Size{1024, 768}, Size{2560, 1080}, Size{720, 1280}})
        {
            view->OnResize(static_cast<std::uint32_t>(size.width), static_cast<std::uint32_t>(size.height));
            const auto *root = FindNode(canvas->Root(), "Result.Content");
            if (!root)
                throw std::runtime_error("Result content node must exist after resize.");
            const auto bounds = root->BoundsInCanvas();
            Check(bounds.width <= canvas->LogicalSize().width && bounds.height <= canvas->LogicalSize().height,
                  "The authored result panel must fit narrow and wide screens.");
        }
        const auto *back = FindNode(canvas->Root(), "Result.Back");
        if (!back)
            throw std::runtime_error("Result return button must exist.");
        const auto bounds = back->BoundsInCanvas();
        Visual2DInputRouter pointerRouter;
        PointerInput pointer;
        pointer.available = true;
        pointer.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
        pointer.leftButtonDown = pointer.leftButtonPressed = true;
        pointerRouter.Process(*canvas, pointer);
        pointer.leftButtonDown = pointer.leftButtonPressed = false;
        pointer.leftButtonReleased = true;
        pointerRouter.Process(*canvas, pointer);
        Check(view->Update(input) == ResultAction::Back,
              "The result button click must request returning to song select.");
        pointerRouter.Reset(*canvas);
        view->Shutdown();
        Check(visuals.CanvasCount() == 0, "Result view must release its owned Canvas.");
        taiko->timingErrors.clear();
        data = taiko_result_ui::BuildGraphs(*taiko, scatter, histogram);
        Check(data.rectangles.size() == 2 && data.maximumBinCount == 0, "Empty graphs must not invent samples.");
    }
    void TestResultRecordSaving()
    {
        const auto directory=std::filesystem::temp_directory_path()/
            ("YOUMU-result-record-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(directory);
        struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);} } cleanup{directory};
        const auto database=directory/"plays.sqlite3", patternPath=directory/"fixture.ymp";
        std::ofstream(patternPath)<<"Mode: Taiko";
        NullRenderer renderer;ScreenVisual2DManager visuals;visuals.Initialize(renderer,{1280,720});
        TextCatalog texts("Client/Texts/Languages");MeshRenderSystem meshes;TextRenderSystem textRendering;AudioSystem audio;
        PatternDocument pattern;pattern.sourcePath=patternPath;pattern.mode="Taiko";
        PlaySessionOptions options;options.play.random=true;options.play.randomSeed=123;options.play.judgeLevel=65;
        auto loaded=TaikoMode(options).CreateSession(pattern);
        Check(loaded.Succeeded(),"Result persistence fixture loads its actual session summary.");
        auto result=loaded.session->TakeResult(PlayEndReason::Interrupted,RhythmTime{});
        ResultViewContext display;display.recordDatabase=database;
        ResultScene scene(std::move(result),visuals,texts,display);
        scene.Initialize({meshes,textRendering,renderer,audio,1280,720});scene.BeginScene();
        InputState input;SceneManager manager;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds{5};
        bool saved{};
        while(std::chrono::steady_clock::now()<deadline)
        {
            scene.Update({0,0,0,input,audio,{}},manager);
            if(std::filesystem::exists(database))
            {
                const auto records=PlayRecordStore(database).Query(patternPath,"Taiko","JudgeLevel = 65 AND Random = true",0,10);
                if(records.size()==1){saved=true;Check(records[0].options.randomSeed==123&&!records[0].completed,"Result worker saves actual options and interrupted state.");break;}
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        Check(saved,"Entering the production result scene persists exactly one local record.");
        for(int i=0;i<5;++i)scene.Update({0,0,0,input,audio,{}},manager);
        Check(PlayRecordStore(database).Query(patternPath,"Taiko","",0,10).size()==1,"Repeated result updates do not duplicate records.");
        scene.Shutdown();
    }
    class ReturnRoute final : public GameScene
    {
      public:
        void Initialize(const EngineServices &) override
        {
        }
        void Update(const UpdateContext &, SceneManager &) override
        {
        }
        void Render(const RenderContext &) override
        {
        }
        void Shutdown() noexcept override
        {
        }
    };
    class TransitionGame final : public SceneGameClient
    {
      public:
        explicit TransitionGame(bool &verified) : verified_(verified)
        {
        }
        EngineConfig GetEngineConfig() const override
        {
            EngineConfig config;
            config.showWindow = false;
            config.audio.allowNoSoundFallback = false;
            config.renderRateOverrideHz = 60;
            return config;
        }

      protected:
        std::string_view InitialSceneId() const noexcept override
        {
            return RhythmTestSceneId;
        }
        void RegisterScenes(SceneManager &scenes) override
        {
            launch_ = std::make_shared<GameplayLaunchStore>();
            PrepareSession();
            Check(scenes.RegisterScene<RhythmTestScene>(std::string(RhythmTestSceneId),
                                                        SceneRetention::DestroyOnExit, launch_,
                                                        std::ref(AudioPlayback()), std::ref(ScreenVisuals()), false,
                                                        std::ref(texts_), std::ref(pending_)),
                  "Register production gameplay Scene.");
            Check(scenes.RegisterScene(std::string(ResultSceneId),
                                       SceneRetention::DestroyOnExit,
                                       [this]
                                       {
                                           ++resultCreations_;
                                           resultData_ = pending_.get();
                                           Check(resultData_ != nullptr, "Factory requires moved play data.");
                                           const auto played = launch_->Snapshot();
                                           ResultViewContext display{L"Retry fixture", L"Regression", true};
                                           display.backgroundPath = played.backgroundPath;
                                           display.backgroundOffsetX = played.backgroundOffsetX;
                                           display.backgroundOffsetY = played.backgroundOffsetY;
                                           return std::make_unique<ResultScene>(
                                               std::move(pending_), ScreenVisuals(), texts_,
                                               std::move(display),
                                               [this](SceneManager &manager)
                                               {
                                                   PrepareSession();
                                                   return manager.ChangeScene(RhythmTestSceneId);
                                               });
                                       }),
                  "Register production Result Scene.");
            Check(scenes.RegisterScene<ReturnRoute>(std::string(LobbySceneId),
                                                    SceneRetention::KeepAlive),
                  "Register test return route.");
            Check(resultCreations_ == 0 && !pending_, "Result construction must be deferred.");
        }
        void OnClientUpdated(const UpdateContext &context) override
        {
            if (context.input.WasKeyPressed(VK_SPACE) || context.input.WasKeyPressed(VK_ESCAPE))
                std::fprintf(stderr, "Native transition control edge: Space=%d Esc=%d scene=%s\n",
                    context.input.WasKeyPressed(VK_SPACE), context.input.WasKeyPressed(VK_ESCAPE),
                    std::string(Scenes().CurrentSceneId()).c_str());
            if (context.totalSeconds >= 12)
            {
                std::fprintf(stderr, "Result timeout: scene=%s sessions=%zu results=%zu canvases=%zu retry=%d return=%d\n",
                    std::string(Scenes().CurrentSceneId()).c_str(), sessionCreations_, resultCreations_,
                    ScreenVisuals().CanvasCount(), retryRequested_, returnRequested_);
                for (ScreenCanvasId id = 1; id <= 5; ++id)
                    if (const auto* canvas = ScreenVisuals().FindCanvas(id))
                        for (const auto name : {"Hud.Error", "Progress.CurrentTime", "Progress.TotalTime"})
                            if (const auto* node = FindNode(canvas->Root(), name))
                                if (const auto* text = node->GetComponent<TextVisualComponent>())
                                {
                                    const auto value = text->Text();
                                    std::fprintf(stderr, "Canvas %llu %s: %.*ls\n",
                                        static_cast<unsigned long long>(id), name,
                                        static_cast<int>(value.size()), value.data());
                                }
            }
            Check(context.totalSeconds < 12, "Result transition regression timed out.");
            if (Scenes().CurrentSceneId() == ResultSceneId && !retryRequested_)
            {
                const auto &summary = resultData_->Summary();
                Check(summary.endReason == PlayEndReason::Completed && summary.processedNotes == 1 &&
                          summary.unprocessedNotes == 0 && context.totalSeconds >= 4.9,
                      "All notes must finalize before the three-second Result delay.");
                Check(summary.hpGauges.size() == 1 && summary.hpGauges.front().applicable &&
                          summary.hpGauges.front().percent == 0 && !summary.IsCleared(),
                      "An automatic MISS must transfer its final HP and fail the clear gate.");
                Check(resultCreations_ == 1 && !pending_ && ScreenVisuals().CanvasCount() == 1 &&
                          !ScreenVisuals().FindCanvas(1),
                      "The moved result must survive gameplay Scene/Canvas destruction.");
                // The first attempt owns loading Canvas 1 and gameplay Canvas 2.
                auto *canvas = ScreenVisuals().FindCanvas(3);
                if (!canvas)
                    throw std::runtime_error("The production Result Canvas must be active.");
                const auto *background = FindNode(canvas->Root(), "Result.Sky");
                if (!background) throw std::runtime_error("The played result BG must survive gameplay destruction.");
                Check(background->GetComponent<SpriteVisualComponent>()->Style().normalImage.value ==
                          ScreenVisuals().RegisterImage(ResolveDefaultSkinAsset(L"InGame/InputPanel.png")).value,
                      "Actual Result rendering retains the played chart BG after gameplay Canvas destruction.");
                const auto *retry = FindNode(canvas->Root(), "Result.Retry");
                if (!retry)
                    throw std::runtime_error("The retry button must exist.");
                Check(retry->IsEnabled(), "The retry button must be enabled.");
                const auto bounds = retry->BoundsInCanvas();
                PointerInput p;
                p.available = true;
                p.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
                p.leftButtonDown = p.leftButtonPressed = true;
                Visual2DInputRouter router;
                router.Process(*canvas, p);
                p.leftButtonDown = p.leftButtonPressed = false;
                p.leftButtonReleased = true;
                router.Process(*canvas, p);
                router.Reset(*canvas);
                retryRequested_ = true;
            }
            else if (Scenes().CurrentSceneId() == RhythmTestSceneId && retryRequested_ &&
                     !returnRequested_)
            {
                Check(sessionCreations_ == 2 && resultCreations_ == 1 && !pending_ && !ScreenVisuals().FindCanvas(3),
                      "Retry must construct a fresh session and destroy the old Result Canvas.");
                // Gameplay creates its Canvas only after asynchronous resources
                // are ready. Scene entry alone is not presentation readiness.
                // Retry owns loading Canvas 4 while asynchronously preparing 5.
                auto *gameplay = ScreenVisuals().FindCanvas(5);
                if (!gameplay) return;
                Check(ScreenVisuals().CanvasCount() == 2 && FindNode(gameplay->Root(), "Hud.HP.Fill") &&
                          !FindNode(gameplay->Root(), "Hud.HP.Fill")->IsVisible(),
                      "Retry owns loading/gameplay Canvases and starts the fresh HP fill at zero.");
                Check(Scenes().ChangeScene(LobbySceneId), "Queue returning after retry.");
                returnRequested_ = true;
            }
            else if (Scenes().CurrentSceneId() == LobbySceneId && returnRequested_)
            {
                Check(ScreenVisuals().CanvasCount() == 0 && resultCreations_ == 1,
                      "Leaving Result must destroy its transient Canvas.");
                verified_ = true;
                Scenes().Quit();
            }
        }

      private:
        void PrepareSession()
        {
            PatternDocument pattern;
            pattern.name = "Automatic completion regression";
            pattern.mode = "Taiko";
            pattern.notes.push_back({.keyType = 1});
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Transition fixture must load.");
            GameplayLaunchRequest request;
            request.backgroundPath = ResolveDefaultSkinAsset(L"InGame/InputPanel.png");
            request.backgroundOffsetX = 70;
            request.backgroundOffsetY = -25;
            launch_->SetValidated(std::move(request), std::move(loaded.session));
            ++sessionCreations_;
        }
        bool &verified_;
        bool retryRequested_{};
        bool returnRequested_{};
        std::size_t sessionCreations_{};
        std::size_t resultCreations_{};
        std::shared_ptr<GameplayLaunchStore> launch_;
        std::unique_ptr<IPlayResult> pending_;
        const IPlayResult *resultData_{};
        TextCatalog texts_;
    };
} // namespace
int main()
{
    try
    {
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        TestResultRecordSaving();
        TestFailureOverlay();
        TestControlUsageOverlay();
        TestHpCurveGeometry();
        TestChartBackground();
        TestAccuracyText();
        TestJudgementRing();
        TestAccuracyRanks();
        TestMillisecondHistogram();
        TestHistogramJudgementColors();
        TestGraphsAndView();
        bool verified{};
        Check(Run(std::make_unique<TransitionGame>(verified)) == 0 && verified,
              "Actual D3D12/FMOD Result transition must complete.");
        std::puts("Result ranks (21 boundary cases), graphs, two-decimal truncated display, ownership and actual deferred "
                  "transition passed.");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    catch (...)
    {
        std::fputs("Unknown Result test failure.\n", stderr);
        return 1;
    }
}
