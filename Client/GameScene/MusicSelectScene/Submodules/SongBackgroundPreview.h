#pragma once
#include "PhantomCore.h"
#include <filesystem>

class SongBackgroundPreview final
{
  public:
    explicit SongBackgroundPreview(PhantomCore::ScreenVisual2DManager &visuals) : visuals_(visuals) {}
    void CreateBackground(PhantomCore::Visual2DNode &board);
    // The jacket frame is a skinned sprite; the square-cropped chart BG sits inside it.
    void BindPreview(PhantomCore::Visual2DNode &frame, PhantomCore::Visual2DNode &image);
    void SetImage(const std::filesystem::path &path, PhantomCore::Size viewport, double offsetX = 0, double offsetY = 0);
    void LayoutBackground(PhantomCore::Size viewport);
    // bounds: the picture square in its parent's coordinates.
    void LayoutPreview(PhantomCore::Rect bounds);
    void Reset() noexcept;

  private:
    void LayoutImage();

    PhantomCore::ScreenVisual2DManager &visuals_;
    std::filesystem::path previewPath_;
    PhantomCore::Size previewImageSize_{};
    PhantomCore::Point chartBackgroundOffset_{};
    PhantomCore::Visual2DNode *chartBackground_{};
    PhantomCore::Visual2DNode *preview_{};
    PhantomCore::Visual2DNode *previewImage_{};
};
