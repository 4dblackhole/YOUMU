#include "GameplayJudgementView.h"
#include "App/AssetPaths.h"
#include "Presentation/LaneKeyBeam.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include "Texts/GameScene/RhythmTestScene/GameplayTexts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

using namespace PhantomCore;

namespace
{

    // The default JudgeLevel 50 guide (BAD +/-90ms plus a 10ms margin each side) is 420 wide.
    constexpr float ReferencePixelsPerMicrosecond = 420.0F / 200'000.0F;
    constexpr float BarHeight = 12.0F;
    // The bar's bottom is 54 below the gear, where it sat on the former 490x48 timing panel.
    constexpr float BarBelowGear = 54.0F;
    // Hit lines follow osu!'s bar hit error meter defaults: 4 thick, 0.6 peak alpha,
    // 100ms OutQuint fade-in, 5s fade-out, at most 50 live lines (older ones fade in 100ms).
    constexpr float LineThickness = 4.0F, LineHeight = 24.0F, LinePeakAlpha = 0.6F;
    constexpr double LineFadeInSeconds = 0.1, LineFadeOutSeconds = 5.0, LineQuickFadeSeconds = 0.1;
    constexpr std::size_t MaxLiveLines = 50;
    constexpr float CaptionBottom = BarHeight * 0.5F + LineHeight * 0.5F + 4.0F;
    constexpr std::array<std::string_view, 8> ImageNames{"MAX", "PERFECT", "GREAT", "GOOD",
                                                         "BAD", "MISS",    "POOR",  "OK"};
    constexpr std::size_t PoorImageIndex = 6, OkImageIndex = 7;
    constexpr Color Ink = ui_colors::theme::Ink;

    void Tint(Visual2DNode &node, const Color color)
    {
        node.GetComponent<SpriteVisualComponent>()->SetTint(color);
    }

    // Only real timing judgements get a hit line. Roll/count-only ticks have no
    // grade; an automatic miss does not represent a player's input timestamp.
    bool HasInputTiming(const NoteEvent &event) noexcept
    {
        return (event.type == NoteEventType::HitAccepted || event.type == NoteEventType::HoldStarted ||
                event.type == NoteEventType::TickAccepted || event.type == NoteEventType::InputRejected) &&
               IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Bad);
    }

    double OutQuint(const double progress) noexcept
    {
        return 1.0 - std::pow(1.0 - progress, 5.0);
    }
} // namespace

void GameplayJudgementView::Initialize(ScreenVisual2DManager &visuals, Visual2DCanvas &canvas,
                                       Visual2DNode &judgementCircle, const AccuracyRange &baseRange,
                                       TextCatalog &texts)
{
    if (guide_ != nullptr)
        throw std::logic_error("Judgement view is already initialized.");
    for (std::size_t index = 0; index < halfWindows_.size(); ++index)
        halfWindows_[index] = baseRange.Bands()[index].halfWindow;
    displayHalfWindow_ = baseRange.HalfWindow(JudgementGrade::Bad) + RhythmDuration{10'000};
    visuals_ = &visuals;
    CreateTimingGuide(canvas.Root(), texts);
    CreateJudgementImages(visuals, judgementCircle);
    OnResize(canvas.LogicalSize().width);
    Reset();
}

void GameplayJudgementView::CreateTimingGuide(Visual2DNode &root, TextCatalog &texts)
{
    guide_ = &root.CreateChild("Hud.JudgementGuide");
    guide_->SetPosition({0.0F, -BarBelowGear});
    guide_->SetZIndex(5);
    CreateTimingBands();
    CreateTimingCaptions(texts);
    CreateHitLines();
}

void GameplayJudgementView::CreateTimingBands()
{
    const float halfWidth = static_cast<float>(displayHalfWindow_.count()) * ReferencePixelsPerMicrosecond;
    margin_ = &CreatePanel(*guide_, {-halfWidth, 0.0F, halfWidth * 2.0F, BarHeight}, "Judgement.Margin");
    Tint(*margin_, ui_colors::Rgb(0xC6D3E1));
    // Wide outer bands first; smaller bands cover the centre without gaps.
    for (std::size_t reverse = bands_.size(); reverse > 0; --reverse)
    {
        const std::size_t index = reverse - 1;
        bands_[index] = &CreatePanel(*guide_, {}, "Judgement.Band." + std::to_string(index));
        bands_[index]->SetZIndex(static_cast<int>(bands_.size() - index));
        Tint(*bands_[index], LaneKeyBeam::GradeColor(static_cast<JudgementGrade>(index)));
    }
}

void GameplayJudgementView::CreateTimingCaptions(TextCatalog &texts)
{
    const auto &text = Gameplay(texts);
    const std::array<std::wstring_view, 3> captions{text.early, L"0", text.late};
    for (std::size_t index = 0; index < labels_.size(); ++index)
    {
        labels_[index] =
            &CreateLabel(*guide_, {}, std::wstring(captions[index]), "Judgement.Caption." + std::to_string(index));
        auto &label = *labels_[index]->GetComponent<TextVisualComponent>();
        auto face = ui_fonts::Number(700, false);
        label.SetFont(std::move(face.font));
        label.SetFontWeight(face.weight);
        label.SetFontSize(12.0F);
        label.SetTextColor(ui_colors::theme::InkSoft);
        label.SetHorizontalAlignment(index == 0   ? TextAlignment::Leading
                                     : index == 1 ? TextAlignment::Center
                                                  : TextAlignment::Trailing);
    }
}

void GameplayJudgementView::CreateHitLines()
{
    auto &zero = CreatePanel(*guide_, {-0.5F, -1.0F, 1.0F, BarHeight + 2.0F}, "Judgement.Zero");
    zero.SetZIndex(6);
    Tint(zero, Ink);
    for (std::size_t i = 0; i < hitLines_.size(); ++i)
    {
        auto &line = hitLines_[i];
        line.node = &CreatePanel(*guide_, {-LineThickness * 0.5F, 0.0F, LineThickness, LineHeight},
                                 "Judgement.HitLine." + std::to_string(i));
        line.node->SetPivot({0.5F, 0.5F});
        line.node->SetZIndex(7);
    }
}

void GameplayJudgementView::CreateJudgementImages(ScreenVisual2DManager &visuals, Visual2DNode &judgementCircle)
{
    const float size = judgementCircle.NodeSize().width * 1.4F;
    for (std::size_t index = 0; index < judgementImages_.size(); ++index)
    {
        const std::string file = "Judgements." + std::string(ImageNames[index]);
        const auto image = visuals.RegisterImage(InGame(file));
        const auto nativeSize = visuals.GetImageSize(image);
        if (nativeSize.width <= 0.0F || nativeSize.height <= 0.0F)
            throw std::logic_error("Judgement skin image must have a non-zero size.");
        const float height = size * nativeSize.height / nativeSize.width;
        auto &node = CreateSprite(judgementCircle, {0.0F, 0.0F, size, height}, image,
                                     "Lane.Judgement." + std::string(ImageNames[index]));
        node.SetPivot({0.5F, 0.5F});
        node.SetPosition({judgementCircle.NodeSize().width * 0.5F, judgementCircle.NodeSize().height * 0.5F});
        // Follow the judgement-circle position/scale but keep the wording
        // upright inside Taiko's clockwise-rotated lane.
        node.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, HalfPi);
        node.SetZIndex(1);
        judgementImages_[index] = &node;
    }
}

void GameplayJudgementView::OnResize(const float logicalWidth)
{
    if (guide_ == nullptr)
        return;
    const float totalMicroseconds = static_cast<float>(displayHalfWindow_.count()) * 2.0F;
    pixelsPerMicrosecond_ =
        std::min(ReferencePixelsPerMicrosecond, std::max(logicalWidth - 40.0F, 1.0F) / totalMicroseconds);
    const float halfWidth = static_cast<float>(displayHalfWindow_.count()) * pixelsPerMicrosecond_;
    margin_->SetBounds({-halfWidth, 0.0F, halfWidth * 2.0F, BarHeight});
    for (std::size_t index = 0; index < bands_.size(); ++index)
    {
        const float width = static_cast<float>(halfWindows_[index].count()) * pixelsPerMicrosecond_ * 2.0F;
        bands_[index]->SetBounds({-width * 0.5F, 0.0F, width, BarHeight});
    }
    labels_[0]->SetBounds({-halfWidth, CaptionBottom, 80.0F, 16.0F});
    labels_[1]->SetBounds({-20.0F, CaptionBottom, 40.0F, 16.0F});
    labels_[2]->SetBounds({halfWidth - 80.0F, CaptionBottom, 80.0F, 16.0F});
    for (auto &line : hitLines_)
        PositionLine(line);
}

void GameplayJudgementView::SetGuidePlacement(const float gearBottom)
{
    if (guide_ != nullptr)
        guide_->SetPosition({0.0F, gearBottom - BarBelowGear});
}

void GameplayJudgementView::PositionLine(HitLine &line)
{
    const auto error = std::clamp(line.error, -displayHalfWindow_, displayHalfWindow_);
    line.node->SetPosition({static_cast<float>(error.count()) * pixelsPerMicrosecond_, BarHeight * 0.5F});
}

// Fade in (alpha and width, OutQuint) for 100ms, then fade out linearly over 5s while
// the width shrinks (InQuint). A displaced line instead fades to zero in 100ms.
void GameplayJudgementView::AnimateLine(HitLine &line, const double delta)
{
    line.age += delta;
    double alpha = 0.0, width = 0.0;
    if (line.age < LineFadeInSeconds)
    {
        const double eased = OutQuint(line.age / LineFadeInSeconds);
        alpha = LinePeakAlpha * eased;
        width = LineThickness * eased;
    }
    else
    {
        const double progress = std::min((line.age - LineFadeInSeconds) / LineFadeOutSeconds, 1.0);
        alpha = LinePeakAlpha * (1.0 - progress);
        width = LineThickness * (1.0 - std::pow(progress, 5.0));
        line.alive = progress < 1.0;
    }
    if (line.quickFadeAge >= 0.0)
    {
        line.quickFadeAge += delta;
        const double remaining = std::max(1.0 - line.quickFadeAge / LineQuickFadeSeconds, 0.0);
        alpha = std::min(alpha, static_cast<double>(line.quickFadeFrom) * remaining);
        line.alive = line.alive && remaining > 0.0;
    }
    auto color = line.color;
    color.alpha = static_cast<float>(alpha);
    Tint(*line.node, color);
    line.node->SetSize({static_cast<float>(width), LineHeight});
    line.node->SetVisible(line.alive);
}

void GameplayJudgementView::RetireOldestLiveLine()
{
    HitLine *oldest = nullptr;
    std::size_t live = 0;
    for (auto &line : hitLines_)
    {
        if (!line.alive || line.quickFadeAge >= 0.0)
            continue;
        ++live;
        if (oldest == nullptr || line.order < oldest->order)
            oldest = &line;
    }
    if (live < MaxLiveLines || oldest == nullptr)
        return;
    oldest->quickFadeFrom = oldest->node->GetComponent<SpriteVisualComponent>()->Style().normal.alpha;
    oldest->quickFadeAge = 0.0;
}

GameplayJudgementView::HitLine &GameplayJudgementView::FreeOrOldestLine()
{
    HitLine *oldest = &hitLines_.front();
    for (auto &line : hitLines_)
    {
        if (!line.alive)
            return line;
        if (line.order < oldest->order)
            oldest = &line;
    }
    return *oldest;
}

void GameplayJudgementView::AddHitLine(const RhythmDuration error, const Color color)
{
    RetireOldestLiveLine();
    auto &line = FreeOrOldestLine();
    line.color = color;
    line.error = error;
    line.order = nextLineOrder_++;
    line.age = 0.0;
    line.quickFadeAge = -1.0;
    line.alive = true;
    // Newer lines draw above older ones.
    line.node->SetZIndex(7 + static_cast<int>(std::min<std::uint64_t>(line.order, 1'000'000'000)));
    PositionLine(line);
    AnimateLine(line, 0.0);
}

void GameplayJudgementView::Present(const NoteEvent &event)
{
    if (guide_ == nullptr)
        return;
    const bool hasTiming = HasInputTiming(event);
    const bool poor = LaneKeyBeam::IsWrongInput(event);
    const bool miss = event.type == NoteEventType::Missed;
    if (!hasTiming && !miss && !poor)
        return;
    const std::size_t index = poor   ? PoorImageIndex
                              : miss ? static_cast<std::size_t>(JudgementGrade::Miss)
                                     : static_cast<std::size_t>(event.judgement.grade);
    if (index >= judgementImages_.size())
        return;
    ShowImage(index);
    if (hasTiming)
    {
        const auto color = poor ? LaneKeyBeam::WrongInputColor() : LaneKeyBeam::GradeColor(event.judgement.grade);
        AddHitLine(event.judgement.signedError, color);
    }
}

void GameplayJudgementView::ShowImage(std::size_t index)
{
    if (activeJudgement_ != nullptr)
        activeJudgement_->SetVisible(false);
    activeJudgement_ = judgementImages_[index];
    judgementElapsedSeconds_ = 0.0;
    Tint(*activeJudgement_, {1.0F, 1.0F, 1.0F, 1.0F});
    activeJudgement_->SetVisible(true);
}

void GameplayJudgementView::PresentCompletion(bool success)
{
    if (guide_ != nullptr)
        ShowImage(success ? OkImageIndex : static_cast<std::size_t>(JudgementGrade::Miss));
}

void GameplayJudgementView::Update(const double deltaSeconds)
{
    if (guide_ == nullptr || !std::isfinite(deltaSeconds))
        return;
    const double delta = std::max(deltaSeconds, 0.0);
    for (auto &line : hitLines_)
        if (line.alive)
            AnimateLine(line, delta);
    if (activeJudgement_ != nullptr)
    {
        judgementElapsedSeconds_ = std::min(FeedbackSeconds, judgementElapsedSeconds_ + delta);
        Tint(*activeJudgement_,
             {1.0F, 1.0F, 1.0F, static_cast<float>(1.0 - judgementElapsedSeconds_ / FeedbackSeconds)});
        activeJudgement_->SetVisible(judgementElapsedSeconds_ < FeedbackSeconds);
    }
}

void GameplayJudgementView::Reset() noexcept
{
    judgementElapsedSeconds_ = FeedbackSeconds;
    nextLineOrder_ = 0;
    for (auto &line : hitLines_)
    {
        line.error = {};
        line.alive = false;
        line.quickFadeAge = -1.0;
        if (line.node != nullptr)
            line.node->SetVisible(false);
    }
    for (auto *node : judgementImages_)
        if (node != nullptr)
            node->SetVisible(false);
    activeJudgement_ = nullptr;
}

void GameplayJudgementView::Shutdown() noexcept
{
    Reset();
    if (guide_ != nullptr)
        guide_->SetVisible(false);
    guide_ = nullptr;
    margin_ = nullptr;
    bands_.fill(nullptr);
    labels_.fill(nullptr);
    hitLines_ = {};
    judgementImages_.fill(nullptr);
}
