#pragma once
#include <array>
#include <string_view>

// Captions, buttons, units and value formats default to English. These keys
// are explanatory copy; their selected-language translations remain independent.
inline constexpr auto ExplanationTextKeys = std::to_array<std::string_view>({
    "Editor.testControls", "Editor.testPaused", "Editor.saveBeforeTest", "Editor.selectionHelp",
    "Editor.offsetAudioHelp", "Editor.finishLongNote", "Editor.timingHelp", "Editor.metadataHelp",
    "Editor.audioHelp", "Editor.analyzingAudio", "Editor.audioSelectionHelp", "Editor.discardChanges", "Editor.savedFormat",
    "TaikoEditor.effectsHelp", "TaikoEditor.selectNoteHelp",
    "MusicSelect.newDifficultyDefaults", "MusicSelect.createDifficultyError", "MusicSelect.createDifficultyExists",
    "MusicSelect.createDifficultyNameError", "MusicSelect.importComplete", "MusicSelect.noRecords", "MusicSelect.noImage",
    "MusicSelect.noSongSelected", "MusicSelect.browserHint", "MusicSelect.noSongsAvailable", "MusicSelect.noDifficulties",
    "MusicSelect.noDifficulty", "MusicSelect.catalogErrorPrefix", "MusicSelect.unsupportedModePrefix",
    "MusicSelect.patternErrorPrefix", "MusicSelect.unknownChartError", "MusicSelect.loadingSongs",
    "Result.pauseUsed", "Result.rewindUsed", "Result.noHpData", "Result.noTimingData",
    "Gameplay.errorPrefix", "Gameplay.audioInitializationFailed", "Pause.preparing",
    "Options.offsetHint", "Options.windowError", "Options.inputError", "Options.noDrivers",
    "Options.saveError", "Options.audioError", "Options.outputFallback", "Options.driverUnavailable", "Options.loadWarning",
    "Performance.measuringFps", "Performance.measuringUps", "Performance.measuringDraws", "Startup.decodeError",
    "Records.loading", "Records.saving", "Records.saved", "Records.saveError", "Records.loadError",
    "SongFilter.hint", "SongFilter.invalidNotes", "SongFilter.invalidBpm", "SongFilter.invalidJudgeLevel",
    "SongFilter.invalidDifficulty", "SongFilter.reversedRange",
    "Favorites.noPattern", "Favorites.hint", "Favorites.empty", "Favorites.error", "RecordFilter.hint", "RecordFilter.error",
    "PlayOptions.closeHint", "PlayOptions.invalidValue"
});
