#include "Mode/PlayGameMode.h"
#include <stdexcept>

bool ModeLoadResult::Succeeded() const noexcept
{
    if (session == nullptr)
    {
        return false;
    }
    return std::ranges::none_of(diagnostics, [](const Diagnostic &diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::Error;
    });
}

std::string IPlayGameMode::SaveResultDetail(const IPlayResult &) const
{
    return {};
}
std::unique_ptr<IPlayResult> IPlayGameMode::LoadResult(PlayResultSummary, std::string_view) const
{
    throw std::runtime_error("This game mode keeps no result detail.");
}
