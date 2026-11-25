#pragma once
#include "Mode/PlayGameMode.h"

[[nodiscard]] std::vector<ModeInputLayout> AvailableInputLayouts();
[[nodiscard]] bool IsSupportedPlayGameMode(std::string_view id) noexcept;
[[nodiscard]] std::unique_ptr<IPlayGameMode> CreatePlayGameMode(std::string_view id,
                                                                PlaySessionOptions options = {});

// Uses the same mode construction as gameplay; unsupported/invalid charts stay unknown, and so does a count
// whose stop was requested (the caller that cancelled checks its own token).
[[nodiscard]] std::optional<std::size_t> CountPlayableNotes(
    const PatternDocument& pattern, const EffectDocument& effects = {}, std::stop_token stop = {});
