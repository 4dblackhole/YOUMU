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
                if (ChartLoadStopped(stop))
                    return ModeLoadResult::Cancelled();
                auto result = CreatePlayGameMode(request.mode,
                    {request.CompiledOffset(), request.inputKeys,
                     request.playOptions})
                    ->LoadSession(request.patternPath, request.effectPath, stop);
                if (result.cancelled || ChartLoadStopped(stop))
                    return ModeLoadResult::Cancelled();
                if (result.Succeeded() && !result.session->CanEnterGameplay())
                {
                    result.session.reset();
                    result.diagnostics.push_back({DiagnosticSeverity::Error, {request.patternPath, 1, 1},
                        "A chart with no playable notes cannot be played."});
                }
                return result;
            }
            catch (const std::exception& error)
            {
                ModeLoadResult result;
                result.diagnostics.push_back({DiagnosticSeverity::Error, {request.patternPath, 1, 1}, error.what()});
                return result;
            }
        });
    }
    // A cancelled load has nothing to show and is dropped like one that has not finished.
    std::optional<ModeLoadResult> Poll()
    {
        auto result = job_.Poll();
        if (result && result->cancelled)
            return {};
        return result;
    }
  private:
    ChartLoadJob<ModeLoadResult> job_;
};
