#pragma once

#include "Texts/TextCatalog.h"

#include <string_view>

struct GameplayTextSet
{
    std::wstring_view audioInitializationFailed;
    std::wstring_view focusNone;
    std::wstring_view focusPrefix;
    std::wstring_view lastFormat;
    std::wstring_view lastNone;
    std::wstring_view early;
    std::wstring_view late;
    // Kind captions of the screen error line.
    std::wstring_view errorEffect;
    std::wstring_view errorAudio;
    std::wstring_view errorBackground;
};

[[nodiscard]] GameplayTextSet Gameplay(const TextCatalog &texts);
