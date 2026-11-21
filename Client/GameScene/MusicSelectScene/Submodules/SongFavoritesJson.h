#pragma once
#include "SongFavorites.h"

// Private file-format boundary; no general JSON object model is exposed.

// One collection file:
// {"format": "YOUMU collection", "version": 1, "name": "...",
//  "charts": [{"sha256": "...", "title": "...", "artist": "...", "pattern": "..."}]}
// The label members of a chart are optional. The id of the result is 0.
[[nodiscard]] SongFavoriteFolder ReadSongCollectionJson(std::string_view source);
[[nodiscard]] std::string WriteSongCollectionJson(const SongFavoriteFolder &collection);

// favorites.json of earlier versions: folder names with lowercase absolute pattern paths (UTF-8).
struct LegacyFavoriteFolder
{
    std::wstring name;
    std::vector<std::string> patternPaths;
};
[[nodiscard]] std::vector<LegacyFavoriteFolder> ReadLegacyFavoritesJson(std::string_view source);

void ValidateSongCollection(const SongFavoriteFolder &collection);
void ValidateSongFavorites(const SongFavoritesValues &values);
