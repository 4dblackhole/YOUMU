#pragma once

#include "Texts/TextCatalog.h"

#include <array>
#include <string_view>

struct MusicSelectTextSet
{
    std::wstring_view personalRecord;
    std::wstring_view noRecord;
    std::wstring_view searchSongs;
    std::wstring_view songCountFormat;
    std::array<std::wstring_view, 4> sortModes;
    std::wstring_view back;
    std::wstring_view go;
    std::wstring_view noSongsAvailable;
    std::wstring_view noDifficulties;
    std::wstring_view catalogErrorPrefix;
    std::wstring_view unsupportedModePrefix;
    std::wstring_view patternErrorPrefix;
    std::wstring_view unknownChartError;
    std::wstring_view newDifficulty, convertOsu;
};

[[nodiscard]] MusicSelectTextSet MusicSelect(const TextCatalog &texts);
