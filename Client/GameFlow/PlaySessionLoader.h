#pragma once
#include "GameFlow/GameplayLaunchRequest.h"
#include "GameFlow/ChartLoadJob.h"
#include "Mode/PlayGameModeFactory.h"
#include "Parsing/LoadCancellation.h"
#include <stdexcept>

// The worker owns a request snapshot and domain data only. Poll never waits.
class PlaySessionLoader final
{
  public:
    bool Busy() const noexcept { return job_.Busy(); }
    void Cancel() noexcept { job_.Cancel(); }
    void Start(GameplayLaunchRequest request)
    {
        if (Busy())
            throw std::logic_error("A pattern load is already in progress.");
        job_ = StartChartLoad([request = std::move(request)](std::stop_token stop)
        {
            try
            {
                CheckChartLoadCancellation(stop);
                auto result = CreatePlayGameMode(request.mode,
                    {RhythmDuration{static_cast<std::int64_t>(request.globalOffsetMs) * 1000}, request.inputKeys,
                     request.playOptions, request.editorTestStart})
                    ->LoadSession(request.patternPath, request.effectPath, stop);
                CheckChartLoadCancellation(stop);
                if (result.Succeeded() && !request.editorTestStart && !result.session->CanEnterGameplay())
                {
                    result.session.reset();
                    result.diagnostics.push_back({DiagnosticSeverity::Error, {request.patternPath, 1, 1},
                        "A chart with no playable notes is available only in editor preview/test."});
                }
                return result;
            }
            catch (const ChartLoadCancelled&)
            {
                throw;
            }
            catch (const std::exception& error)
            {
                ModeLoadResult result;
                result.diagnostics.push_back({DiagnosticSeverity::Error, {request.patternPath, 1, 1}, error.what()});
                return result;
            }
        });
    }
    std::optional<ModeLoadResult> Poll()
    {
        return job_.Poll();
    }
  private:
    ChartLoadJob<ModeLoadResult> job_;
};
