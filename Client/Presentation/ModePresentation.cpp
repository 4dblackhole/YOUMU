#include "Presentation/ModePresentation.h"
#include "Taiko/Submodules/TaikoSoundIds.h"

#include <algorithm>
#include <array>

namespace
{
    constexpr std::array<const char *, 3> TaikoDonSounds{DonHit, LongNoteTick, DonFreeInput};
    constexpr std::array<const char *, 2> TaikoKatSounds{KatHit, KatFreeInput};
    constexpr std::array<const char *, 1> TaikoBigDonSounds{BigDonFirstHit};
    constexpr std::array<const char *, 1> TaikoBigKatSounds{BigKatFirstHit};
    constexpr std::array<const char *, 1> TaikoBalloonSounds{BalloonPop};
    constexpr std::array TaikoHitSounds{
        ModeHitSoundFile{TaikoDonSounds, L"don.wav"},
        ModeHitSoundFile{TaikoKatSounds, L"kat.wav"},
        ModeHitSoundFile{TaikoBigDonSounds, L"bigdon.wav"},
        ModeHitSoundFile{TaikoBigKatSounds, L"bigkat.wav"},
        ModeHitSoundFile{TaikoBalloonSounds, L"pop.wav"}};

    // Face bounds come from the 228px gameplay input panel so the key editor
    // and the gameplay key display share one skin layout.
    constexpr std::array TaikoKeyFaces{
        ModeKeyOverlayFace{"KeyButtonLeftKat", {.04F, .57F, .81F, 1}, {16, 68, 39, 70}, L"Left K"},
        ModeKeyOverlayFace{"KeyButtonLeftDon", {.92F, .07F, .11F, 1}, {68, 92, 39, 70}, L"Left D"},
        ModeKeyOverlayFace{"KeyButtonRightDon", {.92F, .07F, .11F, 1}, {120, 92, 39, 70}, L"Right D"},
        ModeKeyOverlayFace{"KeyButtonRightKat", {.04F, .57F, .81F, 1}, {172, 68, 39, 70}, L"Right K"}};
    constexpr ModeKeyOverlay TaikoKeyOverlay{
        "Options.Taiko", "Options.taikoKeySettings", "InputPanel", 228.0F, TaikoKeyFaces, 84.0F};

    constexpr std::array TaikoCaptions{
        ModeBindingCaption{"LeftKat", "Bindings.LeftKat"},
        ModeBindingCaption{"LeftDon", "Bindings.LeftDon"},
        ModeBindingCaption{"RightDon", "Bindings.RightDon"},
        ModeBindingCaption{"RightKat", "Bindings.RightKat"}};
} // namespace

const ModePresentation TaikoPresentation{"Taiko", L"TaikoMode", TaikoHitSounds, &TaikoKeyOverlay, TaikoCaptions};

namespace
{
    // Add one entry per mode that needs Client presentation.
    constexpr std::array<const ModePresentation *, 1> Registered{&TaikoPresentation};
} // namespace

std::span<const ModePresentation *const> ModePresentations() noexcept
{
    return Registered;
}

const ModePresentation *FindModePresentation(const std::string_view modeId) noexcept
{
    const auto found = std::ranges::find_if(Registered, [modeId](const ModePresentation *entry) {
        return entry->modeId == modeId;
    });
    return found == Registered.end() ? nullptr : *found;
}
