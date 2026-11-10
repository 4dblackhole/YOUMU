#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

enum class ModeKeyPanel { InlineRows, TaikoOverlay };
enum class ModeHitSounds { None, Taiko };

struct ModeBindingCaption
{
    std::string_view id;
    std::string_view textKey;
};

struct ModePresentation
{
    std::string_view modeId;
    ModeHitSounds hitSounds;
    ModeKeyPanel keyPanel;
    float keyPanelHeight;
    std::span<const ModeBindingCaption> bindingCaptions;
};

[[nodiscard]] inline const ModePresentation *FindModePresentation(std::string_view modeId) noexcept
{
    static constexpr std::array TaikoCaptions{
        ModeBindingCaption{"LeftKat", "Bindings.LeftKat"},
        ModeBindingCaption{"LeftDon", "Bindings.LeftDon"},
        ModeBindingCaption{"RightDon", "Bindings.RightDon"},
        ModeBindingCaption{"RightKat", "Bindings.RightKat"}
    };
    static constexpr std::array Modes{
        ModePresentation{"Taiko", ModeHitSounds::Taiko, ModeKeyPanel::TaikoOverlay, 84, TaikoCaptions}
    };
    const auto found = std::ranges::find(Modes, modeId, &ModePresentation::modeId);
    return found == Modes.end() ? nullptr : &*found;
}
