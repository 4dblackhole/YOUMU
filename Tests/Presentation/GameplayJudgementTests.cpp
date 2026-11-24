#include "App/AssetPaths.h"
#include "YOUMUAssets.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayJudgementView.h"
#include "GameScene/RhythmTestScene/Submodules/GameplayPresenter.h"
#include "Mode/Results/IPlayResultCollector.h"
#include "Parsing/ChartParser.h"
#include "Presentation/LaneKeyBeam.h"
#include "MusicSelectTestInput.h"
#include "Presentation/UiErrorLine.h"
#include "Taiko/TaikoMode.h"
#include "Time/RhythmTimer.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>

using namespace PhantomCore;

namespace
{

    void Check(const bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    bool Near(const float left, const float right)
    {
        return std::abs(left - right) < 0.001F;
    }
    Visual2DNode *Find(Visual2DNode &root, const std::string_view name)
    {
        if (root.Name() == name)
            return &root;
        for (const auto &child : root.Children())
            if (auto *found = Find(*child, name))
                return found;
        return nullptr;
    }
    Visual2DNode &Node(Visual2DNode &root, const std::string_view name)
    {
        auto *node = Find(root, name);
        Check(node != nullptr, "Expected judgement node missing.");
        return *node;
    }
    // Visible timing-guide hit lines (Judgement.HitLine.<n>).
    std::size_t VisibleHitLines(Visual2DNode &root)
    {
        std::size_t count = 0;
        for (std::size_t i = 0; auto *line = Find(root, "Judgement.HitLine." + std::to_string(i)); ++i)
            count += line->IsVisible() ? 1 : 0;
        return count;
    }
    Color Tint(Visual2DNode &node)
    {
        return node.GetComponent<SpriteVisualComponent>()->Style().normal;
    }
    bool SameRgb(const Color left, const Color right)
    {
        return Near(left.red, right.red) && Near(left.green, right.green) && Near(left.blue, right.blue);
    }
    std::size_t NodeCount(const Visual2DNode &root)
    {
        std::size_t count = 1;
        for (const auto &child : root.Children())
            count += NodeCount(*child);
        return count;
    }
    class AssetRenderer final : public Visual2DRenderSystem
    {
      public:
        std::size_t loads{};
        std::map<std::uint64_t, std::filesystem::path> paths;
        ImageHandle LoadImageFile(const std::filesystem::path &path) override
        {
            // Read the real embedded/fallback PNG metadata, without a GPU.
            std::ifstream file(path, std::ios::binary);
            std::array<unsigned char, 24> header{};
            file.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
            Check(file.gcount() == static_cast<std::streamsize>(header.size()) && header[0] == 137 && header[1] == 'P',
                  "Judgement sprite must resolve to a real PNG.");
            const auto dimension = [&header](const std::size_t offset)
            {
                return (static_cast<std::uint32_t>(header[offset]) << 24) |
                       (static_cast<std::uint32_t>(header[offset + 1]) << 16) |
                       (static_cast<std::uint32_t>(header[offset + 2]) << 8) |
                       static_cast<std::uint32_t>(header[offset + 3]);
            };
            const auto id = ++loads;
            paths[id] = path;
            sizes_[id] = {static_cast<float>(dimension(16)), static_cast<float>(dimension(20))};
            return {id};
        }
        Size GetImageSize(const ImageHandle image) const noexcept override
        {
            const auto found = sizes_.find(image.value);
            return found != sizes_.end() ? found->second : Size{};
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
        std::map<std::uint64_t, Size> sizes_;
    };
    NoteEvent Event(const NoteEventType type, const JudgementGrade grade, const RhythmDuration error = {})
    {
        NoteEvent event;
        event.type = type;
        event.judgement = {grade, error, 1.0};
        return event;
    }
    void CheckOverlay(Visual2DNode &root, const std::string_view word)
    {
        constexpr std::array names{"MAX",          "PERFECT",          "GREAT",          "GOOD",         "BAD", "MISS",
                                   "POOR",         "Recovery.MAX",     "Recovery.PERFECT", "Recovery.GREAT",
                                   "Recovery.GOOD"};
        for (const auto name : names)
            Check(Node(root, "Lane.Judgement." + std::string(name)).IsVisible() == (word == name),
                  "Exactly the expected judgement image must be visible.");
    }
    std::wstring LabelText(Visual2DNode &root, std::string_view name)
    {
        return std::wstring(Node(root, std::string(name)).GetComponent<TextVisualComponent>()->Text());
    }
    // The panel pieces sit edge to edge, keep the wings and the centre at their
    // authored width and sample the matching columns of the one panel image.
    void CheckTimingPanel(Visual2DNode &root, const float outerHalf)
    {
        constexpr std::array<float, 6> columns{0, 128, 192, 332, 396, 524};
        const std::array<float, 6> edges{-outerHalf, -outerHalf + 128, -70, 70, outerHalf - 128, outerHalf};
        for (std::size_t i = 0; i < 5; ++i)
        {
            auto &piece = Node(root, "Judgement.Panel." + std::to_string(i));
            const auto bounds = piece.Bounds();
            Check(Near(bounds.x, edges[i]) && Near(bounds.width, edges[i + 1] - edges[i]),
                  "Timing panel pieces must abut and keep wings and centre at their authored width.");
            Check(Near(bounds.y, 56.0F - 112.0F) && Near(bounds.height, 112.0F),
                  "The panel spans 56 above to 56 below the bar's bottom.");
            const auto &sprite = *piece.GetComponent<SpriteVisualComponent>();
            Check(Near(sprite.UvOffset().x, columns[i] / 524) &&
                      Near(sprite.UvScale().x, (columns[i + 1] - columns[i]) / 524),
                  "Each panel piece samples its own columns of Hud.TimingPanel.");
            Check(Near(sprite.Style().normal.alpha, 1.0F), "The panel PNG is drawn as authored, with no tint.");
        }
        const auto early = Node(root, "Judgement.Caption.0").Bounds();
        const auto late = Node(root, "Judgement.Caption.1").Bounds();
        Check(Near(early.x, -outerHalf + 52) && Near(late.x + late.width, outerHalf - 52) && Near(early.y, 37.0F),
              "EARLY and LATE sit in the panel's wings.");
    }
    void TestAverageError(GameplayJudgementView &view, Visual2DNode &root)
    {
        const auto average = [&root] { return LabelText(root, "Judgement.Average"); };
        Check(average() == L"—" && LabelText(root, "Judgement.AverageUnit") == L"ms",
              "Before any hit line is drawn the mean shows the explicit empty value.");
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Great, RhythmDuration{12'000}));
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Great, RhythmDuration{12'200}));
        Check(average() == L"+12.1", "A late mean of the drawn lines shows a plus sign and tenths.");
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Good, RhythmDuration{-43'100}));
        Check(average() == L"−6.3", "An early mean shows a typographic minus.");
        view.Update(5.2);
        Check(average() == L"—", "Lines that have faded out no longer count, so the mean empties with them.");
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Max, RhythmDuration{-40}));
        Check(average() == L"0.0", "A mean that rounds to zero has no sign.");
        view.Reset();
        Check(average() == L"—", "Reset removes the lines and the mean.");
        // The 51st line pushes the oldest out early; that line stops counting at once.
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Bad, RhythmDuration{100'000}));
        for (int i = 0; i < 50; ++i)
            view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Max, RhythmDuration{}));
        Check(average() == L"0.0", "A line being pushed out by the 50-line limit is not part of the mean.");
        view.Reset();
    }
    void TestGuideAndOverlays()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280.0F, 720.0F});
        Visual2DCanvas canvas;
        auto &lane = canvas.Root().CreateChild("Lane");
        lane.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, -HalfPi);
        auto &circle = CreateSprite(lane, {30.0F, 40.0F, 96.0F, 96.0F}, {}, "Circle");
        GameplayJudgementView view;
        TextCatalog texts;
        const AccuracyRange base;
        view.Initialize(visuals, canvas, circle, base, texts);
        auto &root = canvas.Root();
        auto &marker = Node(root, "Judgement.HitLine.0");
        auto &bad = Node(root, "Judgement.Band.4");
        auto &margin = Node(root, "Judgement.Margin");
        Check(!marker.IsVisible() && Near(bad.NodeSize().width, 378.0F), "Initial guide uses base BAD +/-90ms.");
        Check(Near(margin.NodeSize().width, 420.0F), "Guide includes 10ms margin on each side.");
        Check(Find(root, "Judgement.Caption.2") == nullptr, "The zero line needs no separate 0 caption.");
        CheckTimingPanel(root, 262.0F);
        const auto zero = Node(root, "Judgement.Zero").Bounds();
        Check(Near(zero.y, -20.0F) && Near(zero.height, 52.0F), "The zero line runs past both ends of the hit lines.");
        TestAverageError(view, root);
        for (std::size_t index = 0; index < base.Bands().size(); ++index)
        {
            auto &band = Node(root, "Judgement.Band." + std::to_string(index));
            const auto color = band.GetComponent<SpriteVisualComponent>()->Style().normal;
            const auto beamColor =
                LaneKeyBeam::GradeColor(static_cast<JudgementGrade>(index));
            Check(Near(color.red, beamColor.red) && Near(color.green, beamColor.green) &&
                      Near(color.blue, beamColor.blue),
                  "Each independent band must match the key-beam grade color.");
        }
        const std::array words{"MAX", "PERFECT", "GREAT", "GOOD", "BAD"};
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            view.Reset();
            const auto type = index == 4 ? NoteEventType::InputRejected : NoteEventType::HitAccepted;
            view.Present(Event(type, static_cast<JudgementGrade>(index), -base.Bands()[index].halfWindow));
            CheckOverlay(root, words[index]);
            Check(marker.IsVisible() && marker.Bounds().x < 0.0F, "Early input must place the hit line to the left.");
            Check(SameRgb(Tint(marker), LaneKeyBeam::GradeColor(static_cast<JudgementGrade>(index))),
                  "A hit line takes its judgement's band colour.");
            const auto overlay = Node(root, "Lane.Judgement." + std::string(words[index])).BoundsInCanvas();
            const auto target = circle.BoundsInCanvas();
            Check(Near(overlay.x + overlay.width * 0.5F, target.x + target.width * 0.5F) &&
                      Near(overlay.y, target.y + target.height + 6.0F),
                  "The judgement word sits centred just above the judgement circle, 6 clear of it.");
            Check(overlay.height < target.height * 0.5F && overlay.width < target.width * 1.5F,
                  "Only the word is shown: no ring around the judgement circle.");
            const auto& world = Node(root, "Lane.Judgement." + std::string(words[index])).Transform().WorldMatrix();
            const auto right = TransformDirection({1, 0, 0}, world);
            Check(Near(right.x, 1.0F) && Near(right.y, 0.0F),
                  "Lettering must remain upright in the rotated lane.");
        }
        view.Reset();
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Good, RhythmDuration{54'500}));
        Check(marker.Bounds().x > 0.0F, "Late input must place the hit line to the right.");
        Check(Near(Tint(marker).alpha, 0.0F) && Near(marker.NodeSize().width, 0.0F),
              "A hit line starts transparent and zero wide.");
        Check(Near(marker.NodeSize().height, 48.0F) && Near(marker.Bounds().y, 6.0F),
              "A hit line is 48 tall, pivoted on the 12px colour bar's centre.");
        view.Update(0.1);
        Check(Near(Node(root, "Lane.Judgement.GOOD").GetComponent<SpriteVisualComponent>()->Style().normal.alpha,
                   0.5F),
              "Judgement overlay must fade over 200ms.");
        Check(Near(Tint(marker).alpha, 0.6F) && Near(marker.NodeSize().width, 4.0F),
              "A hit line reaches 0.6 alpha and 4px after its 100ms fade-in.");
        view.Update(0.1);
        CheckOverlay(root, "");
        view.Update(2.4);
        Check(marker.IsVisible() && Near(Tint(marker).alpha, 0.3F), "A hit line fades out linearly over 5s.");
        view.Update(2.49);
        Check(marker.IsVisible(), "A hit line remains until its 5s fade-out ends.");
        view.Update(0.02);
        Check(!marker.IsVisible(), "A hit line disappears after its fade-out.");

        view.Reset();
        view.Present(Event(NoteEventType::InputRejected, JudgementGrade::Max));
        CheckOverlay(root, "POOR");
        Check(Near(marker.Bounds().x, 0.0F), "Zero-error POOR keeps the actual input timing.");
        Check(SameRgb(Tint(marker), LaneKeyBeam::WrongInputColor()), "A POOR hit line uses the POOR beam colour.");
        view.Reset();
        view.Present(Event(NoteEventType::InputRejected, JudgementGrade::Miss));
        CheckOverlay(root, "");
        Check(VisibleHitLines(root) == 0, "Empty/out-of-range input must not show POOR or a hit line.");
        view.Present(Event(NoteEventType::TickAccepted, JudgementGrade::Unjudged));
        CheckOverlay(root, "");
        view.Present(Event(NoteEventType::TickRejected, JudgementGrade::Unjudged));
        CheckOverlay(root, "");
        Check(VisibleHitLines(root) == 0, "A wrong colour over a tick only leaves it unprocessed: no POOR word or line.");
        NoteProcessResult rejectedTick;
        rejectedTick.events.push_back(Event(NoteEventType::TickRejected, JudgementGrade::Unjudged));
        const auto rejectedColor = LaneKeyBeam::InputColor(rejectedTick);
        Check(rejectedColor.red > .9F && rejectedColor.green > .9F && rejectedColor.blue > .9F,
              "A wrong colour over a tick keeps the plain white beam, not the POOR colour.");
        view.Present(Event(NoteEventType::Missed, JudgementGrade::Miss, RhythmDuration{500'000}));
        CheckOverlay(root, "MISS");
        Check(VisibleHitLines(root) == 0, "Automatic MISS must not invent an input hit line.");
        // BAD recovery: the GOOD..MAX that answers an early BAD or a POOR shows the purple-tinted word.
        for (const auto [grade, word] : std::array<std::pair<JudgementGrade, const char *>, 4>{
                 {{JudgementGrade::Max, "Recovery.MAX"},
                  {JudgementGrade::Perfect, "Recovery.PERFECT"},
                  {JudgementGrade::Great, "Recovery.GREAT"},
                  {JudgementGrade::Good, "Recovery.GOOD"}}})
        {
            auto recovered = Event(NoteEventType::HitAccepted, grade);
            recovered.recovery = true;
            view.Present(recovered);
            CheckOverlay(root, word);
        }
        view.Present(Event(NoteEventType::InputRejected, JudgementGrade::Bad, RhythmDuration{-70'000}));
        CheckOverlay(root, "BAD");
        auto leftAtBad = Event(NoteEventType::Missed, JudgementGrade::Miss, RhythmDuration{500'000});
        leftAtBad.leftAtEarlyBad = true;
        view.Present(leftAtBad);
        CheckOverlay(root, "BAD");

        const auto count = NodeCount(root);
        const auto loads = renderer.loads;
        const AccuracyRange sync{"sync", 50, AccuracyRange::DefaultLevel50Bands(), RhythmDuration{10'000}};
        auto synced = Event(NoteEventType::HitAccepted, JudgementGrade::Good);
        synced.judgement = sync.Evaluate({}, RhythmTime{60'000});
        view.Present(synced);
        CheckOverlay(root, "GOOD");
        Check(Near(bad.NodeSize().width, 378.0F), "Syncopation must not resize the base guide.");
        view.Reset();
        view.Present(Event(NoteEventType::InputRejected, JudgementGrade::Bad, RhythmDuration{500'000}));
        Check(Near(marker.Bounds().x, margin.NodeSize().width * 0.5F), "Hit lines are clamped to the visible guide.");
        view.OnResize(400.0F);
        Check(Near(margin.NodeSize().width, 256.0F), "The panel around a wide range must fit a narrow viewport.");
        Check(Near(marker.Bounds().x, 128.0F), "Resize must preserve the timing position.");
        view.OnResize(1280.0F);
        Check(Near(bad.NodeSize().width, 378.0F), "Resize restores the fixed time scale.");
        Check(NodeCount(root) == count && renderer.loads == loads, "Repeated hits/resize must reuse nodes and images.");
        view.Reset();
        for (int i = 0; i < 50; ++i)
            view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Perfect, RhythmDuration{i * 1000}));
        view.Update(1);
        Check(VisibleHitLines(root) == 50 && Near(Tint(marker).alpha, 0.6F * (1.0F - 0.9F / 5.0F)),
              "Up to 50 hit lines stay live together.");
        view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Great, RhythmDuration{50'000}));
        Check(VisibleHitLines(root) == 51, "The oldest line fades out instead of vanishing at once.");
        view.Update(.05);
        Check(marker.IsVisible() && Tint(marker).alpha < 0.3F, "The displaced oldest line fades out quickly.");
        view.Update(.05);
        Check(!marker.IsVisible() && VisibleHitLines(root) == 50, "The displaced line expires after 100ms.");
        for (int i = 0; i < 100; ++i)
            view.Present(Event(NoteEventType::HitAccepted, JudgementGrade::Good, RhythmDuration{-i * 100}));
        Check(VisibleHitLines(root) <= 64, "A burst of hits reuses the fixed line pool.");
        view.Update(.1);
        Check(VisibleHitLines(root) == 50, "At most 50 lines remain once displaced lines have faded.");
        Check(NodeCount(root) == count && renderer.loads == loads, "Hit lines allocate no per-hit nodes/images.");
        view.Shutdown();
        view.Update(1.0);
        view.OnResize(100.0F);
        view.Present(synced);
        CheckOverlay(root, "");

        Visual2DCanvas strictCanvas;
        auto &strictCircle = CreateSprite(strictCanvas.Root(), {0.0F, 0.0F, 96.0F, 96.0F}, {}, "Circle");
        GameplayJudgementView strictView;
        strictView.Initialize(visuals, strictCanvas, strictCircle, AccuracyRange{"strict", 100}, texts);
        Check(Near(Node(strictCanvas.Root(), "Judgement.Band.4").NodeSize().width, 189.0F),
              "JudgeLevel 100 must halve the base BAD band length.");
        CheckTimingPanel(strictCanvas.Root(), 202.0F);
        strictView.Shutdown();
    }
    void CheckGearTransform(Visual2DNode &root)
    {
        auto &gear = Node(root, "ScrollGear");
        Check(Near(Node(root, "ScrollGear.Border").BoundsInCanvas().y, 0),
              "The outer gear bottom must touch the screen's horizontal centre.");
        std::vector<std::pair<Visual2DNode *, Rect>> descendants;
        const auto collect = [&](const auto &self, Visual2DNode &parent) -> void
        {
            for (const auto &child : parent.Children())
            {
                descendants.emplace_back(child.get(), child->BoundsInCanvas());
                self(self, *child);
            }
        };
        collect(collect, gear);
        for (const auto name : {"ScrollGear.Border", "ScrollGear.Lane", "InputPresentation"})
            Check(Node(root, name).Parent() == &gear, "Every gear branch must share the same transform parent.");
        const auto hud = Node(root, "Hud.GameProgress").BoundsInCanvas();
        const auto position = gear.Position();
        gear.SetPosition({position.x + 37, position.y + 29});
        for (const auto &[node, before] : descendants)
        {
            const auto after = node->BoundsInCanvas();
            Check(Near(after.x - before.x, 37) && Near(after.y - before.y, 29) && Near(after.width, before.width),
                  "One gear transform must move all nested keys, lane parts, notes and judgements together.");
            Check(Near(after.height, before.height),
                  "One gear transform must move all nested keys, lane parts, notes and judgements together.");
        }
        const auto unchangedHud = Node(root, "Hud.GameProgress").BoundsInCanvas();
        Check(Near(hud.x, unchangedHud.x) && Near(hud.y, unchangedHud.y),
              "Screen HUD must stay independent of the gear transform.");
        gear.SetPosition(position);
    }
    void TestGameplayProgress()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        // A late normal head must not hide an earlier long note's later tail.
        pattern.notes = {{{1, {1, 4}}, 1}, {{0, {}}, 11, 1}, {{2, {}}, 11, 2}};
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Progress fixture must load.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        // Any built-in picture serves as the chart background here.
        const auto chartBackground = Result(L"Background.Sky");
        presenter.ConfigureBackground(chartBackground, 70, -25);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        const auto expectedBackground = visuals.RegisterImage(chartBackground);
        const auto *backgroundSprite = Node(root, "Background").GetComponent<SpriteVisualComponent>();
        Check(backgroundSprite && backgroundSprite->Style().normalImage.value == expectedBackground.value,
              "Applying the background colour must preserve the chart image handle.");
        visuals.Update(0);
        const auto backgroundPackets = visuals.FindCanvas(1)->BuildDrawList();
        Check(std::ranges::any_of(backgroundPackets, [&](const auto &packet) {
                  return packet.image.value == expectedBackground.value;
              }), "The chart background must reach the draw list after a visual update.");
        auto &inputPanel = Node(root, "Input.Panel");
        std::size_t keyIndex = 0;
        for (const auto &child : inputPanel.Children())
        {
            if (child->Name() != "Input.Key.PressFlash")
                continue;
            const auto color = child->GetComponent<SpriteVisualComponent>()->Style().normal;
            const bool whiteRedGreen = color.red == 1.0F && color.green == 1.0F;
            Check(whiteRedGreen && color.blue == 1.0F,
                  "Key flashes must be white light without the Don/Kat ambient color.");
            ++keyIndex;
        }
        Check(keyIndex == 4, "All four key flashes must exist.");
        // Left Kat: the first assigned key (D) shows KeyLight and the flash; another assigned key (C) only KeyLight.
        const auto firstChild = [&](const std::string_view name) -> Visual2DNode & {
            for (const auto &child : inputPanel.Children())
                if (child->Name() == name)
                    return *child;
            throw std::runtime_error("Missing input panel node.");
        };
        auto &leftKatGlow = firstChild("Input.Key.Glow");
        auto &leftKatFlash = firstChild("Input.Key.PressFlash");
        const auto pressOnly = [&](const std::uint16_t key) {
            InputState input;
            InputStateTestAccess::Begin(input);
            InputStateTestAccess::Key(input, key, true);
            presenter.UpdateInputPresentation(input, 0.0);
        };
        pressOnly('F');
        Check(leftKatGlow.IsVisible(), "Another assigned key must show KeyLight.");
        Check(!leftKatFlash.IsVisible(), "Another assigned key must not show the key press flash.");
        pressOnly('D');
        Check(leftKatGlow.IsVisible(), "The first assigned key must show KeyLight.");
        Check(leftKatFlash.IsVisible(), "The first assigned key must show the key press flash with KeyLight.");
        auto &progress = Node(root, "Hud.GameProgress");
        auto &track = Node(root, "Progress.Track");
        auto &fill = Node(root, "Progress.Fill");
        auto &marker = Node(root, "Progress.Marker");
        auto &currentText = *Node(root, "Progress.CurrentTime").GetComponent<TextVisualComponent>();
        const auto &totalText = *Node(root, "Progress.TotalTime").GetComponent<TextVisualComponent>();
        Check(totalText.Text() == L"0:04", "Total time includes the last long tail, without judgement grace.");
        const auto checkFraction = [&](const float expected)
        {
            Check(Near(fill.Bounds().width, track.Bounds().width * expected), "Progress fill must follow play time.");
            Check(Near(marker.Bounds().x + marker.Bounds().width * 0.5F,
                       track.Bounds().x + track.Bounds().width * expected),
                  "Marker must follow the fill endpoint.");
            Check(Near(fill.GetComponent<SpriteVisualComponent>()->UvScale().x, expected),
                  "Fill texture must be cropped with progress.");
        };
        presenter.UpdatePresentation(RhythmTime{-2'000'000});
#if defined(_DEBUG)
        const auto &timerDebug = *Node(root, "Debug.NoteState").GetComponent<TextVisualComponent>();
        Check(timerDebug.Text().find(L"DEBUG Timer: -2000000 us (-2000.000 ms)") == 0,
              "Debug must display the signed chart timer including negative pre-roll.");
#else
        Check(Find(root, "Debug.NoteState") == nullptr, "Release must not create a debug timer label.");
#endif
        checkFraction(0);
        Check(!fill.IsVisible() && currentText.Text() == L"0:00", "Pre-roll must not show negative elapsed time.");
        presenter.UpdatePresentation(RhythmTime{2'000'000});
        checkFraction(.5F);
        CheckGearTransform(root);
        Check(fill.IsVisible() && currentText.Text() == L"0:02", "The current time label must be dynamic.");

        RhythmTimer timer;
        timer.Start(0, 1'000'000);
        timer.Pause(1'000'000);
        presenter.UpdatePresentation(timer.Now(3'000'000));
#if defined(_DEBUG)
        Check(timerDebug.Text().find(L"DEBUG Timer: 1000000 us (1000.000 ms)") == 0,
              "Debug must display the frozen timer rather than render elapsed time.");
        presenter.UpdatePresentation(RhythmTime{999'000});
        Check(timerDebug.Text().find(L"DEBUG Timer: 999000 us (999.000 ms)") == 0,
              "A 1ms seek must immediately update the displayed debug time.");
        presenter.UpdatePresentation(timer.Now(3'000'000));
#endif
        checkFraction(.25F);
        timer.Resume(3'000'000);
        presenter.UpdatePresentation(timer.Now(4'000'000));
        checkFraction(.5F);
        for (const auto viewport : std::array<Size, 4>{{{1920, 1080}, {2560, 1080}, {800, 900}, {100, 720}}})
        {
            const auto width = static_cast<std::uint32_t>(viewport.width);
            const auto height = static_cast<std::uint32_t>(viewport.height);
            visuals.OnResize(width, height);
            presenter.OnResize(width, height);
            const auto bg = Node(root, "Background").Bounds();
            const auto logicalWidth = viewport.width / viewport.height * 720;
            Check(bg.x <= -logicalWidth / 2 && bg.x + bg.width >= logicalWidth / 2 && bg.y <= -360,
                  "Chart backgrounds must cover every viewport, including authored offsets.");
            Check(bg.y + bg.height >= 360, "Chart backgrounds must cover every viewport, including authored offsets.");
            Check(Near(bg.x + bg.width / 2, 70) && Near(bg.y + bg.height / 2, -25),
                  "Background offsets must follow the engine's +Y-up coordinate system.");
            checkFraction(.5F);
            CheckGearTransform(root);
            Check(Near(progress.Bounds().width, viewport.width / viewport.height * 720) && Near(progress.Bounds().y, -360),
                  "The progress strip spans the bottom edge at the actual logical viewport width.");
            Check(marker.Bounds().x >= 0 && marker.Bounds().x + marker.Bounds().width <= progress.NodeSize().width,
                  "Marker must remain inside the bar after resize.");
        }
        presenter.UpdatePresentation(RhythmTime{4'000'000});
        checkFraction(1);
        presenter.UpdatePresentation(RhythmTime{9'000'000});
        checkFraction(1);
        Check(currentText.Text() == totalText.Text(), "Post-roll must keep the completed time and progress.");
        loaded.session->Reset();
        presenter.ApplyFeedback({.reset = true});
        presenter.UpdatePresentation(RhythmTime{-1});
        checkFraction(0);
        presenter.Shutdown();
        Check(visuals.FindCanvas(1) == nullptr, "Shutdown must destroy the progress Canvas.");

        // Test the view's constant allocation/resource boundary independently
        // of note/measure visual pools that legitimately change during play.
        Visual2DCanvas canvas;
        GameplayProgressView view;
        pattern.timing = {{{1, {}}, TimingDirectiveType::Bpm, 240},
                          {{1, {}}, TimingDirectiveType::DelayMilliseconds, 500}};
        loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Compiled BPM/delay progress fixture must load.");
        view.Initialize(visuals, canvas.Root(), *loaded.session, texts);
        view.SetLayout({0, 0, 1200, view.Height()});
        const auto nodes = NodeCount(canvas.Root());
        const auto loads = renderer.loads;
        const auto end = loaded.session->Timeline().Compile({2, {}});
        view.Update(end / 2);
        Check(Near(Node(canvas.Root(), "Progress.Fill").GetComponent<SpriteVisualComponent>()->UvScale().x, .5F),
              "Progress must use compiled BPM/delay timing rather than authored beat count.");
        for (int index = 0; index < 1000; ++index)
            view.Update(RhythmTime{index * 5000});
        Check(nodes == NodeCount(canvas.Root()) && loads == renderer.loads,
              "Progress updates must not create nodes or reload images.");
        view.Shutdown();
        view.Update(RhythmTime{1});
        view.SetLayout({0, 0, 1, 1});

        const auto checkZeroDuration = [&](const bool empty, const double offset)
        {
            Visual2DCanvas edgeCanvas;
            PlaySession emptySession;
            pattern.notes = {{{0, {}}, 1}};
            pattern.timing.clear();
            pattern.patternOffsetMilliseconds = offset;
            loaded = TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Zero/negative note fixture must load.");
            view.Initialize(visuals, edgeCanvas.Root(), empty ? emptySession : *loaded.session, texts);
            view.SetLayout({0, 0, 1200, view.Height()});
            const auto noteTime = loaded.session->Gear().Lanes().front()->Notes().front()->Timing();
            view.Update(noteTime - RhythmDuration{1});
            Check(!Node(edgeCanvas.Root(), "Progress.Fill").IsVisible(), "Zero-duration progress starts empty.");
            view.Update(noteTime);
            const auto fraction =
                Node(edgeCanvas.Root(), "Progress.Fill").GetComponent<SpriteVisualComponent>()->UvScale().x;
            Check(std::isfinite(fraction) && Near(fraction, empty ? 0.0F : 1.0F),
                  "Empty charts stay empty; nonpositive endpoints finish at the note time without division by zero.");
            view.Shutdown();
        };
        checkZeroDuration(true, 0);
        checkZeroDuration(false, 0);
        checkZeroDuration(false, -200);
    }
    void TestSlowNotesDoNotExpandAllVisuals()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 180;
        for (std::int64_t i = 0; i < 5000; ++i)
            pattern.notes.push_back({{i / 16, {i % 16, 16}}, 1});
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Large visibility fixture must compile.");
        loaded.session->SetNoteScrollMultiplier(5000, .001);
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        presenter.UpdatePresentation({});
        Check(Node(root, "Hud.Accuracy").GetComponent<TextVisualComponent>()->Text() == L"--.--%",
              "Before the first finalized note the accuracy HUD shows the two-decimal placeholder.");
        // Bound lane visibility independently of the fixed, reusable HUD pool.
        Check(NodeCount(Node(root, "ScrollGear.Lane")) < 400 && Find(root, "Lane.Note.5000"),
              "One slow future note must remain visible without creating thousands of ordinary offscreen notes.");
        const auto checkOrder = [&]
        {
            auto &first = Node(root, "Lane.Note.1");
            auto &second = Node(root, "Lane.Note.2");
            auto &notes = Node(root, "Lane.Notes");
            Check(first.Parent() == &notes && second.Parent() == &notes && first.ZIndex() > second.ZIndex(),
                  "Earlier notes and their entire long-note subtrees must render in front of later notes.");
            Check(notes.ZIndex() > Node(root, "Lane.KeyBeam").ZIndex() &&
                      notes.ZIndex() > Node(root, "Lane.JudgementCircle").ZIndex() &&
                      Node(root, "Lane.JudgementCircle").ZIndex() > Node(root, "Lane.KeyBeam").ZIndex(),
                  "Notes must render above the judgement circle, which remains above the key beam.");
        };
        checkOrder();
        Check(Node(root, "Lane.Note.1").ZIndex() > Node(root, "Lane.Note.5000").ZIndex(),
              "A very slow future note must remain behind earlier notes despite its unusual visibility.");
        loaded.session->SetNoteScrollMultiplier(5000, 1);
        presenter.UpdatePresentation({});
        Check(!Find(root, "Lane.Note.5000"), "Changing Separate must refresh the visibility projection.");
        presenter.UpdatePresentation(loaded.session->Gear().Lanes().front()->Notes().back()->Timing());
        Check(NodeCount(Node(root, "ScrollGear.Lane")) < 400 && Find(root, "Lane.Note.5000"), "Forward seek must query the end of the score.");
        presenter.UpdatePresentation({});
        Check(Find(root, "Lane.Note.1") && !Find(root, "Lane.Note.5000"),
              "Backward seek must restore only visible notes.");
        checkOrder();
        visuals.OnResize(2560, 1080);
        presenter.OnResize(2560, 1080);
        presenter.UpdatePresentation({});
        Check(NodeCount(Node(root, "ScrollGear.Lane")) < 400, "A wide viewport must retain bounded visibility work.");
        presenter.Shutdown();
    }

    void TestMeasureLinesShareNoteMotion()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{1, {}}, 1}};
        const auto effects = ChartParser{}.ParseEffect(
            "[Speed]\n1,0/1,#ScrollSpeed Separate,Value=0.25\n"
            "1,0/1,Area,3,0/1,#ScrollSpeed Whole,From=1,To=2,Curve=Linear\n",
            "bar-motion.yme");
        Check(effects.Succeeded(), "Bar motion effects must parse.");
        auto loaded = TaikoMode{}.CreateSession(pattern, effects.document);
        Check(loaded.Succeeded(), "Bar motion session must load.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        for (const auto viewport : {Size{1280, 720}, Size{1920, 1080}, Size{2560, 1080}})
        {
            visuals.OnResize(static_cast<std::uint32_t>(viewport.width), static_cast<std::uint32_t>(viewport.height));
            presenter.OnResize(static_cast<std::uint32_t>(viewport.width), static_cast<std::uint32_t>(viewport.height));
            for (const auto us : {0, 500'000, 1'500'000, 2'000'000})
            {
                presenter.UpdatePresentation(RhythmTime{us});
                const auto &note = Node(root, "Lane.Note.1");
                const auto centre = note.Position();
                const auto &lane = Node(root, "ScrollGear.Lane");
                const auto aligned =
                    std::ranges::count_if(lane.Children(),
                                          [&](const auto &child)
                                          {
                                              const auto position = child->Position();
                                              const bool visibleMeasureLine =
                                                  child->Name() == "Lane.MeasureLine" && child->IsVisible();
                                              return visibleMeasureLine && Near(position.x, centre.x) &&
                                                     Near(position.y, centre.y);
                                          });
                Check(aligned == 1,
                      "Coincident note and bar must stay centred together under Whole/Separate and resize.");
                for (const auto &child : lane.Children())
                {
                    if (child->Name() != "Lane.MeasureLine" || !child->IsVisible())
                        continue;
                    const auto colour = child->GetComponent<SpriteVisualComponent>()->Style().normal;
                    Check(Near(child->NodeSize().height, 2) && Near(colour.alpha, .5F),
                          "Measure lines are 2 px at 50%, quieter than the notes (UI-02).");
                }
            }
        }
        presenter.Shutdown();
    }

    void TestCountOnlyHeadsAndCounters()
    {
        for (const int type : {11, 13, 15, 16})
        {
            AssetRenderer renderer;
            ScreenVisual2DManager visuals;
            visuals.Initialize(renderer, {1280, 720});
            PatternDocument pattern;
            pattern.mode = "Taiko";
            pattern.baseBpm = 120;
            pattern.patternOffsetMilliseconds = 500;
            pattern.notes = {{{0, {}}, type, 1, {}, {"HitCount=3"}}, {{0, {1, 2}}, type, 2}};
            auto loaded = TaikoMode{}.CreateSession(pattern);
            Check(loaded.Succeeded(), "Ungraded head feedback fixture must load.");
            MeshRenderSystem meshes;
            TextRenderSystem textRendering;
            AudioSystem audio;
            TextCatalog texts;
            GameplayPresenter presenter(visuals, texts);
            presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
            auto &root = visuals.FindCanvas(1)->Root();
            presenter.UpdatePresentation({});
            auto &counter = Node(root, "Hud.NoteCounter.1");
            const auto &remaining = *Node(counter, "RemainingHits").GetComponent<TextVisualComponent>();
            Check(!counter.IsVisible(), "A hit counter must stay hidden before the first accepted hit.");
            for (int index = 0; index < 3; ++index)
            {
                const char key = type == 16 && index == 1 ? 'D' : 'C';
                presenter.ApplyFeedback({.keyPressed = true,
                                         .result = loaded.session->ProcessInput(
                                             key, InputEdge::Pressed, RhythmTime{900'000 + index * 100'000})});
                if (index < 2 || type == 11 || type == 13)
                    CheckOverlay(root, "");
                Check(VisibleHitLines(root) == 0, "Count-only hits must not create a timing hit line.");
                const auto beam = Node(root, "Lane.KeyBeam").GetComponent<SpriteVisualComponent>()->Style().normal;
                Check(Near(beam.red, 1) && Near(beam.green, 1) && Near(beam.blue, 1),
                      "Count-only hits must use the ungraded white key beam.");
                presenter.UpdatePresentation(RhythmTime{900'000 + index * 100'000});
                const bool active = index < 2 || type == 11 || type == 13;
                Check(counter.IsVisible() == active, "Every active counted note must show its remaining hits.");
                if (active)
                    Check(remaining.Text() == std::to_wstring(2 - index),
                          "Each accepted hit must decrement the counter.");
            }
            if (type == 11 || type == 13)
            {
                Check(!Find(root, "Hud.NoteProcessing.1"), "Roll counters must not replace the moving roll head.");
                presenter.ApplyFeedback(
                    {.keyPressed = true,
                     .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{1'200'000})});
                presenter.UpdatePresentation(RhythmTime{1'200'000});
                Check(counter.IsVisible() && remaining.Text() == L"0",
                      "Extra roll hits must not underflow the counter.");
            }
            presenter.ApplyFeedback({.result = loaded.session->Update(RhythmTime{1'500'000})});
            CheckOverlay(root, "MAX");
            presenter.UpdatePresentation(RhythmTime{1'500'000});
            Check(!counter.IsVisible(), "The hit counter must disappear at completion.");
            loaded.session->Reset();
            presenter.ApplyFeedback({.reset = true});
            presenter.UpdatePresentation(RhythmTime{500'000});
            Check(!Node(root, "Hud.NoteCounter.1").IsVisible() &&
                      loaded.session->Gear().Lanes().front()->Notes().front()->Progress() == NoteProgress{0, 3},
                  "Reset must clear roll/balloon/DengDeng progress and hide its counter.");
            presenter.ApplyFeedback({.result = loaded.session->Update(RhythmTime{1'500'000})});
            CheckOverlay(root, "MISS");
            presenter.Shutdown();
        }
    }

    void TestDengDengWrongOrderFeedback()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {}}, 16, 1, {}, {"HitCount=3"}}, {{0, {1, 2}}, 16, 2}};
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Wrong-order presentation fixture must load.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        for (const char first : {'C', 'D'})
        {
            loaded.session->Reset();
            presenter.ApplyFeedback({.reset = true});
            const char other = first == 'C' ? 'D' : 'C';
            const std::array<char, 5> keys{first, first, other, other, first};
            const std::array<std::size_t, 5> counts{1, 1, 2, 2, 3};
            for (std::size_t index = 0; index < keys.size(); ++index)
            {
                presenter.ApplyFeedback(
                    {.keyPressed = true,
                     .result = loaded.session->ProcessInput(keys[index], InputEdge::Pressed,
                                                            RhythmTime{10'000 * static_cast<std::int64_t>(index)})});
                CheckOverlay(root, index == 4 ? "MAX" : "");
                Check(VisibleHitLines(root) == 0, "Wrong-order presses must not display a timing hit line.");
                const auto beam = Node(root, "Lane.KeyBeam").GetComponent<SpriteVisualComponent>()->Style().normal;
                Check(Near(beam.red, 1) && Near(beam.green, 1) && Near(beam.blue, 1),
                      "Wrong-order DengDeng input must remain white, never POOR/red.");
                Check(loaded.session->Gear().Lanes().front()->Notes().front()->Progress() ==
                          NoteProgress{counts[index], 3},
                      "The displayed count must stay unchanged on repeated actions.");
            }
        }
        presenter.Shutdown();
    }

    void TestScreenErrorLine()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        UiErrorLine line(visuals);
        Check(line.Canvas() == nullptr, "The error line must not create its Canvas before a diagnostic.");
        line.Present({L"Effect file", L"quality-regression.yme:4", L"[HitSound] invalid value"}, 1280, 720);
        auto &root = line.Canvas()->Root();
        const auto text = [&](const char *name) { return std::wstring(Node(root, name).GetComponent<TextVisualComponent>()->Text()); };
        Check(line.IsShown() && text("Hud.Error.Kind") == L"Effect file" &&
              text("Hud.Error.Location") == L"quality-regression.yme:4",
              "An effect diagnostic names its kind, file and line, and cause.");
        Check(text("Hud.Error.Cause") == L"[HitSound] invalid value" &&
              Node(root, "Hud.Error.LocationChip").IsVisible(),
              "An effect diagnostic names its kind, file and line, and cause.");
        const auto plate = Node(root, "Hud.Error").BoundsInCanvas();
        Check(Near(plate.y + plate.height, 360 - UiErrorLine::Top) && Near(plate.x + plate.width * .5F, 0) &&
                  plate.width < UiErrorLine::MaximumWidth,
              "The strip hangs 20 below the top edge, centred, as wide as its text.");
        line.Present({L"Audio", {}, std::wstring(400, L'x')}, 1280, 720);
        Check(!Node(root, "Hud.Error.LocationChip").IsVisible() &&
                  Near(Node(root, "Hud.Error").BoundsInCanvas().width, UiErrorLine::MaximumWidth),
              "A cause without a file hides the chip, and a long cause stops at the maximum width on one line.");
        visuals.OnResize(405, 720);
        line.OnResize(405, 720);
        Check(Node(root, "Hud.Error").BoundsInCanvas().width <= 405 - UiErrorLine::ScreenMargin * 2 + .01F,
              "A narrow screen keeps the screen margin.");
        line.Present({}, 1280, 720);
        Check(!line.IsShown(), "A cleared diagnostic hides the line.");
        line.Shutdown();
    }
    void TestHighlightJudgementCircle()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        for (int measure = 0; measure < 4; ++measure)
            pattern.notes.push_back({{measure, {}}, 1});
        EffectDocument effects;
        effects.commands.push_back({.position = {1, {}}, .type = EffectCommandType::Highlight, .beginValue = 1, .endValue = 1});
        effects.commands.push_back({.position = {2, {}}, .type = EffectCommandType::Highlight, .beginValue = 0, .endValue = 0});
        auto loaded = TaikoMode{}.CreateSession(pattern, effects);
        Check(loaded.Succeeded(), "Highlight fixture must load.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        const auto &timeline = loaded.session->Timeline();
        const auto start = timeline.Compile({1, {}}), end = timeline.Compile({2, {}});
        const auto gold = [&] { return Node(root, "Lane.JudgementCircle.Highlight").GetComponent<SpriteVisualComponent>()->Style().normal.alpha; };
        presenter.UpdatePresentation(start - RhythmDuration{10'000});
        Check(!Node(root, "Lane.JudgementCircle.Highlight").IsVisible() && !Node(root, "Lane.Highlight.Glow").IsVisible(),
              "Outside Highlight the judgement circle is unchanged.");
        presenter.UpdatePresentation(start + RhythmDuration{125'000});
        Check(gold() > .4F && gold() < .7F, "Highlight fades in over 250 ms of song time.");
        presenter.UpdatePresentation(start + RhythmDuration{250'000});
        Check(Near(gold(), 1) && Node(root, "Lane.Highlight.Glow").IsVisible() && Node(root, "Lane.JudgementCircle.HighlightRing").IsVisible(),
              "Full Highlight shows the gold circle, the glow and the ring.");
        // 120 BPM: a beat is 500 ms. Just after a beat the pulse is near 1; late in the beat near 0.
        presenter.UpdatePresentation(start + RhythmDuration{510'000});
        const auto beat = Node(root, "Lane.JudgementCircle.HighlightRing").NodeSize().width;
        presenter.UpdatePresentation(start + RhythmDuration{900'000});
        Check(Node(root, "Lane.JudgementCircle.HighlightRing").NodeSize().width < beat,
              "The ring is largest on the beat and shrinks before the next one.");
        presenter.UpdatePresentation(end - RhythmDuration{10'000});
        presenter.UpdatePresentation(end + RhythmDuration{125'000});
        Check(gold() > 0 && gold() < 1, "Highlight fades out after its end.");
        presenter.UpdatePresentation(start - RhythmDuration{1});
        Check(!Node(root, "Lane.JudgementCircle.Highlight").IsVisible(), "A backward seek lands on the new state without fading.");
        const auto glow = Node(root, "Lane.Highlight").ClipRect();
        Check(glow.has_value() && Near(glow->height, Node(root, "ScrollGear.Lane").NodeSize().height),
              "The glow is clipped to the lane, never over the input panel.");
        presenter.Shutdown();
    }
    void TestPresenterEventPriority()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280.0F, 720.0F});
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 180;
        PatternNote note;
        note.keyType = 1;
        pattern.notes.push_back(note);
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Presenter fixture must build.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        NoteProcessResult rollover;
        rollover.events.push_back(Event(NoteEventType::Missed, JudgementGrade::Miss));
        rollover.events.push_back(Event(NoteEventType::HitAccepted, JudgementGrade::Max));
        rollover.events.push_back(Event(NoteEventType::Completed, JudgementGrade::Max));
        presenter.ApplyFeedback({.keyPressed = true, .result = rollover});
        CheckOverlay(root, "MAX");
        presenter.ApplyFeedback({.reset = true});
        CheckOverlay(root, "");
        NoteProcessResult empty;
        empty.events.push_back(Event(NoteEventType::InputRejected, JudgementGrade::Miss));
        empty.events.push_back(Event(NoteEventType::Missed, JudgementGrade::Miss));
        presenter.ApplyFeedback({.keyPressed = true, .result = empty});
        CheckOverlay(root, "");
        const auto beam = Node(root, "Lane.KeyBeam").GetComponent<SpriteVisualComponent>()->Style().normal;
        Check(Near(beam.red, 1.0F) && Near(beam.green, 1.0F) && Near(beam.blue, 1.0F),
              "Out-of-range input must only produce the white key beam.");
        Check(Near(beam.alpha, 1.0F), "The default key beam opacity draws the beam as before.");
        // The player's opacity scales the beam of every mode, through its fade.
        presenter.ConfigureKeyBeamOpacity(40);
        presenter.ApplyFeedback({.keyPressed = true, .result = empty});
        const auto dimmed = [&] { return Node(root, "Lane.KeyBeam").GetComponent<SpriteVisualComponent>()->Style().normal; };
        Check(Near(dimmed().alpha, 0.4F) && Near(dimmed().red, 1.0F), "A 40% key beam starts at 40% alpha.");
        presenter.AdvanceEffects(0.1);
        Check(Near(dimmed().alpha, 0.2F), "The fade keeps the chosen opacity as its ceiling.");
        presenter.ConfigureKeyBeamOpacity(0);
        presenter.ApplyFeedback({.keyPressed = true, .result = empty});
        Check(Near(dimmed().alpha, 0.0F), "A 0% key beam is invisible.");
        presenter.ConfigureKeyBeamOpacity(100);
        // Background brightness darkens the background toward black (osu!'s dim) and leaves its alpha.
        const auto backgroundColor = [&] { return Node(root, "Background").GetComponent<SpriteVisualComponent>()->Style().normal; };
        const auto plain = backgroundColor();
        presenter.ConfigureBackgroundBrightness(25);
        Check(Near(backgroundColor().red, plain.red * .25F) && Near(backgroundColor().blue, plain.blue * .25F) &&
                  Near(backgroundColor().alpha, 1.0F),
              "A 25% background brightness scales the background colour to a quarter.");
        presenter.ConfigureBackgroundBrightness(0);
        Check(Near(backgroundColor().red, 0.0F) && Near(backgroundColor().green, 0.0F), "0% draws the background black.");
        presenter.ConfigureBackgroundBrightness(100);
        Check(Near(backgroundColor().red, plain.red), "100% restores the background as it was.");
        NoteProcessResult wrong;
        wrong.events.push_back(Event(NoteEventType::InputRejected, JudgementGrade::Perfect));
        presenter.ApplyFeedback({.keyPressed = false, .result = wrong});
        CheckOverlay(root, "");
        presenter.ApplyFeedback({.keyPressed = true, .result = wrong});
        CheckOverlay(root, "POOR");
        presenter.Shutdown();
    }
    void TestRollCompletionAndProcessedTicks()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.baseBpm = 120;
        pattern.notes = {{{0, {}}, 12, 1, {}, {"TickDivision=16"}}, {{0, {1, 4}}, 12, 2}};
        auto loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Tick presentation fixture must load.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        const auto visibleTicks = [&]
        {
            std::size_t count = 0;
            const auto visit = [&](const auto &self, const Visual2DNode &node) -> void
            {
                if (node.Name() == "LongNote.Tick" && node.IsVisible())
                    ++count;
                for (const auto &child : node.Children())
                    self(self, *child);
            };
            visit(visit, root);
            return count;
        };
        presenter.UpdatePresentation(RhythmTime{});
        Check(visibleTicks() == 4, "All body ticks, including the aligned tail, must start visible.");
        presenter.ApplyFeedback(
            {.keyPressed = true, .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{})});
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{75'000})});
        presenter.UpdatePresentation(RhythmTime{75'000});
        Check(visibleTicks() == 3, "An early accepted tick must disappear before its scheduled time.");
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{250'000})});
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{375'000})});
        presenter.UpdatePresentation(RhythmTime{375'000});
        Check(visibleTicks() == 1, "The unprocessed tail tick must remain visible after the other ticks.");
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{500'000})});
        presenter.UpdatePresentation(RhythmTime{500'000});
        Check(visibleTicks() == 0, "An accepted tail tick must disappear immediately.");
        presenter.ApplyFeedback({.result = loaded.session->Update(RhythmTime{500'000})});
        CheckOverlay(root, "MAX");
        presenter.AdvanceEffects(.2);
        CheckOverlay(root, "");
        loaded.session->Reset();
        presenter.ApplyFeedback({.reset = true});
        presenter.UpdatePresentation(RhythmTime{});
        Check(visibleTicks() == 4, "Reset must restore consumed tick sprites, including the tail.");
        presenter.ApplyFeedback({.result = loaded.session->Update(RhythmTime{500'000})});
        CheckOverlay(root, "MISS");
        presenter.UpdatePresentation(RhythmTime{500'000});
        Check(visibleTicks() == 0, "Expired/missed ticks must also be hidden.");
        presenter.Shutdown();
        pattern.notes.back().position = {0, {1, 1}};
        pattern.notes.front().extraData = {"TickDivision=8"};
        pattern.timing.push_back({{0, {1, 2}}, TimingDirectiveType::DelayMilliseconds, -750});
        loaded = TaikoMode{}.CreateSession(pattern);
        Check(loaded.Succeeded(), "Negative-delay tick fixture must load.");
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &delayedRoot = visuals.FindCanvas(2)->Root();
        const auto visibleDelayedTicks = [&]
        {
            return std::ranges::count_if(Node(delayedRoot, "Lane.Note.1").Children(), [](const auto &child)
                                         { return child->Name() == "LongNote.Tick" && child->IsVisible(); });
        };
        const auto minimumTickY = [&]
        {
            float minimum = std::numeric_limits<float>::infinity();
            for (const auto &child : Node(delayedRoot, "Lane.Note.1").Children())
                if (child->Name() == "LongNote.Tick" && child->IsVisible())
                    minimum = std::min(minimum, child->Bounds().y);
            return minimum;
        };
        presenter.ApplyFeedback(
            {.keyPressed = true, .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{})});
        presenter.UpdatePresentation(RhythmTime{200'000});
        const auto firstY = minimumTickY();
        Check(visibleDelayedTicks() == 5,
              "Overwritten ticks must not be presented or judged; the active tail remains.");
        presenter.ApplyFeedback(
            {.keyPressed = true,
             .result = loaded.session->ProcessInput('C', InputEdge::Pressed, RhythmTime{200'000})});
        presenter.UpdatePresentation(RhythmTime{200'000});
        Check(std::isfinite(firstY) && minimumTickY() > firstY && visibleDelayedTicks() == 4,
              "Surviving processed ticks must disappear after overwritten ticks have been excluded.");
        presenter.Shutdown();
    }
    std::string Number(Visual2DNode &root, std::string_view name)
    {
        // HUD numbers are ASCII digits and signs, so each character narrows without loss.
        const auto text = Node(root, std::string(name)).GetComponent<TextVisualComponent>()->Text();
        std::string number;
        number.reserve(text.size());
        for (const wchar_t character : text)
            number.push_back(static_cast<char>(character));
        return number;
    }
    void TestStatisticsHud()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280, 720});
        auto parsed = ChartParser{}.ParsePattern(
            "[Metadata]\nName: HUD\nBase BPM: 120\n[Difficulty]\nMode: Taiko\n"
            "[Time Signature]\n[Pattern]\n0/1,1,0\n1/4,1,0\n1/2,1,0\n", "hud.ymp");
        auto loaded = TaikoMode{}.CreateSession(parsed.document);
        Check(loaded.Succeeded(), "HUD fixture must load.");
        MeshRenderSystem meshes;
        TextRenderSystem textRendering;
        AudioSystem audio;
        TextCatalog texts;
        GameplayPresenter presenter(visuals, texts);
        presenter.Initialize({meshes, textRendering, renderer, audio, 1280, 720}, *loaded.session);
        auto &root = visuals.FindCanvas(1)->Root();
        const auto hit = [&](char key, std::int64_t time)
        {
            static_cast<void>(loaded.session->ProcessInput(key, InputEdge::Pressed, RhythmTime{time}));
            static_cast<void>(loaded.session->ProcessInput(key, InputEdge::Released, RhythmTime{time}));
        };
        const auto numberNodes = [&] { return NodeCount(Node(root, "Hud.JudgeCountsPanel")) + NodeCount(Node(root, "Hud.ScorePanel")); };
        const auto nodes = numberNodes(), loads = renderer.loads;
        presenter.UpdatePresentation({});
        Check(Number(root, "Hud.Combo") == "0" && Number(root, "Hud.Score") == "0",
              "New attempts show zero combo/score.");
        Check(Find(Node(root, "Input.Panel"), "Hud.Combo") != nullptr,
              "Combo must belong to the key layout and inherit its transform.");
        const auto combo = Node(root, "Hud.Combo").Bounds();
        for (const auto &child : Node(root, "Input.Panel").Children())
            if (child->Name() == "Input.Key.Face")
                Check(combo.y + combo.height <= child->Bounds().y,
                      "Combo must fit beneath the existing key faces.");
        hit('C', 0);
        presenter.UpdatePresentation({});
        Check(Number(root, "Hud.Score") == "33333333" && Number(root, "Hud.Combo") == "1" &&
                  Number(root, "Hud.JudgementCount0") == "1",
              "One perfect note out of three earns one third of the maximum score, one combo and one MAX.");
        hit('D', 500'000);
        hit('C', 500'000);
        static_cast<void>(loaded.session->Update(RhythmTime{1'200'000}));
        presenter.UpdatePresentation(RhythmTime{1'200'000});
        Check(Number(root, "Hud.Score") == "33333333" && Number(root, "Hud.Combo") == "0" &&
              Number(root, "Hud.JudgementCount5") == "1",
              "POOR and automatic MISS update exact score/counts and break combo.");
        Check(Number(root, "Hud.JudgementCount7") == "1",
              "POOR and automatic MISS update exact score/counts and break combo.");
        loaded.session->RewindJudgements(RhythmTime{500'000});
        presenter.UpdatePresentation(RhythmTime{500'000});
        Check(Number(root, "Hud.Score") == "33333333" && Number(root, "Hud.Combo") == "1" &&
              Number(root, "Hud.JudgementCount5") == "0", "Rewind immediately restores all visible HUD values.");
        Check(Number(root, "Hud.JudgementCount7") == "0", "Rewind immediately restores all visible HUD values.");
        for (const auto width : {405U, 1280U, 1920U})
        {
            visuals.OnResize(width, 720);
            presenter.OnResize(width, 720);
            for (std::size_t i = 0; i < 8; ++i)
            {
                const auto bounds = Node(root, "Hud.JudgementCount" + std::to_string(i)).BoundsInCanvas();
                Check(bounds.x >= -static_cast<float>(width) * .5F &&
                          bounds.x + bounds.width <= static_cast<float>(width) * .5F,
                      "Counters must wrap within narrow and wide viewports.");
                if (width >= 1280)
                    Check(bounds.x >= static_cast<float>(width) * .5F - 496 && bounds.y + bounds.height <= -232,
                          "The bottom-right count panel must leave the central processing area free.");
            }
        }
        loaded.session->Reset();
        presenter.UpdatePresentation({});
        Check(Number(root, "Hud.Score") == "0" && Number(root, "Hud.Combo") == "0" &&
                  Number(root, "Hud.JudgementCount0") == "0", "Reset clears every visible statistic.");
        Check(numberNodes() == nodes && renderer.loads == loads,
              "Updates/resizes reuse every number node and image independently of note pooling.");
        const auto accuracy = Node(root, "Hud.Accuracy").BoundsInCanvas();
        const auto panel = Node(root, "Hud.ScorePanel").BoundsInCanvas();
        Check(Near(panel.y + panel.height, 360 - 24) && accuracy.x >= panel.x && Near(accuracy.width, 133),
              "Accuracy keeps a fixed 100.00% width in the top-right panel, left of the score.");
        Check(accuracy.x + accuracy.width <= Node(root, "Hud.Score").BoundsInCanvas().x,
              "Accuracy keeps a fixed 100.00% width in the top-right panel, left of the score.");
        presenter.Shutdown();
    }
    void TestHpGauge()
    {
        AssetRenderer renderer;
        ScreenVisual2DManager visuals;
        visuals.Initialize(renderer, {1280.0F, 720.0F});
        Visual2DCanvas canvas;
        const auto baseline = NodeCount(canvas.Root());
        GameplayHpView view;
        view.Initialize(visuals, canvas.Root());
        view.SetLayout(1280, 160);
        auto &frame = Node(canvas.Root(), "Hud.HP.Frame");
        auto &fill = Node(canvas.Root(), "Hud.HP.Fill");
        auto &marker = Node(canvas.Root(), "Hud.HP.ClearMarker");
        auto &lock = Node(canvas.Root(), "Hud.HP.Lock");
        Check(NodeCount(canvas.Root()) == baseline + 4 && frame.Children().size() == 3,
              "HP consists of the frame, one fill, the clear marker and the lock.");
        Check(Near(frame.Bounds().x, -640) && Near(frame.Bounds().y, 176) && Near(frame.Bounds().width, 490),
              "The HP frame starts at the left screen edge, 16 above the gear.");
        Check(Near(frame.Bounds().height, 24), "The HP frame starts at the left screen edge, 16 above the gear.");
        Check(!fill.IsVisible() && !lock.IsVisible(), "Starting HP is zero and unlocked.");
        const auto image = [&](Visual2DNode &node)
        { return renderer.paths.at(node.GetComponent<SpriteVisualComponent>()->Style().normalImage.value).filename().wstring(); };
        const auto nodes = NodeCount(canvas.Root());
        for (const auto percent : {30.0, 59.9, 60.0, 100.0, 0.0})
        {
            view.Update({percent, 60.0, false});
            const auto *sprite = fill.GetComponent<SpriteVisualComponent>();
            Check(Near(fill.Bounds().x, 32) && Near(fill.Bounds().width, 454 * static_cast<float>(percent / 100)) &&
                      Near(sprite->UvScale().x, static_cast<float>(percent / 100)),
                  "The fill is cropped from the left to the live gauge value, never stretched.");
            Check(percent == 0 || image(fill) == (percent >= 60.0 ? L"FillClear.png" : L"FillLow.png"),
                  "The fill switches to the Clear picture at the clear threshold.");
        }
        Check(Near(marker.Bounds().x + marker.Bounds().width / 2, 32 + 454 * .6F),
              "The clear marker sits at the gauge's clear threshold.");
        view.Update({60, 80.0, true});
        Check(Near(marker.Bounds().x + marker.Bounds().width / 2, 32 + 454 * .8F) && image(fill) == L"FillLow.png" &&
                  lock.IsVisible(),
              "Another gauge threshold moves the marker; the lock appears once HP is disabled.");
        view.SetLayout(800, 160);
        Check(Near(frame.Bounds().x, -400) && Near(fill.Bounds().width, 454 * .6F) && lock.IsVisible(),
              "Resizing preserves HP and the lock without allocating new nodes.");
        Check(NodeCount(canvas.Root()) == nodes, "Resizing preserves HP and the lock without allocating new nodes.");
        view.Shutdown();
    }
} // namespace

int main()
{
    try
    {
        std::string error;
        Check(InitializeBuiltInAssets(error), error.c_str());
        TestGuideAndOverlays();
        TestGameplayProgress();
        TestHpGauge();
        TestStatisticsHud();
        TestSlowNotesDoNotExpandAllVisuals();
        TestMeasureLinesShareNoteMotion();
        TestCountOnlyHeadsAndCounters();
        TestDengDengWrongOrderFeedback();
        TestScreenErrorLine();
        TestHighlightJudgementCircle();
        TestPresenterEventPriority();
        TestRollCompletionAndProcessedTicks();
        std::puts("Gameplay progress and judgement guide/marker/overlay tests passed.");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    catch (...)
    {
        std::fputs("Unknown judgement test failure.\n", stderr);
        return 1;
    }
}
