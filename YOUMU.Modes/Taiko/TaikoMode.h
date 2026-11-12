#pragma once

#include "Mode/PlayGameMode.h"
#include "Taiko/Submodules/TaikoNoteDefinition.h"
#include "Taiko/Submodules/TaikoSoundIds.h"

#include <array>
#include <cstddef>

class TaikoMode final : public IPlayGameMode
{
  public:
    explicit TaikoMode(PlaySessionOptions options = {}) : options_(std::move(options))
    {
    }
    [[nodiscard]] ModeInputLayout InputLayout() const override;
    [[nodiscard]] std::string_view Id() const noexcept override;
    [[nodiscard]] ModeLoadResult LoadSession(
        const std::filesystem::path &patternPath,
        const std::optional<std::filesystem::path> &effectPath = std::nullopt,
        std::stop_token stop = {}) const override;
    [[nodiscard]] ModeLoadResult CreateSession(
        const PatternDocument &pattern,
        const EffectDocument &effects = {},
        std::stop_token stop = {}) const override;

  private:
    PlaySessionOptions options_;
};
