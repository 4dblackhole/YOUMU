#pragma once
#include "Mode/PlayGameMode.h"

[[nodiscard]] std::vector<ModeInputLayout> AvailableInputLayouts();
[[nodiscard]] bool IsSupportedPlayGameMode(std::string_view id) noexcept;
[[nodiscard]] std::unique_ptr<IPlayGameMode> CreatePlayGameMode(std::string_view id,
                                                                PlaySessionOptions options = {});
