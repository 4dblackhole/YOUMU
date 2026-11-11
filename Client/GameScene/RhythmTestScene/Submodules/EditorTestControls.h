#pragma once
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <optional>

class EditorTestControls final
{
  public:
    EditorTestControls(PhantomCore::ScreenVisual2DManager &visuals, TextCatalog &texts) : visuals_(visuals), texts_(texts) {}
    void Initialize(std::uint32_t width, std::uint32_t height);
    void Resize(std::uint32_t width, std::uint32_t height);
    void SetVisible(bool visible) { static_cast<void>(canvas_.SetVisible(visible)); }
    std::optional<double> Update(const PhantomCore::InputState &input, double rate, bool paused);
    void Shutdown() noexcept;

  private:
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    PhantomCore::ScreenCanvasHandle canvas_;
    PhantomCore::Visual2DInputRouter input_;
    PhantomCore::Visual2DNode *panel_{}, *help_{}, *caption_{}, *slider_{};
    std::uint32_t width_{}, height_{};
    std::uint64_t textRevision_{};
};
