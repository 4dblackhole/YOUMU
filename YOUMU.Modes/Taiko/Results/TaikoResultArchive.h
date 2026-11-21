#pragma once
#include "TaikoPlayResult.h"
#include <memory>
#include <string>
#include <string_view>

// The detail a local record keeps beside its summary columns so the result screen can be shown
// again: judgement and tick counts, group accuracies, timing samples and statistics, HP gauges
// and curve, end reason, times and control markers. Per-note results, the input history and the
// key mapping stay out (a replay is not implemented). The text is versioned and ASCII.
[[nodiscard]] std::string SaveTaikoResultDetail(const TaikoPlayResult &result);
// Rebuilds a result from the summary its record columns restored and the saved detail.
// Throws std::runtime_error when the detail is damaged or from an unknown version.
[[nodiscard]] std::unique_ptr<TaikoPlayResult> LoadTaikoResultDetail(PlayResultSummary summary,
                                                                     std::string_view detail);
