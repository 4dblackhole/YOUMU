#include "MusicSelectView.h"
#include "SongSelectLayout.h"

using namespace song_select;

void MusicSelectView::UpdateSongThumbnail(SongCard &card, const SongCatalogPattern *pattern, bool focused)
{
    const auto path = pattern && !pattern->pattern.backgroundFile.empty()
                          ? settings_.SongsRoot() / L"BG" / pattern->pattern.backgroundFile
                          : std::filesystem::path{};
    const float size = std::min(96.0F, std::max(songContentWidth_ - 28.0F, 1.0F) * .28F);
    card.thumbnail->SetBounds(ScaleTopLeftBounds({14, focused ? 16.0F : 10.0F, size, size}, card.button->NodeSize().height));
    if (path != card.thumbnailPath)
    {
        card.thumbnailPath = path;
        card.thumbnail->SetVisible(false);
        auto &sprite = RequireComponent<SpriteVisualComponent>(*card.thumbnail);
        sprite.SetImage({});
        if (!path.empty())
        {
            try
            {
                const auto image = screenVisuals_.RegisterImage(path);
                const auto imageSize = screenVisuals_.GetImageSize(image);
                if (image && imageSize.width > 0 && imageSize.height > 0)
                {
                    const float scale = std::max(size / imageSize.width, size / imageSize.height);
                    const DirectX::XMFLOAT2 uvScale{size / (imageSize.width * scale), size / (imageSize.height * scale)};
                    sprite.SetImage(image);
                    sprite.SetTint(PureWhite);
                    sprite.SetUvTransform(uvScale, {(1 - uvScale.x) / 2, (1 - uvScale.y) / 2});
                    SetCornerRadius(*card.thumbnail, 8);
                    card.thumbnail->SetVisible(true);
                }
            }
            catch (const std::exception &)
            {
                // Leave the existing card surface visible, without the previous chart's image.
            }
        }
    }
    const float inset = card.thumbnail->IsVisible() ? size + 12 : 0;
    const float width = std::max(songContentWidth_ - 28 - inset - (IsEditorSongSelect() ? 48 : 0), 1.0F);
    card.title->SetBounds(ScaleTopLeftBounds({14 + inset, focused ? 16.0F : 10.0F, width, focused ? 60.0F : 56.0F},
                                           card.button->NodeSize().height));
    card.artist->SetBounds(ScaleTopLeftBounds({14 + inset, focused ? 82.0F : 72.0F, width, focused ? 48.0F : 44.0F},
                                            card.button->NodeSize().height));
}

void MusicSelectView::RefreshSelectedSongThumbnail()
{
    if (selection_.focusedSongPosition_ >= selection_.visibleSongIndices_.size()) return;
    const auto catalogIndex = selection_.visibleSongIndices_[selection_.focusedSongPosition_];
    if (catalogIndex >= selection_.catalog_.songs.size()) return;
    const auto &patterns = selection_.catalog_.songs[catalogIndex].patterns;
    const auto index = patterns.empty() ? 0 : std::min(selection_.selectedPatternIndex_, patterns.size() - 1);
    for (auto &card : songCards_)
        if (card.catalogIndex == catalogIndex)
            UpdateSongThumbnail(card, patterns.empty() ? nullptr : &patterns[index], true);
}
