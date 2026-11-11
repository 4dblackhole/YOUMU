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

    strongKeyLightImage_ = visuals.RegisterImage(InGameSkinAssetPath(L"KeyLightStrong"));
    weakKeyLightImage_ = visuals.RegisterImage(InGameSkinAssetPath(L"KeyLightWeak"));
    keyPressFlashImage_ = visuals.RegisterImage(InGameSkinAssetPath(L"KeyPressFlash"));
    const auto lightSize = ScaledImageSize(visuals, strongKeyLightImage_);
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
                                                 strongKeyLightImage_, "Input.Key.Glow");
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
        RequireComponent<SpriteVisualComponent>(pressFlash).SetTint(InputKeyStrongGlowColors[index]);
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
        const bool primaryPressed = input.IsKeyDown(keys.front());
        const bool secondaryPressed =
            std::ranges::any_of(auxiliary, [&input](auto key) { return key != 0 && input.IsKeyDown(key); });
        const bool pressedThisFrame =
            std::ranges::any_of(keys, [&input](auto key) { return key != 0 && input.WasKeyPressed(key); });
        const Color &faceColor =
            primaryPressed ? InputKeyStrongColors[index]
                           : (secondaryPressed ? InputKeyWeakColors[index] : InputKeyBaseColors[index]);
        if (keyIndicators_[index] != nullptr)
        {
            RequireComponent<SpriteVisualComponent>(*keyIndicators_[index]).SetTint(faceColor);
        }
        if (keyGlows_[index] != nullptr)
        {
            auto &glow = RequireComponent<SpriteVisualComponent>(*keyGlows_[index]);
            glow.SetImage(primaryPressed ? strongKeyLightImage_ : weakKeyLightImage_);
            glow.SetTint(primaryPressed ? InputKeyStrongGlowColors[index] : InputKeyWeakGlowColors[index]);
            keyGlows_[index]->SetVisible(primaryPressed || secondaryPressed);
        }
        if (keyPressFlashes_[index] != nullptr)
        {
            double &remaining = keyPressFlashRemainingSeconds_[index];
            if (pressedThisFrame)
            {
                remaining = KeyPressFlashDurationSeconds;
            }
            else
            {
                remaining = std::max(0.0, remaining - std::max(deltaSeconds, 0.0));
            }
            auto &flash = RequireComponent<SpriteVisualComponent>(*keyPressFlashes_[index]);
            auto color = InputKeyStrongGlowColors[index];
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
    strongKeyLightImage_ = {};
    weakKeyLightImage_ = {};
    keyPressFlashImage_ = {};
}
