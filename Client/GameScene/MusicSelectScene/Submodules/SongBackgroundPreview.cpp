#include "SongBackgroundPreview.h"
#include "Presentation/BackgroundLayout.h"
#include "SongSelectLayout.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"

using namespace song_select;

void SongBackgroundPreview::CreateBackground(Visual2DNode &board)
{
    chartBackground_ = &CreateSprite(board, {}, {}, "SongBackground");
    chartBackground_->SetVisible(false);
}

void SongBackgroundPreview::BindPreview(Visual2DNode &panel, Visual2DNode &image,
                                        Visual2DNode &title, Visual2DNode &empty)
{
    preview_ = &panel;
    previewImage_ = &image;
    previewTitle_ = &title;
    previewEmpty_ = &empty;
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
    previewTitle_->SetVisible(true);
    previewEmpty_->SetVisible(true);
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
        previewTitle_->SetVisible(false);
        previewEmpty_->SetVisible(false);
    }
    catch (const std::exception &)
    {
        // Missing/corrupt artwork keeps NO IMAGE until the selected path changes.
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
    const auto size = preview_->NodeSize();
    previewImage_->SetBounds({0, 0, size.width, size.height});
    if (previewImageSize_.width <= 0 || previewImageSize_.height <= 0)
        return;
    const auto scale = std::max(size.width / previewImageSize_.width,
                                size.height / previewImageSize_.height);
    const DirectX::XMFLOAT2 uvScale{size.width / (previewImageSize_.width * scale),
                                   size.height / (previewImageSize_.height * scale)};
    RequireComponent<SpriteVisualComponent>(*previewImage_).SetUvTransform(
        uvScale, {(1 - uvScale.x) * .5F, (1 - uvScale.y) * .5F});
}

float SongBackgroundPreview::LayoutPreview(float panelWidth, float parentHeight)
{
    if (!preview_)
        return 0.0F;
    const bool showPreview = panelWidth >= 500.0F;
    preview_->SetVisible(showPreview);
    if (!showPreview)
        return 0.0F;

    const float previewWidth =
        std::clamp(layout::Preview.width * panelWidth / layout::InformationPanel.width, 160.0F,
                   layout::Preview.width);
    preview_->SetBounds(ScaleTopLeftBounds({24.0F, 24.0F, previewWidth, previewWidth}, parentHeight));
    previewTitle_->SetBounds(ScaleTopLeftBounds(
        {20.0F, layout::PreviewTitle.y * previewWidth / layout::Preview.width,
         std::max(previewWidth - 40.0F, 1.0F), layout::PreviewTitle.height},
        preview_->NodeSize().height));
    previewEmpty_->SetBounds(ScaleTopLeftBounds(
        {20.0F, layout::PreviewEmpty.y * previewWidth / layout::Preview.width,
         std::max(previewWidth - 40.0F, 1.0F), layout::PreviewEmpty.height},
        preview_->NodeSize().height));
    LayoutImage();
    return previewWidth;
}

void SongBackgroundPreview::ApplyTexts(const TextCatalog &texts)
{
    if (!preview_)
        return;

    const auto &text = MusicSelect(texts);
    auto &title = RequireComponent<TextVisualComponent>(*previewTitle_);
    title.SetText(std::wstring(text.backgroundPreview));
    texts.ApplyFont(title);
    auto &empty = RequireComponent<TextVisualComponent>(*previewEmpty_);
    empty.SetText(std::wstring(text.noImage));
    texts.ApplyFont(empty);
}

void SongBackgroundPreview::Reset() noexcept
{
    previewPath_.clear();
    previewImageSize_ = {};
    chartBackgroundOffset_ = {};
    chartBackground_ = preview_ = previewImage_ = previewTitle_ = previewEmpty_ = nullptr;
}
