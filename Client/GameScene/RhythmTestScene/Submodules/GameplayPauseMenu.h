#pragma once
#include "GameplayPauseAction.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <optional>
#include <chrono>
#include <string>

// Owns only the modal Canvas, focus and parameter editing. Session, judgement
// and audio changes remain in GameplaySessionController.
class GameplayPauseMenu final
{
  public:
    GameplayPauseMenu(PhantomCore::ScreenVisual2DManager &visuals, TextCatalog &texts)
        : visuals_(visuals), texts_(texts)
    {
    }
    ~GameplayPauseMenu()
    {
        Shutdown();
    }
    void Show(std::uint32_t width, std::uint32_t height, bool canResume, std::size_t measureCount);
    void Hide() noexcept;
    GameplayPauseChoice Update(const PhantomCore::InputState &input);
    GameplayPauseChoice HandleKey(std::uint16_t virtualKey);
    void OnResize(std::uint32_t width, std::uint32_t height);
    void Shutdown() noexcept;

  private:
    void Build();
    void BuildRow(PhantomCore::Visual2DNode &parent, std::size_t index);
    void BuildSelector(PhantomCore::Visual2DNode &parent, std::size_t index);
    void RefreshRows();
    void AdjustValue(std::size_t index, int direction);
    void ProcessActions(GameplayPauseChoice &result);
    void ProcessPointer(const PhantomCore::InputState &input);
    PhantomCore::Visual2DNode &Label(PhantomCore::Visual2DNode &parent, PhantomCore::Rect bounds,
                                       std::wstring text, float size, PhantomCore::Color color,
                                       std::uint16_t weight);
    void AnimatePreparing();
    PhantomCore::ScreenVisual2DManager &visuals_;
    TextCatalog &texts_;
    PhantomCore::ScreenCanvasHandle handle_;
    PhantomCore::Visual2DInputRouter router_;
    std::array<PhantomCore::NodeId, 4> rows_{}, frames_{}, captions_{};
    std::array<PhantomCore::NodeId, 2> values_{}, previous_{}, next_{}, selectors_{}, descriptions_{};
    PhantomCore::NodeId rootId_{}, prepId_{}, prepPieceId_{};
    PhantomCore::ImageHandle rowImage_{}, rowSelectedImage_{}, stepperImage_{}, stepperSelectedImage_{};
    std::optional<PhantomCore::Point> lastPointer_;
    std::size_t focus_{}, maxMeasures_{4};
    GameplayPauseChoice choice_;
    std::uint32_t width_{}, height_{};
    std::uint64_t textRevision_{};
    bool visible_{}, canResume_{};
    std::chrono::steady_clock::time_point shownAt_{};
};
