#pragma once
#include "PhantomCore.h"
#include <filesystem>
#include <vector>

class SongBackgroundPreview final
{
  public:
    // Chart backgrounds stay registered while they are among the last RecentBackgrounds shown here; older ones
    // are released, so browsing many songs keeps a bounded number of textures (graphics memory, and the engine's
    // 1,024 image descriptors that every screen shares).
    static constexpr std::size_t RecentBackgrounds = 64;

    explicit SongBackgroundPreview(PhantomCore::ScreenVisual2DManager &visuals) : visuals_(visuals)
    {
        recent_.reserve(RecentBackgrounds + 1);
    }
    void CreateBackground(PhantomCore::Visual2DNode &board);
    // The jacket frame is a skinned sprite; the square-cropped chart BG sits inside it.
    void BindPreview(PhantomCore::Visual2DNode &frame, PhantomCore::Visual2DNode &image);
    void SetImage(const std::filesystem::path &path, PhantomCore::Size viewport, double offsetX = 0, double offsetY = 0);
    void LayoutBackground(PhantomCore::Size viewport);
    // bounds: the picture square in its parent's coordinates.
    void LayoutPreview(PhantomCore::Rect bounds);
    void Reset() noexcept;

  private:
    struct Shown
    {
        std::filesystem::path path;
        PhantomCore::ImageHandle image;
    };
    void LayoutImage();
    void Remember(const std::filesystem::path &path, PhantomCore::ImageHandle image);

    PhantomCore::ScreenVisual2DManager &visuals_;
    std::filesystem::path previewPath_;
    PhantomCore::ImageHandle previewHandle_;
    std::vector<Shown> recent_; // Oldest first; the last one is on screen.
    PhantomCore::Size previewImageSize_{};
    PhantomCore::Point chartBackgroundOffset_{};
    PhantomCore::Visual2DNode *chartBackground_{};
    PhantomCore::Visual2DNode *preview_{};
    PhantomCore::Visual2DNode *previewImage_{};
};
