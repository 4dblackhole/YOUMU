#pragma once
#include "Mode/PlayGameMode.h"

[[nodiscard]] std::vector<ModeInputLayout> AvailableInputLayouts();
[[nodiscard]] bool IsSupportedPlayGameMode(std::string_view id) noexcept;
[[nodiscard]] std::unique_ptr<IPlayGameMode> CreatePlayGameMode(std::string_view id,
                                                                PlaySessionOptions options = {});

// Uses the same mode construction as gameplay; unsupported/invalid charts stay unknown.
[[nodiscard]] std::optional<std::size_t> CountPlayableNotes(
    const PatternDocument& pattern, const EffectDocument& effects = {});
