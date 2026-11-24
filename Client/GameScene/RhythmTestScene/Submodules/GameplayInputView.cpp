#include "GameplayInputView.h"
#include "GameplaySupport.h"

using namespace PhantomCore;

void GameplayInputView::Initialize(
    ScreenVisual2DManager& visuals, Visual2DNode& inputPanel)
{
    Shutdown();
    const Size inputPanelSize = inputPanel.NodeSize();
    struct KeySpec
    {
        Rect bounds;
        Color baseColor;
        std::wstring_view image;
    };
    constexpr std::array<KeySpec, 4> keys{{
        {{16.0F, 68.0F, 39.0F, 70.0F}, InputKeyBaseColors[0], L"KeyButtonLeftKat"},
        {{68.0F, 92.0F, 39.0F, 70.0F}, InputKeyBaseColors[1], L"KeyButtonLeftDon"},
        {{120.0F, 92.0F, 39.0F, 70.0F}, InputKeyBaseColors[2], L"KeyButtonRightDon"},
        {{172.0F, 68.0F, 39.0F, 70.0F}, InputKeyBaseColors[3], L"KeyButtonRightKat"},
    }};

    keyLightImage_ = visuals.RegisterImage(InGameSkinAssetPath(L"KeyLight"));
    keyPressFlashImage_ = visuals.RegisterImage(InGameSkinAssetPath(L"KeyPressFlash"));
    const auto lightSize = ScaledImageSize(visuals, keyLightImage_);
    const auto pressFlashSize = ScaledImageSize(visuals, keyPressFlashImage_);
    for (std::size_t index = 0; index < keys.size(); ++index)
    {
        const Rect bounds{
            keys[index].bounds.x * InGameAssetScale,
            inputPanelSize.height - (keys[index].bounds.y + keys[index].bounds.height) * InGameAssetScale,
            keys[index].bounds.width * InGameAssetScale, keys[index].bounds.height * InGameAssetScale};
        auto &glow = CreateSprite(inputPanel,
                                                 {bounds.x - (lightSize.width - bounds.width) * 0.5F,
                                                  bounds.y - (lightSize.height - bounds.height) * 0.5F, lightSize.width,
                                                  lightSize.height},
                                                 keyLightImage_, "Input.Key.Glow");
        RequireComponent<SpriteVisualComponent>(glow).SetTint(keys[index].baseColor);
        glow.SetVisible(false);
        glow.SetZIndex(1);
        keyGlows_[index] = &glow;

        const auto keyImage = visuals.RegisterImage(InGameSkinAssetPath(keys[index].image));
        auto &face = CreateSprite(inputPanel, bounds, keyImage, "Input.Key.Face");
        RequireComponent<SpriteVisualComponent>(face).SetTint(keys[index].baseColor);
        face.SetZIndex(2);
        keyIndicators_[index] = &face;

        auto &pressFlash = CreateSprite(inputPanel,
                                                       {bounds.x - (pressFlashSize.width - bounds.width) * 0.5F,
                                                        bounds.y - (pressFlashSize.height - bounds.height) * 0.5F,
                                                        pressFlashSize.width, pressFlashSize.height},
                                                       keyPressFlashImage_, "Input.Key.PressFlash");
        RequireComponent<SpriteVisualComponent>(pressFlash).SetTint(KeyPressFlashColor);
        pressFlash.SetVisible(false);
        pressFlash.SetZIndex(3);
        keyPressFlashes_[index] = &pressFlash;
    }
}

void GameplayInputView::Update(
    const ModeInputLayout& layout, const InputState &input, const double deltaSeconds)
{
    for (std::size_t index = 0; index < keyIndicators_.size(); ++index)
    {
        const auto &bindings = layout.bindings;
        if (index >= bindings.size() || bindings[index].keys.empty())
            continue;
        const auto &keys = bindings[index].keys;
        const auto auxiliary = std::span(keys).subspan(1);
        // An unassigned primary (0) never lights or flashes; auxiliary keys still light the key the same way.
        const PhysicalKey primary = keys.front();
        const bool primaryPressed = primary != 0 && input.IsKeyDown(primary);
        const bool secondaryPressed =
            std::ranges::any_of(auxiliary, [&input](auto key) { return key != 0 && input.IsKeyDown(key); });
        // Every assigned key lights the key identically; only the first assigned key adds the cross-shaped flash.
        const bool anyPressed = primaryPressed || secondaryPressed;
        const bool primaryPressedThisFrame = primary != 0 && input.WasKeyPressed(primary);
        const Color &faceColor = anyPressed ? InputKeyPressedColors[index] : InputKeyBaseColors[index];
        if (keyIndicators_[index] != nullptr)
        {
            RequireComponent<SpriteVisualComponent>(*keyIndicators_[index]).SetTint(faceColor);
        }
        if (keyGlows_[index] != nullptr)
        {
            auto &glow = RequireComponent<SpriteVisualComponent>(*keyGlows_[index]);
            glow.SetTint(InputKeyGlowColors[index]);
            keyGlows_[index]->SetVisible(anyPressed);
        }
        if (keyPressFlashes_[index] != nullptr)
        {
            double &remaining = keyPressFlashRemainingSeconds_[index];
            if (primaryPressedThisFrame)
            {
                remaining = KeyPressFlashDurationSeconds;
            }
            else
            {
                remaining = std::max(0.0, remaining - std::max(deltaSeconds, 0.0));
            }
            auto &flash = RequireComponent<SpriteVisualComponent>(*keyPressFlashes_[index]);
            auto color = KeyPressFlashColor;
            color.alpha = static_cast<float>(remaining / KeyPressFlashDurationSeconds);
            flash.SetTint(color);
            keyPressFlashes_[index]->SetVisible(remaining > 0.0);
        }
    }
}

void GameplayInputView::Shutdown() noexcept
{
    keyIndicators_.fill(nullptr);
    keyGlows_.fill(nullptr);
    keyPressFlashes_.fill(nullptr);
    keyPressFlashRemainingSeconds_.fill(0.0);
    keyLightImage_ = {};
    keyPressFlashImage_ = {};
}
