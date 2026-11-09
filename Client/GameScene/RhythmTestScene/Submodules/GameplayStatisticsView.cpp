#include "GameplayStatisticsView.h"
#include "App/AssetPaths.h"

namespace
{
    Visual2DNode &Label(Visual2DNode &parent, std::string name, std::wstring text, TextCatalog &texts)
    {
        auto &node = CreateLabel(parent, {}, std::move(text), std::move(name));
        node.SetZIndex(5);
        auto &visual = *node.GetComponent<TextVisualComponent>();
        texts.ApplyFont(visual);
        visual.SetFontSize(13);
        visual.SetTextColor({.12F, .16F, .24F, 1});
        visual.SetHorizontalAlignment(TextAlignment::Center);
        return node;
    }
}
void GameplayStatisticsView::Initialize(ScreenVisual2DManager &visuals, Visual2DNode &root,
                                        Visual2DNode &inputPanel, TextCatalog &texts)
{
    Shutdown();
    const auto atlas = visuals.RegisterImage(InGame(L"HudDigits"));
    combo_.Initialize(inputPanel, atlas, "Hud.Combo");
    comboLabel_ = &Label(inputPanel, "Hud.ComboLabel", std::wstring(texts.Get("Gameplay.combo")), texts);
    comboLabel_->GetComponent<TextVisualComponent>()->SetFontSize(9);
    score_.Initialize(root, atlas, "Hud.Score");
    scoreLabel_ = &Label(root, "Hud.ScoreLabel", std::wstring(texts.Get("Result.score")), texts);
    const std::array<std::wstring_view, 8> names{
        texts.Get("Judgements.MAX"), texts.Get("Judgements.PERFECT"), texts.Get("Judgements.GREAT"),
        texts.Get("Judgements.GOOD"), texts.Get("Judgements.BAD"), texts.Get("Judgements.MISS"),
        texts.Get("Judgements.OK"), texts.Get("Judgements.POOR")};
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        const auto name = std::to_string(i);
        counts_[i].Initialize(root, atlas, "Hud.JudgementCount" + name);
        labels_[i] = &Label(root, "Hud.JudgementLabel" + name, std::wstring(names[i]), texts);
        // Keep names readable over either light or dark chart backgrounds.
        labels_[i]->GetComponent<TextVisualComponent>()->SetTextOffset({1, -1});
        auto &foreground = Label(*labels_[i], "Foreground", std::wstring(names[i]), texts);
        foreground.GetComponent<TextVisualComponent>()->SetTextColor({1, 1, 1, 1});
    }
}
void GameplayStatisticsView::SetLayout(float width, Size inputPanel, Rect accuracy)
{
    if (!comboLabel_)
        return;
    comboLabel_->SetBounds({6, 4, std::max(inputPanel.width - 12, 1.0F), 10});
    combo_.SetLayout({6, 14, std::max(inputPanel.width - 12, 1.0F), 28}, TextAlignment::Center);
    const float scoreWidth = std::max(1.0F, std::min(320.0F, width - 40));
    const float scoreLeft = width * .5F - 20 - scoreWidth;
    scoreLabel_->SetBounds({scoreLeft, accuracy.y - 19, scoreWidth, 15});
    scoreLabel_->GetComponent<TextVisualComponent>()->SetHorizontalAlignment(TextAlignment::Trailing);
    score_.SetLayout({scoreLeft, accuracy.y - 59, scoreWidth, 40});
    const float available = std::max(width - 40, 1.0F);
    // Reserve the centre for Balloon/DengDeng processing visuals.
    const auto columns = std::clamp(static_cast<std::size_t>(available / 90), std::size_t{1}, std::size_t{4});
    const float cellWidth = std::min(90.0F, available / static_cast<float>(columns));
    const float left = width * .5F - 20 - cellWidth * static_cast<float>(columns);
    for (std::size_t i = 0; i < counts_.size(); ++i)
    {
        const float x = left + static_cast<float>(i % columns) * cellWidth;
        const float top = -12 - static_cast<float>(i / columns) * 58;
        labels_[i]->SetBounds({x, top - 16, cellWidth, 16});
        labels_[i]->Children().front()->SetBounds({0, 0, cellWidth, 16});
        counts_[i].SetLayout({x + 4, top - 49, std::max(cellWidth - 8, 1.0F), 32}, TextAlignment::Center);
    }
}
void GameplayStatisticsView::Update(const PlaySession &session)
{
    const auto statistics = session.Statistics();
    combo_.SetValue(statistics.combo);
    score_.SetValue(session.Score());
    for (std::size_t i = 0; i < statistics.judgements.size(); ++i)
        counts_[i].SetValue(statistics.judgements[i]);
    counts_[6].SetValue(statistics.okCount);
    counts_[7].SetValue(statistics.poorCount);
}
void GameplayStatisticsView::Shutdown() noexcept
{
    combo_.Reset();
    score_.Reset();
    for (auto &count : counts_) count.Reset();
    labels_ = {};
    comboLabel_ = scoreLabel_ = nullptr;
}
