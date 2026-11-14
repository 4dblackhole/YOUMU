#include "MusicSelectTexts.h"

MusicSelectTextSet MusicSelect(const TextCatalog &texts)
{
    return {texts.Get("MusicSelect.personalRecord"),
            texts.Get("MusicSelect.noRecord"),
            texts.Get("MusicSelect.searchSongs"),
            texts.Get("MusicSelect.songCountFormat"),
            {texts.Get("MusicSelect.sortModes.0"), texts.Get("MusicSelect.sortModes.1"),
             texts.Get("MusicSelect.sortModes.2"), texts.Get("MusicSelect.sortModes.3")},
            texts.Get("MusicSelect.back"),
            texts.Get("MusicSelect.go"),
            texts.Get("MusicSelect.noSongsAvailable"),
            texts.Get("MusicSelect.noDifficulties"),
            texts.Get("MusicSelect.catalogErrorPrefix"),
            texts.Get("MusicSelect.unsupportedModePrefix"),
            texts.Get("MusicSelect.patternErrorPrefix"),
            texts.Get("MusicSelect.unknownChartError"), texts.Get("MusicSelect.newDifficulty"),
            texts.Get("MusicSelect.convertOsu"), texts.Get("MusicSelect.importTitle"),
            texts.Get("MusicSelect.importComplete"), texts.Get("MusicSelect.osuFileFilter")};
}
