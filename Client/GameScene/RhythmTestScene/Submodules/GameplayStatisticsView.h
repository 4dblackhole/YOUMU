#pragma once
#include "Presentation/SpriteNumber.h"
#include "Mode/Submodules/PlaySession.h"
#include "Texts/TextCatalog.h"

class GameplayStatisticsView final
{
  public:
    void Initialize(PhantomCore::ScreenVisual2DManager &visuals, PhantomCore::Visual2DNode &root,
                    PhantomCore::Visual2DNode &inputPanel, TextCatalog &texts);
    void SetLayout(float width, PhantomCore::Size inputPanel, PhantomCore::Rect accuracy);
    void Update(const PlaySession &session);
    void Shutdown() noexcept;
  private:
    SpriteNumber combo_, score_;
    std::array<SpriteNumber, 8> counts_;
    std::array<PhantomCore::Visual2DNode *, 8> labels_{};
    PhantomCore::Visual2DNode *comboLabel_{}, *scoreLabel_{};
};
