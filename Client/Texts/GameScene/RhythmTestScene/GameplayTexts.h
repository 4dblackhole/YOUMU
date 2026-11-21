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
    // Mean timing error under the guide: unit after the number, and the value
    // shown before the attempt has a graded hit.
    std::wstring_view averageErrorUnit;
    std::wstring_view averageErrorEmpty;
    // Kind captions of the screen error line.
    std::wstring_view errorEffect;
    std::wstring_view errorAudio;
    std::wstring_view errorBackground;
    // Notice after a +/- local offset change; {} is the signed millisecond value.
    std::wstring_view localOffsetFormat;
};

[[nodiscard]] GameplayTextSet Gameplay(const TextCatalog &texts);
