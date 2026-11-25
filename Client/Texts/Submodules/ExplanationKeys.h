#pragma once
#include <array>
#include <string_view>

// Captions, buttons, units and value formats default to English. These keys
// are explanatory copy; their selected-language translations remain independent.
// The pause menu's Resume and Rewind values follow the selected language by the user's decision.
inline constexpr auto ExplanationTextKeys = std::to_array<std::string_view>({
    "Editor.selectionHelp",
    "Editor.offsetAudioHelp", "Editor.finishLongNote", "Editor.timingHelp", "Editor.metadataHelp",
    "Editor.audioHelp", "Editor.analyzingAudio", "Editor.excludeHelp", "Editor.delayCutFormat", "Editor.selectedCountFormat", "Editor.curveValid", "Editor.curveNameError", "Editor.curvePositionFormat", "Editor.audioSelectionHelp", "Editor.discardChanges", "Editor.saveFailedMessage", "Editor.pasteFailedMessage", "Editor.savedFormat",
    "TaikoEditor.effectsHelp", "TaikoEditor.selectNoteHelp",
    "MusicSelect.createDifficultyError", "MusicSelect.createDifficultyExists",
    "MusicSelect.createDifficultyNameError", "MusicSelect.importFailedMessage", "MusicSelect.importPartialMessage", "MusicSelect.noSongsAvailable", "MusicSelect.noDifficulties", "MusicSelect.catalogErrorPrefix", "MusicSelect.unsupportedModePrefix",
    "MusicSelect.patternErrorPrefix", "MusicSelect.unknownChartError",
    "Result.pauseUsed", "Result.rewindUsed", "Result.noHpData", "Result.noTimingData",
    "Gameplay.audioInitializationFailed", "Pause.preparing", "Pause.retryDescription", "Pause.retireDescription",
    "Pause.resumeValueFormat", "Pause.rewindValueFormat",
    "Options.windowError", "Options.inputError", "Options.noDrivers",
    "Options.saveError", "Options.audioError", "Options.outputFallback", "Options.driverUnavailable", "Options.loadWarning", "Options.bufferRestart",
    "Startup.decodeError", "Startup.logNote",
    "Records.loading", "Records.saving", "Records.saved", "Records.saveError", "Records.saveFailedMessage", "Records.openFailedMessage", "Records.loadError",
    "SongFilter.invalidNotes", "SongFilter.invalidBpm", "SongFilter.invalidJudgeLevel",
    "SongFilter.invalidDifficulty", "SongFilter.invalidHp", "SongFilter.invalidLength", "SongFilter.reversedRange",
    "Favorites.noPattern", "Favorites.hint", "Favorites.empty", "Favorites.error", "RecordFilter.error",
    "OsuImport.hint", "OsuImport.none", "OsuImport.exists", "OsuImport.failed",
    "SongMenu.deleteDifficultyMessage", "SongMenu.deleteSongMessage", "SongMenu.deleteFailedMessage",
    "SongMenu.openFolderFailedMessage",
    "PlayOptions.invalidValue"
});
