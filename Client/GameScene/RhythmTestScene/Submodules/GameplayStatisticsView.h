#pragma once
#include "PhantomCore.h"
#include "Mode/Submodules/PlaySession.h"
#include "Texts/TextCatalog.h"
#include <array>
#include <cstdint>
#include <optional>

// Live play statistics of the redesign (UI-02): the score beside the accuracy
// in the top-right panel, the combo inside the key layout and the eight
// judgement counts in the bottom-right panel. Every text sits on a translucent
// white panel so it stays readable over any chart BG. Values are text in the
// numeric face and are rewritten only when they change.
class GameplayStatisticsView final
{
  public:
    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, PhantomCore::Visual2DNode &root,
                    PhantomCore::Visual2DNode &inputPanel, TextCatalog &texts);
    void SetLayout(float width, PhantomCore::Size inputPanel);
    // Canvas bounds reserved for the accuracy text inside the top-right panel.
    // The width fits 100.00% so the panel never changes size during play.
    [[nodiscard]] PhantomCore::Rect AccuracyBounds() const noexcept { return accuracyBounds_; }
    void Update(const PlaySession &session);
    void Shutdown() noexcept;

  private:
    struct Value
    {
        PhantomCore::Visual2DNode *node{};
        std::optional<std::uint64_t> shown;
        void Set(std::uint64_t value);
    };
    Value combo_, score_;
    std::array<Value, 8> counts_;
    std::array<PhantomCore::Visual2DNode *, 8> labels_{}, dots_{};
    PhantomCore::Visual2DNode *comboLabel_{}, *scorePanel_{}, *countsPanel_{};
    PhantomCore::Rect accuracyBounds_{};
};
