#include "Mode/PlayGameModeFactory.h"
#include "Parsing/LoadCancellation.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace
{
    struct ModeRegistration
    {
        std::string_view id;
        std::unique_ptr<IPlayGameMode> (*create)(PlaySessionOptions);
    };

    constexpr std::array Modes{
        ModeRegistration{"Taiko", [](PlaySessionOptions options) -> std::unique_ptr<IPlayGameMode> {
            return std::make_unique<TaikoMode>(std::move(options));
        }}
    };

    const ModeRegistration *FindMode(std::string_view id) noexcept
    {
        const auto found = std::ranges::find(Modes, id, &ModeRegistration::id);
        return found == Modes.end() ? nullptr : &*found;
    }
}

std::vector<ModeInputLayout> AvailableInputLayouts()
{
    std::vector<ModeInputLayout> layouts;
    layouts.reserve(Modes.size());
    for (const auto &mode : Modes)
        layouts.push_back(mode.create({})->InputLayout());
    return layouts;
}

bool IsSupportedPlayGameMode(std::string_view id) noexcept
{
    return FindMode(id) != nullptr;
}
std::unique_ptr<IPlayGameMode> CreatePlayGameMode(std::string_view id, PlaySessionOptions options)
{
    if (const auto *mode = FindMode(id))
        return mode->create(std::move(options));
    throw std::invalid_argument("Unsupported game mode: " + std::string(id));
}

std::optional<std::size_t> CountPlayableNotes(
    const PatternDocument& pattern, const EffectDocument& effects, std::stop_token stop)
{
    CheckChartLoadCancellation(stop);
    if (!IsSupportedPlayGameMode(pattern.mode)) return std::nullopt;
    const auto result = CreatePlayGameMode(pattern.mode)->CreateSession(pattern, effects, stop);
    return result.Succeeded()
        ? std::optional{result.session->PlayableNoteCount()} : std::nullopt;
}
