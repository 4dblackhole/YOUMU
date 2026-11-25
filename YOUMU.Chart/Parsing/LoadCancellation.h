#pragma once

#include <stop_token>

// Cancelling a chart or catalog load is an expected outcome, not an error, so nothing throws for it. The code
// that runs a load checks this between its stages (one file, one chart, one build step) and returns a result
// marked as cancelled; a cancelled load never becomes a chart diagnostic and nothing of it is published.
[[nodiscard]] inline bool ChartLoadStopped(const std::stop_token& stop) noexcept
{
    return stop.stop_requested();
}
