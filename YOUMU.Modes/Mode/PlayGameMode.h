#pragma once
#include "Mode/ModeInputLayout.h"
#include "Mode/Submodules/PlaySession.h"
#include <stop_token>

struct ModeLoadResult
{
    std::unique_ptr<PlaySession> session;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept;
};

class IPlayGameMode
{
  public:
    virtual ~IPlayGameMode() = default;
    [[nodiscard]] virtual ModeInputLayout InputLayout() const = 0;
    [[nodiscard]] virtual std::string_view Id() const noexcept = 0;
    [[nodiscard]] virtual ModeLoadResult LoadSession(
        const std::filesystem::path &patternPath,
        const std::optional<std::filesystem::path> &effectPath = std::nullopt,
        std::stop_token stop = {}) const = 0;
    [[nodiscard]] virtual ModeLoadResult CreateSession(
        const PatternDocument &pattern,
        const EffectDocument &effects = {},
        std::stop_token stop = {}) const = 0;
};
