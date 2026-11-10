#pragma once
#include "PhantomCore.h"
#include <filesystem>

class TextCatalog;

class SongBackgroundPreview final
{
  public:
    explicit SongBackgroundPreview(ScreenVisual2DManager &visuals) : visuals_(visuals) {}
    void CreateBackground(Visual2DNode &board);
    void BindPreview(Visual2DNode &panel, Visual2DNode &image, Visual2DNode &title, Visual2DNode &empty);
    void SetImage(const std::filesystem::path &path, Size viewport, double offsetX = 0, double offsetY = 0);
    void LayoutBackground(Size viewport);
    [[nodiscard]] float LayoutPreview(float panelWidth, float parentHeight);
    void ApplyTexts(const TextCatalog &texts);
    void Reset() noexcept;

  private:
    void LayoutImage();

    ScreenVisual2DManager &visuals_;
    std::filesystem::path previewPath_;
    Size previewImageSize_{};
    Point chartBackgroundOffset_{};
    Visual2DNode *chartBackground_{};
    Visual2DNode *preview_{};
    Visual2DNode *previewImage_{};
    Visual2DNode *previewTitle_{};
    Visual2DNode *previewEmpty_{};
};
