#pragma once
#include "PhantomCore.h"
#include <string>

// A node owns its skin binding. Removing a virtualized song card also
// removes the binding; no external registry keeps dead node references.
class SkinVisualComponent final : public PhantomCore::Visual2DComponent
{
  public:
    SkinVisualComponent(PhantomCore::ScreenVisual2DManager &visuals, std::string scene, std::string key,
                        PhantomCore::VisualStyle base, float radius);
    void Configure(std::string key, std::string decoration = {});
    // The image carries its designed opacity; the fallback colour's alpha applies only without it.
    void UseImageOpacity();
    void SetBaseStyle(const PhantomCore::VisualStyle &style);
    void Update(double elapsedSeconds) override;

  private:
    bool Refresh();
    void Apply();
    PhantomCore::ScreenVisual2DManager &visuals_;
    std::string scene_, key_, decoration_;
    PhantomCore::VisualStyle base_, images_;
    float radius_{};
    std::uint64_t revision_{};
    bool loaded_{};
    bool imageOpacity_{};
};
void BindSkinImage(PhantomCore::Visual2DNode &node, PhantomCore::ScreenVisual2DManager &visuals,
                   std::string scene, std::string key, std::string decoration = {});
// Binds a part whose image already carries its designed opacity. The fallback colour, alpha
// included, is used only when no image is found; with BindSkinImage a translucent fallback
// would thin a translucent image a second time. A disabled part without its own image is dimmed.
void BindSkinPart(PhantomCore::Visual2DNode &node, PhantomCore::ScreenVisual2DManager &visuals,
                  std::string scene, std::string key);
// Dynamic selection styles update the saved procedural default as well.
void SetSkinBaseStyle(PhantomCore::Visual2DNode &node, const PhantomCore::VisualStyle &style);
