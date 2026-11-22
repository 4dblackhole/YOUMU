#pragma once
#include "Model/Submodules/MusicDocument.h"
#include "Model/Submodules/PatternDocument.h"
#include <filesystem>
#include <string>

// One difficulty as the converter left it: three files in one folder. Its YMM and YMP still
// name the audio and the background as the osu! file does, relative to the osu! song folder.
struct ConvertedOsuChart
{
    std::filesystem::path sourceFolder; // The osu! song folder that holds the audio and images.
    std::filesystem::path musicFile, patternFile, effectFile;
    MusicDocument music;
    PatternDocument pattern;
};

// Where a converted chart goes inside Songs. Working it out changes nothing on disk.
struct OsuPlacement
{
    std::filesystem::path songFolder, musicFile, patternFile, effectFile;
    std::filesystem::path audioName; // File name of the audio beside the YMM.
    // False when a song folder with byte-identical audio already exists (whatever its name): the chart
    // joins it and that song's YMM and audio stay untouched. A new song whose Artist - Title folder
    // already holds other music is named `Artist - Title [<first 8 digits of the audio's SHA-256>]`.
    bool newSong{};
    bool chartExists{};  // The chart set folder is already there; nothing would be written.
    std::string problem; // UTF-8. Not empty when the chart cannot be placed for another reason.
};

// The Client owns everything between the converter's three files and a playable song folder:
// names, reuse of identical audio, copying audio and background, and safe publication.
//
//   Songs/<Artist - Title>/Music/<Artist - Title>.<audio extension> and .ymm
//   (Songs/<Artist - Title [3fa9c1d2]>/Music/... when other audio already has the name)
//                          Chart/<Creator - Version>/<Artist - Title> [<Version>].ymp and .yme
//                          BG/<background file name>
[[nodiscard]] OsuPlacement PlanOsuPlacement(const std::filesystem::path &songsRoot, const ConvertedOsuChart &chart);

// Holds the Songs import lock, stages every file outside Songs, checks the staged files and
// publishes them. Existing files are never replaced and a failure removes only what this call
// created. Throws std::runtime_error with a UTF-8 message.
OsuPlacement PlaceConvertedOsuChart(const std::filesystem::path &songsRoot, const ConvertedOsuChart &chart);
