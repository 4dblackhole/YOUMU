#pragma once
#include "Mode/PlayOptions.h"
#include <cstddef>
#include <string>
#include <vector>

// A record row's play options, like osu!'s mod letters (user decision 2026-11-22): only what changes the
// play itself, each as two capitals. AC with the Accelerate multiplier (records keep a multiplier),
// JL for a judge level the player set, MR (Mirror), RD (Random), CS (scroll effects ignored, a constant
// speed) and SS with the scroll speed (`SS2x`, or `SS180` for a BPM). Metronome and pitch keeping do
// not change the play; the stroke count has its own icon on the row.
[[nodiscard]] std::vector<std::wstring> RecordOptionTokens(const PlayOptions &options);
// The first `shown` tokens joined with commas; "…" marks options left out for want of room.
[[nodiscard]] std::wstring JoinRecordOptions(const std::vector<std::wstring> &tokens, std::size_t shown);
