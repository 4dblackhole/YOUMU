#include "GameplayStatisticsView.h"
#include "App/AssetPaths.h"
#include "Presentation/LaneKeyBeam.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include <algorithm>
#include <string>

using namespace PhantomCore;

namespace
{
    constexpr float HalfHeight = 360;
    // Top-right panel: 24 from the top, 56 tall. Accuracy takes a fixed 133 so
    // the nine-digit score beside it never moves it.
    constexpr float ScoreTop = 24, ScoreHeight = 56, ScoreWidth = 300;
    constexpr float AccuracyLeft = 30, AccuracyWidth = 133, ScoreGap = 14, ScoreRight = 31;
    // Bottom-right panel: two rows of four judgement cells.
    constexpr float CountsTop = 592, CountsHeight = 76, CountsWidth = 496, CountsInset = 30;
    constexpr float CellHeight = 28, CellRowStep = 32, CellTop = 8;

    Visual2DNode &Text(Visual2DNode &parent, std::string name, std::wstring text, PhantomCore::TextFont font,
                       float size, std::uint16_t weight, Color color, TextAlignment alignment)
    {
        auto &node = CreateLabel(parent, {}, std::move(text), std::move(name));
        node.SetZIndex(1);
        auto &visual = *node.GetComponent<TextVisualComponent>();
        visual.SetFont(std::move(font));
        visual.SetFontSize(size);
        visual.SetFontWeight(weight);
        visual.SetTextColor(color);
        visual.SetHorizontalAlignment(alignment);
        return node;
    }
    Visual2DNode &Number(Visual2DNode &parent, std::string name, float size, Color color, TextAlignment alignment,
                         bool italic = true)
    {
        auto face = ui_fonts::Number(800, italic);
        return Text(parent, std::move(name), L"0", std::move(face.font), size, face.weight, color, alignment);
    }
    // Count dots keep the established judgement colours.
    Color CountColor(std::size_t index)
    {
        constexpr std::array<Color, 3> others{ui_colors::Rgb(0x829AB3), ui_colors::Rgb(0x4F9FDF), ui_colors::Rgb(0xFF3A26)};
        return index < 5 ? LaneKeyBeam::GradeColor(static_cast<JudgementGrade>(index)) : others[index - 5];
    }
} // namespace
void GameplayStatisticsView::Value::Set(std::uint64_t value)
{
    if (!node || shown == value)
        return;
    node->GetComponent<TextVisualComponent>()->SetText(std::to_wstring(value));
    shown = value;
}
void GameplayStatisticsView::Initialize(ScreenVisual2DManager &visuals, Visual2DNode &root,
                                        Visual2DNode &inputPanel, TextCatalog &texts)
{
    Shutdown();
    using namespace ui_colors::theme;
    combo_.node = &Number(inputPanel, "Hud.Combo", 26, Ink, TextAlignment::Center);
    const std::wstring comboCaption(texts.Get("Gameplay.combo"));
    comboLabel_ = &Text(inputPanel, "Hud.ComboLabel", comboCaption, ui_fonts::Text(comboCaption), 13, 700, InkSoft,
                        TextAlignment::Center);
    scorePanel_ = &CreateSprite(root, {}, visuals.RegisterImage(InGame(L"Hud.ScorePanel")), "Hud.ScorePanel");
    scorePanel_->SetZIndex(4);
    score_.node = &Number(*scorePanel_, "Hud.Score", 20, InkSoft, TextAlignment::Trailing);
    countsPanel_ = &CreateSprite(root, {}, visuals.RegisterImage(InGame(L"Hud.JudgeCountsPanel")), "Hud.JudgeCountsPanel");
    countsPanel_->SetZIndex(4);
    const std::array<std::wstring_view, 8> names{
        texts.Get("Judgements.MAX"), texts.Get("Judgements.PERFECT"), texts.Get("Judgements.GREAT"),
        texts.Get("Judgements.GOOD"), texts.Get("Judgements.BAD"), texts.Get("Judgements.MISS"),
        texts.Get("Judgements.OK"), texts.Get("Judgements.POOR")};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const auto name = std::to_string(i);
        dots_[i] = &CreatePanel(*countsPanel_, {}, "Hud.JudgementDot" + name);
        auto &dot = *dots_[i]->GetComponent<SpriteVisualComponent>();
        dot.SetTint(CountColor(i));
        dot.SetCornerRadius(3);
        auto face = ui_fonts::Number(700, false);
        labels_[i] = &Text(*countsPanel_, "Hud.JudgementLabel" + name, std::wstring(names[i]), std::move(face.font), 12,
                           face.weight, InkSoft, TextAlignment::Leading);
        counts_[i].node = &Number(*countsPanel_, "Hud.JudgementCount" + name, 14, Ink, TextAlignment::Trailing, false);
    }
}
void GameplayStatisticsView::SetLayout(float width, Size inputPanel)
{
    if (!comboLabel_)
        return;
    // Combo sits under the keys inside the key layout: the number, then its caption.
    combo_.node->SetBounds({0, 17, inputPanel.width, 26});
    comboLabel_->SetBounds({0, 1, inputPanel.width, 14});

    const float scoreWidth = std::min(ScoreWidth, std::max(width, 1.0F));
    const Rect scorePanel{width * 0.5F - scoreWidth, HalfHeight - ScoreTop - ScoreHeight, scoreWidth, ScoreHeight};
    scorePanel_->SetBounds(scorePanel);
    const float accuracyWidth = std::min(AccuracyWidth, scoreWidth - AccuracyLeft);
    accuracyBounds_ = {scorePanel.x + AccuracyLeft, scorePanel.y + 10, accuracyWidth, 36};
    const float scoreLeft = AccuracyLeft + accuracyWidth + ScoreGap;
    score_.node->SetBounds({scoreLeft, 14, std::max(scoreWidth - scoreLeft - ScoreRight, 1.0F), 28});

    const float countsWidth = std::min(CountsWidth, std::max(width, 1.0F));
    countsPanel_->SetBounds({width * 0.5F - countsWidth, HalfHeight - CountsTop - CountsHeight, countsWidth, CountsHeight});
    const float cellStep = (countsWidth - CountsInset) / 4;
    const float cellWidth = std::max(cellStep - 6, 1.0F);
    for (std::size_t i = 0; i < counts_.size(); ++i)
    {
        const float x = CountsInset + static_cast<float>(i % 4) * cellStep;
        const float y = CountsHeight - CellTop - CellHeight - static_cast<float>(i / 4) * CellRowStep;
        dots_[i]->SetBounds({x + 8, y + (CellHeight - 6) * 0.5F, 6, 6});
        labels_[i]->SetBounds({x + 18, y, std::max(cellWidth - 18 - 24, 1.0F), CellHeight});
        counts_[i].node->SetBounds({x + cellWidth - 60 - 8, y, 60, CellHeight});
    }
}
void GameplayStatisticsView::Update(const PlaySession &session)
{
    const auto statistics = session.Statistics();
    combo_.Set(statistics.combo);
    score_.Set(session.Score());
    for (std::size_t i = 0; i < statistics.judgements.size(); ++i)
        counts_[i].Set(statistics.judgements[i]);
    counts_[6].Set(statistics.okCount);
    counts_[7].Set(statistics.poorCount);
}
void GameplayStatisticsView::Shutdown() noexcept
{
    combo_ = score_ = {};
    counts_ = {};
    labels_ = dots_ = {};
    comboLabel_ = scorePanel_ = countsPanel_ = nullptr;
    accuracyBounds_ = {};
}
