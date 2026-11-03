#pragma once
#include "Catalog/SongCatalog.h"
#include "SongFilter.h"
#include "Mode/PlayOptions.h"
#include <cstdint>
#include <string>
#include <vector>

class SongSelectionState final
{
  public:
    enum class SortMode : std::size_t
    {
        Difficulty,
        Title,
        Artist,
        BaseBpm,
    };

    enum class LaunchErrorKind : std::uint8_t
    {
        None,
        Catalog,
        UnsupportedMode,
        Pattern,
    };

    PlayOptions playOptions_;
    std::string selectedMode_{"Taiko"};
    SongCatalogLoadResult catalog_;
    bool catalogLoading_{};
    std::optional<std::uint64_t> selectedDurationMicroseconds_;
    std::vector<std::size_t> visibleSongIndices_;
    std::size_t focusedSongPosition_{};
    std::size_t selectedPatternIndex_{};
    SortMode sortMode_{SortMode::Difficulty};
    std::wstring searchText_;
    SongFilter filter_;
    LaunchErrorKind launchErrorKind_{LaunchErrorKind::None};
    std::string launchErrorDetail_;

    void RebuildVisibleSongs();
    void MoveSongFocus(const int delta);
    void MoveDifficultyFocus(const int delta);
    void SelectVisibleSong(const std::size_t visiblePosition);
    void SelectPattern(std::size_t index);
    void SetLaunchError(LaunchErrorKind kind, std::string detail);
    void ClearLaunchError() noexcept;
    [[nodiscard]] std::vector<std::size_t> VisiblePatternIndices(std::size_t catalogIndex) const;
    [[nodiscard]] std::size_t VisiblePatternCount(std::size_t catalogIndex) const noexcept;
  private:
    bool patternsFiltered_{};
    std::vector<std::vector<std::size_t>> filteredPatterns_;
};
