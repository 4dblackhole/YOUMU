#include "GameplayJudgementView.h"
#include "App/AssetPaths.h"
#include "GameplaySupport.h"
#include "Presentation/LaneKeyBeam.h"
#include "Presentation/UiColors.h"
#include "Presentation/UiFonts.h"
#include "Texts/GameScene/RhythmTestScene/GameplayTexts.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>

using namespace PhantomCore;

namespace
{

    // The default JudgeLevel 50 guide (BAD +/-90ms plus a 10ms margin each side) is 420 wide.
    constexpr float ReferencePixelsPerMicrosecond = 420.0F / 200'000.0F;
    constexpr float BarHeight = 12.0F;
    // The guide origin is the bar's bottom, 78 below the gear: the bar centre sits 72
    // below it, 24 lower than before the hit lines doubled to 48.
    constexpr float BarBelowGear = 78.0F;
    // Hit lines follow osu!'s bar hit error meter defaults (4 thick, 0.6 peak alpha,
    // 100ms OutQuint fade-in, 5s fade-out, at most 50 live lines; older ones fade in
    // 100ms) at twice the default height, 18 beyond the bar on each side.
    constexpr float LineThickness = 4.0F, LineHeight = 48.0F, LinePeakAlpha = 0.6F;
    constexpr double LineFadeInSeconds = 0.1, LineFadeOutSeconds = 5.0, LineQuickFadeSeconds = 0.1;
    constexpr std::size_t MaxLiveLines = 50;
    constexpr float ZeroLineHeight = LineHeight + 4.0F;
    // Hud.TimingPanel is drawn as authored (translucency is in the PNG). Its design is
    // the JudgeLevel 50 guide: 524x112 with the bar 52 in from each side and 44..56 from
    // the top. Wings (EARLY/LATE) and the centre tab (mean error) keep their size; the
    // straight columns between them stretch. The inner width never drops below 300.
    constexpr float PanelWidth = 524.0F, PanelHeight = 112.0F, PanelPad = 52.0F, PanelMinInner = 300.0F;
    constexpr float PanelTop = 56.0F; // Above the bar's bottom.
    constexpr float WingWidth = 128.0F, CentreHalf = 70.0F;
    constexpr float CaptionBottom = PanelTop - 19.0F, CaptionHeight = 16.0F, CaptionWidth = 80.0F;
    // Mean error in the 128x30 tab under the panel: the number ends right of centre and
    // the smaller unit follows it, both on roughly the same baseline.
    constexpr float TabTop = PanelTop - 82.0F, TabHeight = 30.0F;
    constexpr float AverageNumberLeft = -64.0F, AverageNumberRight = 13.0F, AverageUnitLeft = 15.0F;
    constexpr float AverageUnitDrop = 3.0F;
    // Skin keys after "Judgements.": the grades, then the BAD-recovery words (Judgements.Recovery.MAX..GOOD).
    constexpr std::array<std::string_view, 11> ImageNames{"MAX",          "PERFECT",          "GREAT",
                                                          "GOOD",         "BAD",              "MISS",
                                                          "POOR",         "Recovery.MAX",     "Recovery.PERFECT",
                                                          "Recovery.GREAT", "Recovery.GOOD"};
    constexpr std::size_t RecoveryImageIndex = 7;
    // Judgement words sit just above the judgement circle (screen up), clear of the Highlight ring.
    constexpr float WordGapAboveCircle = 6.0F;
    constexpr std::size_t PoorImageIndex = 6;
    constexpr Color Ink = ui_colors::theme::Ink;

    void Tint(Visual2DNode &node, const Color color)
    {
        node.GetComponent<SpriteVisualComponent>()->SetTint(color);
    }

    Visual2DNode &NumberLabel(Visual2DNode &parent, std::string name, std::uint16_t weight, float size, Color color,
                              TextAlignment alignment)
    {
        auto &node = CreateLabel(parent, {}, L"", std::move(name));
        auto &label = *node.GetComponent<TextVisualComponent>();
        auto face = ui_fonts::Number(weight, true);
        label.SetFont(std::move(face.font));
        label.SetFontWeight(face.weight);
        label.SetFontSize(size);
        label.SetTextColor(color);
        label.SetHorizontalAlignment(alignment);
        return node;
    }

    // A line counts toward the mean while it is drawn and not being pushed out early.
    bool CountsTowardAverage(bool alive, double quickFadeAge) noexcept
    {
        return alive && quickFadeAge < 0.0;
    }

    // Only real timing judgements get a hit line. Roll/count-only ticks have no
    // grade; an automatic miss does not represent a player's input timestamp.
    bool HasInputTiming(const NoteEvent &event) noexcept
    {
        const bool acceptedInput = event.type == NoteEventType::HitAccepted ||
                                   event.type == NoteEventType::HoldStarted ||
                                   event.type == NoteEventType::TickAccepted;
        const bool hasInput = acceptedInput || event.type == NoteEventType::InputRejected;
        return hasInput && IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Bad);
    }

    // A BAD recovery of GOOD or better uses its purple-tinted word; every other grade its own word.
    std::size_t WordIndex(const NoteEvent &event) noexcept
    {
        const auto grade = static_cast<std::size_t>(event.judgement.grade);
        const bool recovered = event.recovery && IsAtLeastAsAccurateAs(event.judgement.grade, JudgementGrade::Good);
        return recovered ? RecoveryImageIndex + grade : grade;
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
    CreateTimingPanel();
    CreateTimingBands();
    CreateTimingCaptions(texts);
    CreateAverageError(texts);
    CreateHitLines();
}

void GameplayJudgementView::CreateTimingPanel()
{
    const auto image = visuals_->RegisterImage(InGame(L"Hud.TimingPanel"));
    // Each piece samples its own columns of the one image; OnResize places them.
    constexpr std::array<float, 6> columns{0.0F, WingWidth, PanelWidth * 0.5F - CentreHalf,
                                           PanelWidth * 0.5F + CentreHalf, PanelWidth - WingWidth, PanelWidth};
    for (std::size_t index = 0; index < panelPieces_.size(); ++index)
    {
        auto &piece = CreateSprite(*guide_, {}, image, "Judgement.Panel." + std::to_string(index));
        piece.SetZIndex(-1);
        const float from = columns[index] / PanelWidth, to = columns[index + 1] / PanelWidth;
        piece.GetComponent<SpriteVisualComponent>()->SetUvTransform({to - from, 1.0F}, {from, 0.0F});
        panelPieces_[index] = &piece;
    }
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
    // EARLY and LATE sit in the panel's wings, aligned with its inner edges.
    const auto &text = Gameplay(texts);
    const std::array<std::wstring_view, 2> captions{text.early, text.late};
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
        label.SetHorizontalAlignment(index == 0 ? TextAlignment::Leading : TextAlignment::Trailing);
    }
}

void GameplayJudgementView::CreateAverageError(TextCatalog &texts)
{
    const auto &text = Gameplay(texts);
    averageEmpty_ = std::wstring(text.averageErrorEmpty);
    averageValue_ = &NumberLabel(*guide_, "Judgement.Average", 800, 22.0F, Ink, TextAlignment::Trailing);
    const float numberWidth = AverageNumberRight - AverageNumberLeft;
    averageValue_->SetBounds({AverageNumberLeft, TabTop - TabHeight, numberWidth, TabHeight});
    averageUnit_ = &NumberLabel(*guide_, "Judgement.AverageUnit", 700, 13.0F, ui_colors::theme::InkSoft,
                                TextAlignment::Leading);
    averageUnit_->GetComponent<TextVisualComponent>()->SetText(std::wstring(text.averageErrorUnit));
    averageUnit_->SetBounds({AverageUnitLeft, TabTop - TabHeight - AverageUnitDrop, 40.0F, TabHeight});
    averageValue_->GetComponent<TextVisualComponent>()->SetText(averageEmpty_);
    shownAverage_ = EmptyAverage;
}

void GameplayJudgementView::CreateHitLines()
{
    const float zeroBottom = (BarHeight - ZeroLineHeight) * 0.5F;
    auto &zero = CreatePanel(*guide_, {-0.5F, zeroBottom, 1.0F, ZeroLineHeight}, "Judgement.Zero");
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
    const auto circle = judgementCircle.NodeSize();
    for (std::size_t index = 0; index < judgementImages_.size(); ++index)
    {
        // Word-only skin art at its 1080p size, like the other in-game parts.
        const std::string file = "Judgements." + std::string(ImageNames[index]);
        const auto image = visuals.RegisterImage(InGame(file));
        const auto size = ScaledImageSize(visuals, image);
        auto &node = CreateSprite(judgementCircle, {0.0F, 0.0F, size.width, size.height}, image,
                                  "Lane.Judgement." + std::string(ImageNames[index]));
        node.SetPivot({0.5F, 0.5F});
        // Taiko's lane is turned clockwise, so the circle's local -X is screen up: the word's
        // centre sits half its height above the circle's top edge plus the gap.
        const float above = circle.width * 0.5F + WordGapAboveCircle + size.height * 0.5F;
        node.SetPosition({circle.width * 0.5F - above, circle.height * 0.5F});
        // Follow the judgement-circle position/scale but keep the wording upright.
        node.Transform().SetRotationRollPitchYaw(0.0F, 0.0F, HalfPi);
        node.SetZIndex(1);
        judgementImages_[index] = &node;
    }
}

void GameplayJudgementView::OnResize(const float logicalWidth)
{
    if (guide_ == nullptr)
        return;
    // Only the time scale shrinks, so that the whole panel keeps 20 from each screen edge.
    const float totalMicroseconds = static_cast<float>(displayHalfWindow_.count()) * 2.0F;
    const float available = std::max(logicalWidth - 40.0F - PanelPad * 2.0F, 1.0F);
    pixelsPerMicrosecond_ = std::min(ReferencePixelsPerMicrosecond, available / totalMicroseconds);
    const float halfWidth = static_cast<float>(displayHalfWindow_.count()) * pixelsPerMicrosecond_;
    margin_->SetBounds({-halfWidth, 0.0F, halfWidth * 2.0F, BarHeight});
    for (std::size_t index = 0; index < bands_.size(); ++index)
    {
        const float width = static_cast<float>(halfWindows_[index].count()) * pixelsPerMicrosecond_ * 2.0F;
        bands_[index]->SetBounds({-width * 0.5F, 0.0F, width, BarHeight});
    }
    LayoutTimingPanel(halfWidth);
    for (auto &line : hitLines_)
        PositionLine(line);
}

void GameplayJudgementView::LayoutTimingPanel(const float halfWidth)
{
    const float outer = std::max(halfWidth, PanelMinInner * 0.5F) + PanelPad;
    // Piece edges, left to right; neighbours share the same computed value.
    const std::array<float, 6> edges{-outer, -outer + WingWidth, -CentreHalf, CentreHalf, outer - WingWidth, outer};
    const float bottom = PanelTop - PanelHeight;
    for (std::size_t index = 0; index < panelPieces_.size(); ++index)
        panelPieces_[index]->SetBounds({edges[index], bottom, edges[index + 1] - edges[index], PanelHeight});
    const float inner = outer - PanelPad;
    labels_[0]->SetBounds({-inner, CaptionBottom, CaptionWidth, CaptionHeight});
    labels_[1]->SetBounds({inner - CaptionWidth, CaptionBottom, CaptionWidth, CaptionHeight});
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
    RefreshAverageError();
}

void GameplayJudgementView::Present(const NoteEvent &event)
{
    if (guide_ == nullptr)
        return;
    const bool hasTiming = HasInputTiming(event);
    const bool poor = LaneKeyBeam::IsWrongInput(event);
    // A note missed after an early BAD or a POOR already showed that word; it shows no further word.
    const bool shownBefore = event.leftAtEarlyBad || event.leftAtPoor;
    const bool miss = event.type == NoteEventType::Missed && !shownBefore;
    if (!hasTiming && !miss && !poor)
        return;
    const std::size_t index = poor   ? PoorImageIndex
                              : miss ? static_cast<std::size_t>(JudgementGrade::Miss)
                                     : WordIndex(event);
    if (index >= judgementImages_.size())
        return;
    ShowImage(index);
    if (hasTiming)
    {
        const auto color = poor ? LaneKeyBeam::WrongInputColor() : LaneKeyBeam::GradeColor(event.judgement.grade);
        AddHitLine(event.judgement.signedError, color);
    }
}

void GameplayJudgementView::RefreshAverageError()
{
    if (averageValue_ == nullptr)
        return;
    std::int64_t sum = 0;
    std::int64_t count = 0;
    for (const auto &line : hitLines_)
    {
        if (!CountsTowardAverage(line.alive, line.quickFadeAge))
            continue;
        sum += line.error.count();
        ++count;
    }
    // Rounded to tenths of a millisecond; positive is late.
    std::optional<std::int64_t> tenths;
    if (count > 0)
        tenths = std::llround(static_cast<double>(sum) / static_cast<double>(count) / 100.0);
    const std::int64_t shown = tenths.value_or(EmptyAverage);
    if (shown == shownAverage_)
        return;
    shownAverage_ = shown;
    std::wstring text = averageEmpty_;
    if (tenths)
    {
        // A typographic minus for early, an explicit plus for late, no sign at zero.
        const wchar_t *sign = *tenths > 0 ? L"+" : (*tenths < 0 ? L"−" : L"");
        const std::int64_t magnitude = *tenths < 0 ? -*tenths : *tenths;
        text = std::format(L"{}{}.{}", sign, magnitude / 10, magnitude % 10);
    }
    averageValue_->GetComponent<TextVisualComponent>()->SetText(std::move(text));
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

void GameplayJudgementView::PresentCompletion(const JudgementGrade grade)
{
    const auto index = static_cast<std::size_t>(grade);
    if (guide_ != nullptr && index < PoorImageIndex)
        ShowImage(index);
}

void GameplayJudgementView::Update(const double deltaSeconds)
{
    if (guide_ == nullptr || !std::isfinite(deltaSeconds))
        return;
    const double delta = std::max(deltaSeconds, 0.0);
    for (auto &line : hitLines_)
        if (line.alive)
            AnimateLine(line, delta);
    RefreshAverageError();
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
    RefreshAverageError();
}

void GameplayJudgementView::Shutdown() noexcept
{
    Reset();
    if (guide_ != nullptr)
        guide_->SetVisible(false);
    guide_ = nullptr;
    panelPieces_.fill(nullptr);
    margin_ = nullptr;
    bands_.fill(nullptr);
    labels_.fill(nullptr);
    averageValue_ = nullptr;
    averageUnit_ = nullptr;
    shownAverage_ = EmptyAverage;
    hitLines_ = {};
    judgementImages_.fill(nullptr);
}
