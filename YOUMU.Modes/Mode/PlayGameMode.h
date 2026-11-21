#pragma once
#include "Mode/ModeInputLayout.h"
#include "Mode/Results/IPlayResult.h"
#include "Mode/Submodules/PlaySession.h"
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

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
    // The detail a local record keeps with its summary so the result screen can be shown again.
    // A mode without a saved format keeps none (empty).
    [[nodiscard]] virtual std::string SaveResultDetail(const IPlayResult &result) const;
    // Rebuilds a recorded result from the summary its record restored and the saved detail.
    // Throws when the detail is damaged or the mode keeps none.
    [[nodiscard]] virtual std::unique_ptr<IPlayResult> LoadResult(PlayResultSummary summary,
                                                                  std::string_view detail) const;
};
