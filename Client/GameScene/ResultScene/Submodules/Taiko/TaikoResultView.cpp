#include "TaikoResultView.h"
#include "App/AssetPaths.h"
#include "Judgement/AccuracyScore.h"
#include "Presentation/BackgroundLayout.h"
#include "Presentation/SkinVisualComponent.h"
#include "Presentation/UiFonts.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

using namespace PhantomCore;

using namespace taiko_result_ui::layout;

namespace
{
    std::wstring_view RankImageKey(const AccuracyScore accuracy) noexcept
    {
        struct RankThreshold
        {
            std::uint32_t minimumPercent;
            std::wstring_view imageKey;
        };
        static constexpr std::array<RankThreshold, 10> ranks{{
            {100, L"Rank.Y"},
            {99, L"Rank.X"},
            {98, L"Rank.SS"},
            {96, L"Rank.S"},
            {93, L"Rank.AA"},
            {90, L"Rank.A"},
            {85, L"Rank.B"},
            {80, L"Rank.C"},
            {75, L"Rank.D"},
            {70, L"Rank.E"},
        }};
        // Compare the recorded six-decimal units, never the truncated display text.
        for (const auto &rank : ranks)
            if (accuracy.Units() >= rank.minimumPercent * 1'000'000U)
                return rank.imageKey;
        return L"Rank.F";
    }
    constexpr Color White{1, 1, 1, 1};
} // namespace

TaikoResultView::TaikoResultView(const TaikoPlayResult &result, ScreenVisual2DManager &visuals,
                                 TextCatalog &texts, ResultViewContext context)
    : result_(result), visuals_(visuals), texts_(texts), context_(std::move(context))
{
}
TaikoResultView::~TaikoResultView()
{
    Shutdown();
}
void TaikoResultView::LoadAssets()
{
    const auto skin = [this](const std::wstring_view key) { return visuals_.RegisterImage(Result(key)); };
    const auto icon = [this](const std::wstring_view key) { return visuals_.RegisterImage(Common(key)); };
    sky_ = skin(L"Background.Sky");
    leftScrim_ = skin(L"Panels.Left");
    rightScrim_ = skin(L"Panels.Right");
    banner_ = skin(L"Banner");
    bottomBar_ = skin(L"BottomBar");
    modeBadge_ = skin(L"ModeBadge");
    modeIcon_ = skin(L"Mode.Taiko");
    rankRing_ = skin(L"Rank.Ring");
    rankImage_ = skin(RankImageKey(result_.summary.accuracy));
    backIcon_ = icon(L"Icons.arrow-left");
    replayIcon_ = icon(L"Icons.film");
    retryIcon_ = icon(L"Icons.retry");
    creatorIcon_ = icon(L"Icons.pencil");
    chartBackground_ = {};
    chartBackgroundSize_ = {};
    if (!context_.backgroundPath.empty())
    {
        try
        {
            const auto image = visuals_.RegisterImage(context_.backgroundPath);
            const auto size = visuals_.GetImageSize(image);
            if (image && size.width > 0 && size.height > 0)
            {
                chartBackground_ = image;
                chartBackgroundSize_ = size;
            }
        }
        catch (const std::exception &)
        {
            // The result remains available with the skin sky.
        }
    }
}
void TaikoResultView::Initialize(const EngineServices &services)
{
    width_ = services.windowWidth;
    height_ = services.windowHeight;
    canvasHandle_ = visuals_.CreateOwnedCanvas({{ReferenceWidth, Height}, CanvasScaleMode::FixedHeight});
    canvas_ = canvasHandle_.Get();
    if (!canvas_)
        throw std::runtime_error("Failed to create result Canvas.");
    canvas_->SetViewportSize({static_cast<float>(width_), static_cast<float>(height_)});
    LoadAssets();
    Build();
}
Visual2DNode &TaikoResultView::Label(Visual2DNode &root, Rect designBounds, std::wstring text, TextStyle style)
{
    auto font = ui_fonts::Text(text);
    auto &label = CreateLabel(root, Bounds(designBounds), std::move(text), "Result.Label");
    auto &component = *label.GetComponent<TextVisualComponent>();
    component.SetFont(std::move(font));
    component.SetFontSize(style.size);
    component.SetFontWeight(style.weight);
    component.SetTextColor(style.color);
    component.SetHorizontalAlignment(style.alignment);
    label.SetClipRect({0, 0, designBounds.width, designBounds.height});
    return label;
}
Visual2DNode &TaikoResultView::NumberLabel(Visual2DNode &root, Rect designBounds, std::wstring text, TextStyle style,
                                           bool italic)
{
    auto &label = CreateLabel(root, Bounds(designBounds), std::move(text), "Result.Number");
    auto &component = *label.GetComponent<TextVisualComponent>();
    auto face = ui_fonts::Number(style.weight, italic);
    component.SetFont(std::move(face.font));
    component.SetFontSize(style.size);
    component.SetFontWeight(face.weight);
    component.SetTextColor(style.color);
    component.SetHorizontalAlignment(style.alignment);
    // Italic digits lean past their advance box, so numbers are not clipped.
    return label;
}
Visual2DNode &TaikoResultView::Fill(Visual2DNode &root, Rect designBounds, Color color, std::string name)
{
    auto &panel = CreatePanel(root, Bounds(designBounds), std::move(name));
    auto &visual = *panel.GetComponent<SpriteVisualComponent>();
    visual.SetTint(color);
    visual.SetCornerRadius(0);
    return panel;
}
Visual2DNode &TaikoResultView::Image(Visual2DNode &root, Rect designBounds, ImageHandle image, std::string name,
                                     Color tint)
{
    auto &sprite = CreateSprite(root, Bounds(designBounds), image, std::move(name));
    sprite.GetComponent<SpriteVisualComponent>()->SetTint(tint);
    return sprite;
}
Visual2DNode &TaikoResultView::Button(Visual2DNode &root, Rect designBounds, std::string name, std::wstring title,
                                      float fontSize, Color textColor, ImageHandle icon, float contentLeft)
{
    auto &button = CreateButton(root, Bounds(designBounds), L"", std::move(name));
    auto &sprite = *button.GetComponent<SpriteVisualComponent>();
    VisualStyle style;
    // The skin supplies the shape; a disabled button keeps it at 40%.
    style.normal = style.hovered = style.pressed = White;
    style.disabled = {1, 1, 1, .4F};
    sprite.SetStyle(style);
    sprite.SetCornerRadius(0);
    const auto key = button.Name().substr(std::string_view("Result.").size()) + "Button";
    BindSkinImage(button, visuals_, "Result", key);
    constexpr float iconSize = 18, gap = 8;
    auto &glyph = CreateSprite(button, {contentLeft, (designBounds.height - iconSize) * 0.5F, iconSize, iconSize}, icon,
                               "Result.ButtonIcon");
    glyph.GetComponent<SpriteVisualComponent>()->SetTint(textColor);
    const float textLeft = contentLeft + iconSize + gap;
    auto font = ui_fonts::Text(title);
    auto &caption = CreateLabel(button, {textLeft, 0, std::max(designBounds.width - textLeft, 1.0F), designBounds.height},
                                std::move(title), "Result.ButtonText");
    auto &text = *caption.GetComponent<TextVisualComponent>();
    text.SetFont(std::move(font));
    text.SetFontSize(fontSize);
    text.SetFontWeight(800);
    text.SetHorizontalAlignment(TextAlignment::Leading);
    text.SetTextColor(textColor);
    return button;
}
void TaikoResultView::BuildBackground()
{
    const auto size = canvas_->LogicalSize();
    auto &anchor = canvas_->AnchorNode(Anchor::Center);
    const float referenceScale = size.height / Height;
    const auto backgroundBounds = ChartBackgroundBounds(size, chartBackgroundSize_,
        {static_cast<float>(context_.backgroundOffsetX) * referenceScale,
         static_cast<float>(context_.backgroundOffsetY) * referenceScale});
    auto &sky =
        CreateSprite(anchor, backgroundBounds, chartBackground_ ? chartBackground_ : sky_, "Result.Sky");
    sky.SetZIndex(-10);
    backgroundId_ = sky.Id();

    // The 16:9 layout is never squeezed. A narrower screen shows it scaled
    // between top and bottom bands; a screen wider than 32:9 gets side bands.
    auto &frame = canvas_->CreateNode(Anchor::Center, "Result.Letterbox");
    frame.SetZIndex(-5);
    frameId_ = frame.Id();
    const float contentWidth = columns_.width * contentScale_, contentHeight = Height * contentScale_;
    const auto band = [&frame](Rect bounds)
    {
        if (bounds.width <= 0.01F || bounds.height <= 0.01F)
            return;
        auto &panel = CreatePanel(frame, bounds, "Result.LetterboxBand");
        auto &visual = *panel.GetComponent<SpriteVisualComponent>();
        visual.SetTint(ui_colors::theme::Letterbox);
        visual.SetCornerRadius(0);
    };
    const float vertical = (size.height - contentHeight) * 0.5F, horizontal = (size.width - contentWidth) * 0.5F;
    band({-size.width * 0.5F, contentHeight * 0.5F, size.width, vertical});
    band({-size.width * 0.5F, -size.height * 0.5F, size.width, vertical});
    band({-size.width * 0.5F, -contentHeight * 0.5F, horizontal, contentHeight});
    band({contentWidth * 0.5F, -contentHeight * 0.5F, horizontal, contentHeight});
}
void TaikoResultView::Build()
{
    router_.Reset(*canvas_);
    for (const auto id : {contentId_, backgroundId_, frameId_})
        if (id)
            static_cast<void>(canvas_->RemoveNode(id));
    tooltipId_ = 0;
    hoveredSlice_.reset();
    const auto logical = canvas_->LogicalSize();
    const float contentWidth = std::clamp(logical.width, ReferenceWidth, MaximumWidth);
    contentScale_ = std::min(logical.width / contentWidth, 1.0F);
    columns_ = ColumnsFor(contentWidth);
    contentOrigin_ = {-contentWidth * contentScale_ * 0.5F, -Height * contentScale_ * 0.5F};
    BuildBackground();
    auto &root = canvas_->CreateNode(Anchor::Center, "Result.Content");
    root.SetBounds({0, 0, contentWidth, Height});
    root.SetPivot({.5F, .5F});
    root.Transform().SetScale(contentScale_, contentScale_, 1);
    contentId_ = root.Id();
    // White scrims keep text and graphs readable over the chart BG.
    Image(root, {0, 0, 460, Height}, leftScrim_, "Result.LeftScrim");
    const float rightScrimWidth = contentWidth * 0.4F;
    Image(root, {contentWidth - rightScrimWidth, 0, rightScrimWidth, Height}, rightScrim_, "Result.RightScrim");
    BuildSongBanner(root);
    BuildSummary(root);
    BuildHero(root);
    BuildGraphs(root);
    BuildActions(root);
    textRevision_ = texts_.Revision();
    router_.InvalidateHitTest();
}
void TaikoResultView::BuildActions(Visual2DNode &root)
{
    const auto &text = Result(texts_);
    const float width = columns_.width;
    Image(root, {0, Height - BottomBarHeight, width, BottomBarHeight}, bottomBar_, "Result.BottomBar");
    // Back sits in the bottom-left corner; Retry is the single primary action and
    // rises above the bar at the bottom-right; Replay is a text button beside it.
    auto &back = Button(root, {0, Height - BottomBarHeight, 136, BottomBarHeight}, "Result.Back",
                        std::wstring(text.back), 17, White, backIcon_, 30);
    backId_ = back.Id();
    constexpr float retryWidth = 240, retryHeight = 84, replayWidth = 125;
    auto &replay = Button(root, {width - retryWidth - replayWidth, Height - BottomBarHeight, replayWidth, BottomBarHeight},
                          "Result.Replay", std::wstring(text.replay), 16,
                          {BrandText.red, BrandText.green, BrandText.blue, .4F}, replayIcon_, 23);
    // Replay playback is not implemented; the button stays visible but disabled.
    replay.SetEnabled(false);
    for (const auto &child : replay.Children())
        child->SetEnabled(false);
    auto &retry = Button(root, {width - retryWidth, Height - retryHeight, retryWidth, retryHeight}, "Result.Retry",
                         std::wstring(text.retry), 22, White, retryIcon_, 82);
    retry.SetEnabled(context_.canRetry);
    for (const auto &child : retry.Children())
        child->SetEnabled(context_.canRetry);
    retryId_ = retry.Id();
}
void TaikoResultView::SetVisible(bool visible) noexcept
{
    static_cast<void>(canvasHandle_.SetVisible(visible));
}
void TaikoResultView::ProcessPointer(const InputState &input)
{
    const auto position = input.IsMouseInsideWindow()
                              ? MapScreenPointer({static_cast<float>(input.MousePositionX()),
                                                     static_cast<float>(input.MousePositionY())},
                                                    {static_cast<float>(width_), static_cast<float>(height_)}, *canvas_)
                              : std::nullopt;
    PointerInput pointer;
    pointer.available = position.has_value();
    pointer.position = position.value_or(Point{});
    pointer.leftButtonDown = input.IsMouseButtonDown(MouseButton::Left);
    pointer.leftButtonPressed = input.WasMouseButtonPressed(MouseButton::Left);
    pointer.leftButtonReleased = input.WasMouseButtonReleased(MouseButton::Left);
    router_.Process(*canvas_, pointer);
    UpdateRingTooltip(position);
}
ResultAction TaikoResultView::Update(const InputState &input)
{
    if (!canvas_)
        return ResultAction::None;
    if (textRevision_ != texts_.Revision())
        Build();
    ProcessPointer(input);
    for (const auto &action : canvas_->TakeActions())
    {
        if (action.type != ActionType::Clicked)
            continue;
        if (action.source == backId_)
            return ResultAction::Back;
        if (action.source == retryId_ && context_.canRetry)
            return ResultAction::Retry;
    }
    return input.WasKeyPressed(VK_ESCAPE) || input.WasKeyPressed(VK_RETURN) ? ResultAction::Back : ResultAction::None;
}
void TaikoResultView::OnResize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0)
        return;
    width_ = width;
    height_ = height;
    if (canvas_)
    {
        canvas_->SetViewportSize({static_cast<float>(width), static_cast<float>(height)});
        Build();
    }
}
void TaikoResultView::Shutdown() noexcept
{
    if (canvas_)
        router_.Reset(*canvas_);
    contentId_ = backId_ = retryId_ = backgroundId_ = frameId_ = tooltipId_ = 0;
    canvas_ = nullptr;
    canvasHandle_.Reset();
}
