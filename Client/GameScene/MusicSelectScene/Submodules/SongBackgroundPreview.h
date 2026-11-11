#pragma once
#include "PhantomCore.h"
#include <filesystem>

class TextCatalog;

class SongBackgroundPreview final
{
  public:
    explicit SongBackgroundPreview(PhantomCore::ScreenVisual2DManager &visuals) : visuals_(visuals) {}
    void CreateBackground(PhantomCore::Visual2DNode &board);
    void BindPreview(PhantomCore::Visual2DNode &panel, PhantomCore::Visual2DNode &image, PhantomCore::Visual2DNode &title, PhantomCore::Visual2DNode &empty);
    void SetImage(const std::filesystem::path &path, PhantomCore::Size viewport, double offsetX = 0, double offsetY = 0);
    void LayoutBackground(PhantomCore::Size viewport);
    [[nodiscard]] float LayoutPreview(float panelWidth, float parentHeight);
    void ApplyTexts(const TextCatalog &texts);
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
    PhantomCore::Visual2DNode *previewTitle_{};
    PhantomCore::Visual2DNode *previewEmpty_{};
};
