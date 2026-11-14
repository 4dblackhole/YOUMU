#include "SongBackgroundPreview.h"
#include "Presentation/BackgroundLayout.h"
#include "SongSelectLayout.h"

using namespace PhantomCore;

using namespace song_select;

void SongBackgroundPreview::CreateBackground(Visual2DNode &board)
{
    chartBackground_ = &CreateSprite(board, {}, {}, "SongBackground");
    chartBackground_->SetVisible(false);
}

namespace
{
    // The exported frame is 309px for a 196px picture at 1.5x: 5 logical px of rim per side.
    constexpr float JacketRim = 5.0F;
}

void SongBackgroundPreview::BindPreview(Visual2DNode &frame, Visual2DNode &image)
{
    preview_ = &frame;
    previewImage_ = &image;
    BindSkinImage(frame, visuals_, "MusicSelect", "Detail.JacketEmpty");
}

void SongBackgroundPreview::SetImage(const std::filesystem::path &path, Size viewport,
                                     double offsetX, double offsetY)
{
    chartBackgroundOffset_ = {static_cast<float>(offsetX), static_cast<float>(offsetY)};
    if (previewPath_ == path)
    {
        LayoutBackground(viewport);
        return;
    }
    previewPath_ = path;
    previewImageSize_ = {};
    previewImage_->SetVisible(false);
    RequireComponent<SpriteVisualComponent>(*previewImage_).SetImage({});
    chartBackground_->SetVisible(false);
    RequireComponent<SpriteVisualComponent>(*chartBackground_).SetImage({});
    BindSkinImage(*preview_, visuals_, "MusicSelect", "Detail.JacketEmpty");
    if (path.empty())
        return;
    try
    {
        const auto image = visuals_.RegisterImage(previewPath_);
        const auto size = visuals_.GetImageSize(image);
        if (image.value == 0 || size.width <= 0 || size.height <= 0)
            return;
        previewImageSize_ = size;
        auto &sprite = RequireComponent<SpriteVisualComponent>(*previewImage_);
        sprite.SetImage(image);
        sprite.SetTint(PureWhite);
        auto &background = RequireComponent<SpriteVisualComponent>(*chartBackground_);
        background.SetImage(image);
        background.SetTint(PureWhite);
        LayoutBackground(viewport);
        chartBackground_->SetVisible(true);
        LayoutImage();
        previewImage_->SetVisible(true);
        BindSkinImage(*preview_, visuals_, "MusicSelect", "Detail.Jacket");
    }
    catch (const std::exception &)
    {
        // Missing/corrupt artwork keeps the empty jacket until the selected path changes.
    }
}

void SongBackgroundPreview::LayoutBackground(Size viewport)
{
    if (!chartBackground_) return;
    auto bounds = ChartBackgroundBounds(viewport, previewImageSize_, chartBackgroundOffset_);
    // The board's children use its lower-left origin, while gameplay/result use Centre.
    bounds.x += viewport.width / 2;
    bounds.y += viewport.height / 2;
    chartBackground_->SetBounds(bounds);
}

void SongBackgroundPreview::LayoutImage()
{
    if (previewImage_ == nullptr)
        return;
    const Size size{std::max(preview_->NodeSize().width - JacketRim * 2, 1.0F),
                    std::max(preview_->NodeSize().height - JacketRim * 2, 1.0F)};
    previewImage_->SetBounds({JacketRim, JacketRim, size.width, size.height});
    if (previewImageSize_.width <= 0 || previewImageSize_.height <= 0)
        return;
    const auto scale = std::max(size.width / previewImageSize_.width,
                                size.height / previewImageSize_.height);
    const Float2 uvScale{size.width / (previewImageSize_.width * scale),
                                   size.height / (previewImageSize_.height * scale)};
    RequireComponent<SpriteVisualComponent>(*previewImage_).SetUvTransform(
        uvScale, {(1 - uvScale.x) * .5F, (1 - uvScale.y) * .5F});
}

void SongBackgroundPreview::LayoutPreview(const Rect bounds)
{
    if (!preview_)
        return;
    preview_->SetBounds({bounds.x - JacketRim, bounds.y - JacketRim, bounds.width + JacketRim * 2,
                         bounds.height + JacketRim * 2});
    LayoutImage();
}

void SongBackgroundPreview::Reset() noexcept
{
    previewPath_.clear();
    previewImageSize_ = {};
    chartBackgroundOffset_ = {};
    chartBackground_ = preview_ = previewImage_ = nullptr;
}
