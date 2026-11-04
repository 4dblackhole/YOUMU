#pragma once
#include "Presentation/SpriteNumber.h"
#include "Mode/Submodules/PlaySession.h"
#include "Texts/TextCatalog.h"

class GameplayStatisticsView final
{
  public:
    void Initialize(ScreenVisual2DManager &visuals, Visual2DNode &root,
                    Visual2DNode &inputPanel, TextCatalog &texts);
    void SetLayout(float width, Size inputPanel, Rect accuracy);
    void Update(const PlaySession &session);
    void Shutdown() noexcept;
  private:
    SpriteNumber combo_, score_;
    std::array<SpriteNumber, 8> counts_;
    std::array<Visual2DNode *, 8> labels_{};
    Visual2DNode *comboLabel_{}, *scoreLabel_{};
};
