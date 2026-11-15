#pragma once

#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

#include <cstdint>
#include <string>
#include <vector>

// The F7 performance band (UI-02 common part): a navy band fixed to the bottom-right corner with a
// slanted left edge, reading "FPS 144 | UPS 1000". Names are faint captions and values italic
// numbers. Debug builds add the GPU draw calls per frame and the busiest frame of the sample.
// It sits on its own Canvas above every screen; the game toggles it with F7.
class PerformanceBand final
{
  public:
#if defined(_DEBUG)
    static constexpr bool ShowsDrawCalls = true;
#else
    static constexpr bool ShowsDrawCalls = false;
#endif
    static constexpr float Height = 24;

    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, TextCatalog &texts);
    void Shutdown() noexcept;
    void SetVisible(bool visible) noexcept;
    [[nodiscard]] bool IsVisible() const noexcept { return visible_; }
    // Takes a new measurement; values read "—" until the first one arrives.
    void Refresh(const PhantomCore::PerformanceStatistics &performance);
    // Lays the band out again once the renderer has measured its text or the language changed.
    void Update();

    // The band's segments for tests: each name and its value, in order.
    struct Segment
    {
        std::wstring name, value;
        // A thin rule separates this segment from the one before it.
        bool separated{};
    };
    [[nodiscard]] const std::vector<Segment> &Segments() const noexcept { return segments_; }
    [[nodiscard]] PhantomCore::Visual2DNode *Band() const noexcept { return band_; }

  private:
    struct Parts
    {
        PhantomCore::Visual2DNode *name{}, *value{}, *rule{};
    };
    void BuildSegments(const PhantomCore::PerformanceStatistics *performance);
    void Layout();

    PhantomCore::ScreenCanvasHandle canvas_;
    TextCatalog *texts_{};
    PhantomCore::Visual2DNode *band_{}, *body_{}, *slant_{};
    std::vector<Segment> segments_;
    std::vector<Parts> parts_;
    PhantomCore::PerformanceStatistics last_{};
    std::uint64_t textRevision_{};
    float laidOutWidth_{};
    bool visible_{};
};
