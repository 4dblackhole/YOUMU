#include "GameplayTexts.h"

GameplayTextSet Gameplay(const TextCatalog &texts)
{
    return {texts.Get("Gameplay.audioInitializationFailed"),
            texts.Get("Gameplay.focusNone"),   texts.Get("Gameplay.focusPrefix"),
            texts.Get("Gameplay.lastFormat"),  texts.Get("Gameplay.lastNone"),
            texts.Get("Gameplay.early"),       texts.Get("Gameplay.late"),
            texts.Get("Gameplay.errorEffect"), texts.Get("Gameplay.errorAudio"),
            texts.Get("Gameplay.errorBackground")};
}
