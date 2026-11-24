#include "TitlePlayerView.h"
#include "App/AssetPaths.h"
#include "GameScene/MusicSelectScene/Submodules/MusicPlayerState.h"
#include "LogoStyle.h"
#include <algorithm>
using namespace PhantomCore;

using namespace logo_ui;

namespace
{
    // UI-02 title player, logical pixels with a top-left origin on the 1280-wide reference board.
    constexpr Rect ArtworkBounds{320.0F, 112.0F, 160.0F, 160.0F};
    // The frame adds a 10px shadow right and below the picture.
    constexpr Rect ArtworkFrameBounds{320.0F, 112.0F, 170.0F, 170.0F};
    constexpr Rect MusicIconBounds{508.0F, 126.0F, 16.0F, 16.0F};
    constexpr Rect ArtistBounds{530.0F, 124.0F, 430.0F, 20.0F};
    constexpr Rect TitleBounds{508.0F, 146.0F, 452.0F, 38.0F};
    constexpr Rect ElapsedBounds{508.0F, 198.0F, 40.0F, 20.0F};
    constexpr Rect LengthBounds{920.0F, 198.0F, 40.0F, 20.0F};
    constexpr Rect SeekBounds{558.0F, 199.0F, 352.0F, 18.0F};
    // The elapsed fill is a 4px line through the bar; the knob is a 4x14 bar on its right end.
    constexpr float SeekFillTop = 7.0F, SeekFillHeight = 4.0F;
    constexpr float SeekKnobTop = 2.0F;
    constexpr Size SeekKnobSize{4.0F, 14.0F};
    // User decision: play/pause, stop and next only, laid out with the mockup's spacing.
    constexpr Rect PlayBounds{508.0F, 228.0F, 68.0F, 44.0F};
    constexpr Rect StopBounds{584.0F, 228.0F, 44.0F, 44.0F};
    constexpr Rect NextBounds{636.0F, 228.0F, 44.0F, 44.0F};
    constexpr float PlayIconSize = 24.0F, ButtonIconSize = 22.0F, StopGlyphSize = 12.0F;
    constexpr Color EmptyArtwork{.918F, .949F, 1.0F, 1.0F};
    constexpr Color Clear{1.0F, 1.0F, 1.0F, 0.0F};
    // Skinned parts draw their image at the fallback's opacity, so fallbacks stay opaque.
    constexpr Color SeekTrackFallback{.247F, .435F, .659F, 1.0F};
    constexpr float DisabledIconAlpha = 0.4F;

    [[nodiscard]] ImageHandle IconImage(ScreenVisual2DManager &visuals, const wchar_t *name)
    {
        const auto path = Common(name);
        return path.empty() ? ImageHandle{} : visuals.RegisterImage(path);
    }

    // A skinned part keeps its fallback colour only while the skin has no image for it.
    void BindPart(Visual2DNode &node, ScreenVisual2DManager &visuals, const char *key, const Color fallback)
    {
        VisualStyle style{};
        style.normal = style.hovered = style.pressed = fallback;
        style.disabled = {fallback.red, fallback.green, fallback.blue, fallback.alpha * 0.45F};
        SetSkinBaseStyle(node, style);
        RequireComponent<SpriteVisualComponent>(node).SetCornerRadius(0.0F);
        BindSkinImage(node, visuals, "Logo", key);
    }

    void StyleLabel(Visual2DNode &node, const TextFont &font, const float size, const std::uint16_t weight,
                    const Color color, const TextAlignment alignment = TextAlignment::Leading)
    {
        auto &text = RequireComponent<TextVisualComponent>(node);
        text.SetFont(font);
        text.SetFontSize(size);
        text.SetFontWeight(weight);
        text.SetTextColor(color);
        text.SetHorizontalAlignment(alignment);
    }

    void SetCaption(Visual2DNode &node, const std::wstring &value)
    {
        auto &text = RequireComponent<TextVisualComponent>(node);
        text.SetFont(ui_fonts::Text(value));
        text.SetText(value);
    }

    [[nodiscard]] bool SameSong(const TitlePlayerDisplay &left, const TitlePlayerDisplay &right)
    {
        if (left.hasSong != right.hasSong)
            return false;
        return left.title == right.title && left.artist == right.artist;
    }

    void SetIconAlpha(Visual2DNode &icon, const Color color, const bool enabled)
    {
        RequireComponent<SpriteVisualComponent>(icon).SetTint(
            {color.red, color.green, color.blue, enabled ? color.alpha : color.alpha * DisabledIconAlpha});
    }
} // namespace

void TitlePlayerView::Create(Visual2DNode &board)
{
    CreateArtwork(board);
    CreateInformation(board);
    CreateControls(board);
    ApplySong();
}

void TitlePlayerView::CreateArtwork(Visual2DNode &board)
{
    artworkBox_ = &CreatePanel(board, {}, "YOUMU.PlayerArtworkEmpty");
    auto &box = RequireComponent<SpriteVisualComponent>(*artworkBox_);
    box.SetTint(EmptyArtwork);
    box.SetCornerRadius(0.0F);
    artworkImage_ = &CreateSprite(board, {}, {}, "YOUMU.PlayerArtwork");
    RequireComponent<SpriteVisualComponent>(*artworkImage_).SetTint(ui_colors::White);
    artworkImage_->SetVisible(false);
    artworkFrame_ = &CreateSprite(board, {}, {}, "YOUMU.PlayerArtworkFrame");
    BindPart(*artworkFrame_, visuals_, "Player.ArtFrame", ui_colors::White);
    for (auto *node : {artworkBox_, artworkImage_, artworkFrame_})
        node->SetZIndex(10);
}

void TitlePlayerView::CreateInformation(Visual2DNode &board)
{
    musicIcon_ = &CreateSprite(board, {}, IconImage(visuals_, L"Icons.music"), "YOUMU.PlayerMusicIcon");
    artistLabel_ = &CreateLabel(board, {}, L"", "YOUMU.PlayerArtist");
    StyleLabel(*artistLabel_, ui_fonts::Text(), 15.0F, 700, ui_colors::theme::Caption);
    titleLabel_ = &CreateLabel(board, {}, L"", "YOUMU.PlayerTitle");
    StyleLabel(*titleLabel_, ui_fonts::Text(), 30.0F, 900, ui_colors::theme::Ink);
    const auto number = ui_fonts::Number(700, false);
    elapsedLabel_ = &CreateLabel(board, {}, L"", "YOUMU.PlayerElapsed");
    StyleLabel(*elapsedLabel_, number.font, 15.0F, number.weight, ui_colors::theme::InkSoft);
    lengthLabel_ = &CreateLabel(board, {}, L"", "YOUMU.PlayerLength");
    StyleLabel(*lengthLabel_, number.font, 15.0F, number.weight, ui_colors::theme::InkSoft, TextAlignment::Trailing);

    // The bar is the click and drag target; fill and knob only show the position.
    seekTrack_ = &CreateSprite(board, {}, {}, "YOUMU.PlayerSeek");
    BindPart(*seekTrack_, visuals_, "Player.SeekTrack", SeekTrackFallback);
    seekTrack_->AddComponent<RectangleCollider2DComponent>();
    seekTrack_->AddComponent<PointerReceiverComponent>();
    seekFill_ = &CreateSprite(board, {}, {}, "YOUMU.PlayerSeekFill");
    BindPart(*seekFill_, visuals_, "Player.SeekFill", ui_colors::theme::Brand);
    seekKnob_ = &CreateSprite(board, {}, {}, "YOUMU.PlayerSeekKnob");
    BindPart(*seekKnob_, visuals_, "Player.SeekKnob", ui_colors::theme::Ink);
    for (auto *node : {musicIcon_, artistLabel_, titleLabel_, elapsedLabel_, lengthLabel_, seekTrack_})
        node->SetZIndex(10);
    seekFill_->SetZIndex(11);
    seekKnob_->SetZIndex(12);
}

void TitlePlayerView::CreateControls(Visual2DNode &board)
{
    playImage_ = IconImage(visuals_, L"Icons.play");
    pauseImage_ = IconImage(visuals_, L"Icons.pause");

    playButton_ = &CreateButton(board, {}, L"", "YOUMU.PlayerPlay");
    BindPart(*playButton_, visuals_, "Player.PlayButton", ui_colors::theme::Brand);
    playIcon_ = &CreateSprite(*playButton_, {(PlayBounds.width - PlayIconSize) * 0.5F, (PlayBounds.height - PlayIconSize) * 0.5F,
                                             PlayIconSize, PlayIconSize},
                              playImage_, "Icon");

    // Stop and next are bare icons that light up on hover, as in the mockup.
    const auto iconButton = [this, &board](const char *name) -> Visual2DNode & {
        auto &button = CreateButton(board, {}, L"", name);
        VisualStyle style{};
        style.normal = style.disabled = Clear;
        style.hovered = {1.0F, 1.0F, 1.0F, 0.6F};
        style.pressed = {1.0F, 1.0F, 1.0F, 0.85F};
        SetSkinBaseStyle(button, style);
        RequireComponent<SpriteVisualComponent>(button).SetCornerRadius(2.0F);
        return button;
    };
    stopButton_ = &iconButton("YOUMU.PlayerStop");
    // The skin has no stop icon; a filled square reads as stop next to the line icons.
    stopIcon_ = &CreatePanel(*stopButton_, {(StopBounds.width - StopGlyphSize) * 0.5F, (StopBounds.height - StopGlyphSize) * 0.5F,
                                            StopGlyphSize, StopGlyphSize},
                             "Icon");
    RequireComponent<SpriteVisualComponent>(*stopIcon_).SetCornerRadius(1.0F);
    nextButton_ = &iconButton("YOUMU.PlayerNext");
    nextIcon_ = &CreateSprite(*nextButton_, {(NextBounds.width - ButtonIconSize) * 0.5F, (NextBounds.height - ButtonIconSize) * 0.5F,
                                             ButtonIconSize, ButtonIconSize},
                              IconImage(visuals_, L"Icons.next"), "Icon");
    for (auto *node : {playButton_, stopButton_, nextButton_})
        node->SetZIndex(10);
}

void TitlePlayerView::Layout(const Size board)
{
    if (artworkBox_ == nullptr)
        return;
    boardHeight_ = board.height;
    originX_ = (board.width - ReferenceSize.width) * 0.5F;
    const auto place = [this](Visual2DNode &node, const Rect bounds) {
        node.SetBounds(TopLeft({bounds.x + originX_, bounds.y, bounds.width, bounds.height}, boardHeight_));
    };
    place(*artworkBox_, ArtworkBounds);
    place(*artworkImage_, ArtworkBounds);
    place(*artworkFrame_, ArtworkFrameBounds);
    place(*musicIcon_, MusicIconBounds);
    place(*artistLabel_, ArtistBounds);
    place(*titleLabel_, TitleBounds);
    place(*elapsedLabel_, ElapsedBounds);
    place(*lengthLabel_, LengthBounds);
    place(*seekTrack_, SeekBounds);
    place(*playButton_, PlayBounds);
    place(*stopButton_, StopBounds);
    place(*nextButton_, NextBounds);
    LayoutArtworkImage();
    ApplyProgress();
}

void TitlePlayerView::SetDisplay(const TitlePlayerDisplay &display)
{
    const bool songChanged = !SameSong(display, display_);
    const bool artworkChanged = display.artwork != display_.artwork;
    const bool playingChanged = display.playing != display_.playing;
    display_ = display;
    if (!display_.hasSong)
        seekDrag_.reset();
    if (songChanged)
        ApplySong();
    // Song select releases backgrounds it no longer keeps; a released album art registers again.
    if (artworkChanged || songChanged || ArtworkReleased())
        ApplyArtwork(display_.hasSong ? display_.artwork : std::filesystem::path{});
    if ((playingChanged || songChanged) && playIcon_ != nullptr)
        RequireComponent<SpriteVisualComponent>(*playIcon_).SetImage(display_.playing ? pauseImage_ : playImage_);
    ApplyProgress();
}

void TitlePlayerView::ApplySong()
{
    if (titleLabel_ == nullptr)
        return;
    // Without a song the art is an empty box, the text is blank and the controls are off.
    const bool enabled = display_.hasSong;
    SetCaption(*titleLabel_, enabled ? display_.title : std::wstring{});
    SetCaption(*artistLabel_, enabled ? display_.artist : std::wstring{});
    musicIcon_->SetVisible(enabled);
    RequireComponent<SpriteVisualComponent>(*musicIcon_).SetTint(ui_colors::sky::Accent);
    for (auto *node : {playButton_, stopButton_, nextButton_, seekTrack_})
        node->SetEnabled(enabled);
    SetIconAlpha(*playIcon_, ui_colors::White, enabled);
    SetIconAlpha(*stopIcon_, ui_colors::theme::InkSoft, enabled);
    SetIconAlpha(*nextIcon_, ui_colors::theme::InkSoft, enabled);
    RequireComponent<SpriteVisualComponent>(*playIcon_).SetImage(display_.playing ? pauseImage_ : playImage_);
}

bool TitlePlayerView::ArtworkReleased() const noexcept
{
    return artworkHandle_ && visuals_.GetImageSize(artworkHandle_).width <= 0;
}

void TitlePlayerView::ApplyArtwork(const std::filesystem::path &path)
{
    if (artworkImage_ == nullptr)
        return;
    const bool released = ArtworkReleased();
    if (path == artworkPath_ && !released)
        return;
    // The player keeps one album art, so stepping through many songs here does not pile up images. Song select
    // registers the previous one again if it shows that song.
    if (artworkHandle_ && !released)
        visuals_.ReleaseImage(artworkHandle_);
    artworkPath_ = path;
    artworkHandle_ = {};
    artworkImageSize_ = {};
    auto &sprite = RequireComponent<SpriteVisualComponent>(*artworkImage_);
    sprite.SetImage({});
    artworkImage_->SetVisible(false);
    if (path.empty())
        return;
    try
    {
        const auto image = visuals_.RegisterImage(path);
        const auto size = visuals_.GetImageSize(image);
        if (image.value == 0 || size.width <= 0 || size.height <= 0)
            return;
        artworkImageSize_ = size;
        artworkHandle_ = image;
        sprite.SetImage(image);
        LayoutArtworkImage();
        artworkImage_->SetVisible(true);
    }
    catch (const std::exception &)
    {
        // Missing or corrupt artwork keeps the empty box until the song changes.
    }
}

void TitlePlayerView::LayoutArtworkImage()
{
    if (artworkImage_ == nullptr || artworkImageSize_.width <= 0 || artworkImageSize_.height <= 0)
        return;
    // Cover the square from the middle of the chart background, like the song select jacket.
    const auto scale = std::max(ArtworkBounds.width / artworkImageSize_.width,
                                ArtworkBounds.height / artworkImageSize_.height);
    const Float2 uvScale{ArtworkBounds.width / (artworkImageSize_.width * scale),
                         ArtworkBounds.height / (artworkImageSize_.height * scale)};
    RequireComponent<SpriteVisualComponent>(*artworkImage_).SetUvTransform(
        uvScale, {(1 - uvScale.x) * .5F, (1 - uvScale.y) * .5F});
}

void TitlePlayerView::ApplyProgress()
{
    if (seekFill_ == nullptr)
        return;
    const auto duration = display_.hasSong ? display_.durationMicroseconds.value_or(0) : 0;
    const auto shown = seekDrag_ && duration > 0
                           ? static_cast<std::uint64_t>(static_cast<double>(*seekDrag_) * static_cast<double>(duration))
                           : display_.positionMicroseconds;
    const float ratio = duration > 0 ? std::clamp(static_cast<float>(static_cast<double>(shown) / static_cast<double>(duration)), 0.0F, 1.0F)
                                     : 0.0F;
    const float fillWidth = SeekBounds.width * ratio;
    const float left = SeekBounds.x + originX_;
    seekFill_->SetVisible(fillWidth > 0.0F);
    if (fillWidth > 0.0F)
    {
        // The fill shows the part of its image that has played, cropped from the left.
        seekFill_->SetBounds(TopLeft({left, SeekBounds.y + SeekFillTop, fillWidth, SeekFillHeight}, boardHeight_));
        RequireComponent<SpriteVisualComponent>(*seekFill_).SetUvTransform({ratio, 1.0F}, {0.0F, 0.0F});
    }
    seekKnob_->SetVisible(display_.hasSong && duration > 0);
    seekKnob_->SetBounds(TopLeft({left + fillWidth - SeekKnobSize.width * 0.5F, SeekBounds.y + SeekKnobTop,
                                  SeekKnobSize.width, SeekKnobSize.height},
                                 boardHeight_));

    auto elapsed = display_.hasSong ? FormatPlayerTime(shown) : std::wstring{};
    auto length = display_.hasSong && duration > 0 ? FormatPlayerTime(duration) : std::wstring{};
    if (display_.hasSong && duration == 0)
        length = L"-:--";
    if (elapsed != elapsedText_)
    {
        elapsedText_ = std::move(elapsed);
        RequireComponent<TextVisualComponent>(*elapsedLabel_).SetText(elapsedText_);
    }
    if (length != lengthText_)
    {
        lengthText_ = std::move(length);
        RequireComponent<TextVisualComponent>(*lengthLabel_).SetText(lengthText_);
    }
}

std::optional<TitlePlayerRequest> TitlePlayerView::ProcessAction(const Action &action) const
{
    if (action.type != ActionType::Clicked || !display_.hasSong || playButton_ == nullptr)
        return std::nullopt;
    if (action.source == playButton_->Id())
        return TitlePlayerRequest{TitlePlayerAction::TogglePlay};
    if (action.source == stopButton_->Id())
        return TitlePlayerRequest{TitlePlayerAction::Stop};
    if (action.source == nextButton_->Id())
        return TitlePlayerRequest{TitlePlayerAction::Next};
    return std::nullopt;
}

std::optional<TitlePlayerRequest> TitlePlayerView::ProcessSeekPointer(const InputState &input,
                                                                      const std::optional<Point> canvasPointer,
                                                                      const NodeId hovered)
{
    const bool seekable = display_.hasSong && display_.durationMicroseconds.value_or(0) > 0;
    if (!seekable || seekTrack_ == nullptr)
    {
        CancelSeek();
        return std::nullopt;
    }
    const bool pressedOnBar = input.WasMouseButtonPressed(MouseButton::Left) && hovered == seekTrack_->Id();
    if (pressedOnBar && canvasPointer)
        seekDrag_ = SeekRatioAt(*canvasPointer);
    if (!seekDrag_)
        return std::nullopt;
    if (canvasPointer)
        seekDrag_ = SeekRatioAt(*canvasPointer);
    std::optional<TitlePlayerRequest> request;
    if (input.WasMouseButtonReleased(MouseButton::Left))
        request = TitlePlayerRequest{TitlePlayerAction::Seek, *seekDrag_};
    const bool held = input.IsMouseButtonDown(MouseButton::Left);
    if (request || !held)
        seekDrag_.reset();
    ApplyProgress();
    return request;
}

void TitlePlayerView::CancelSeek() noexcept
{
    seekDrag_.reset();
}

float TitlePlayerView::SeekRatioAt(const Point canvasPointer) const
{
    const Rect bar = seekTrack_->BoundsInCanvas();
    if (bar.width <= 0.0F)
        return 0.0F;
    return std::clamp((canvasPointer.x - bar.x) / bar.width, 0.0F, 1.0F);
}

void TitlePlayerView::Reset() noexcept
{
    seekDrag_.reset();
    artworkPath_.clear();
    artworkImageSize_ = {};
    elapsedText_.clear();
    lengthText_.clear();
    display_ = {};
    artworkBox_ = artworkImage_ = artworkFrame_ = musicIcon_ = nullptr;
    artistLabel_ = titleLabel_ = elapsedLabel_ = lengthLabel_ = nullptr;
    seekTrack_ = seekFill_ = seekKnob_ = nullptr;
    playButton_ = playIcon_ = stopButton_ = stopIcon_ = nextButton_ = nextIcon_ = nullptr;
    playImage_ = pauseImage_ = {};
}
