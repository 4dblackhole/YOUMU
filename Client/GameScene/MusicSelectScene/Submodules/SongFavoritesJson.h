#pragma once
#include "SongFavorites.h"

// Private file-format boundary; no general JSON object model is exposed.
[[nodiscard]] SongFavoritesValues ReadSongFavoritesJson(std::string_view source);
[[nodiscard]] std::string WriteSongFavoritesJson(const SongFavoritesValues &values);
void ValidateSongFavorites(const SongFavoritesValues &values);
