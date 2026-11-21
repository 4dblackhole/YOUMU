#pragma once
#include "Records/PlayRecordStore.h"
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

// The judgement counts a record's saved detail holds, in the result screen's order, as
// (Judgements.* text key, count). Empty when the record keeps no detail, the detail cannot be
// read or its mode has no counts to show.
[[nodiscard]] std::vector<std::pair<std::string_view, std::size_t>> RecordJudgementCounts(const PlayRecord &record) noexcept;
