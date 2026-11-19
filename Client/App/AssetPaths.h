#pragma once

#include "YOUMUAssets.h"
#include "App/SkinSetSelection.h"

#include <filesystem>

// Runtime asset paths are centralized here so source code does not duplicate
// directory strings when the asset layout changes.

[[nodiscard]] inline std::filesystem::path UserSongs()
{
    return UserSongsRoot();
}

[[nodiscard]] inline std::filesystem::path EditorSound(const std::filesystem::path &file)
{
    return SkinSetSelection::Instance().Resolve(std::filesystem::path(L"Editor") / file);
}
[[nodiscard]] inline std::filesystem::path Result(const std::filesystem::path &file)
{
    return SkinSetSelection::Instance().Image("Result", file.generic_string());
}
[[nodiscard]] inline std::filesystem::path Loading(const std::filesystem::path &file)
{
    return SkinSetSelection::Instance().Image("Loading", file.generic_string());
}
// Shared parts such as the white line icons under [Common] Icons.<name>.
[[nodiscard]] inline std::filesystem::path Common(const std::filesystem::path &file)
{
    return SkinSetSelection::Instance().Image("Common", file.generic_string());
}
[[nodiscard]] inline std::filesystem::path InGame(
    const std::filesystem::path& file)
{
    return SkinSetSelection::Instance().Image("InGame", file.generic_string());
}

// Built-in hit sounds live in one skin folder per mode under HitSounds.
[[nodiscard]] inline std::filesystem::path ModeHitSound(
    const std::wstring_view modeFolder,
    const std::filesystem::path& file)
{
    return SkinSetSelection::Instance().Resolve(
        std::filesystem::path(L"HitSounds") / modeFolder / file);
}
