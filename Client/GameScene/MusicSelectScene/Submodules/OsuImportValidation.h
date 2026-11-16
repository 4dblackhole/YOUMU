#pragma once
#include "OsuSongPlacement.h"
#include <filesystem>
#include <string_view>

// The Client owns playability checks of what the converter wrote. `output` is the converter's
// JSON v1 result and `outputFolder` the folder it was told to write into. Returns the parsed
// chart (its sourceFolder is left empty for the caller). Throws on a malformed result, a file
// outside the folder, inconsistent file references or lost notes.
ConvertedOsuChart ValidateConvertedOsuResult(std::string_view output, const std::filesystem::path &outputFolder);
