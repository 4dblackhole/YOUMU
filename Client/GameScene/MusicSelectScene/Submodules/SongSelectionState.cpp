#include "SongSelectionState.h"
#include "SongSelectionText.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <ranges>

using namespace song_select;

void SongSelectionState::RebuildVisibleSongs()
{
    std::optional<std::size_t> previousCatalogIndex;
    if (focusedSongPosition_ < visibleSongIndices_.size())
    {
        previousCatalogIndex = visibleSongIndices_[focusedSongPosition_];
    }

    const auto terms = SplitSearchTerms(searchText_);
    patternsFiltered_ = filter_.IsActive();
    if (patternsFiltered_)
    {
        filteredPatterns_.resize(catalog_.songs.size());
        for (auto &indices : filteredPatterns_) indices.clear();
    }
    else filteredPatterns_.clear();
    visibleSongIndices_.clear();
    for (std::size_t index = 0; index < catalog_.songs.size(); ++index)
    {
        if (MatchesSearch(catalog_.songs[index], terms))
        {
            if (patternsFiltered_)
            {
                const auto &patterns = catalog_.songs[index].patterns;
                for (std::size_t pattern = 0; pattern < patterns.size(); ++pattern)
                    if (filter_.Matches(patterns[pattern])) filteredPatterns_[index].push_back(pattern);
                if (filteredPatterns_[index].empty()) continue;
            }
            visibleSongIndices_.push_back(index);
        }
    }

    // Runtime difficulty is uncalculated. Preserve catalog order until its
    // calculation is defined rather than deriving it from note count or JudgeLevel.
    if (sortMode_ == SortMode::BaseBpm)
    {
        std::vector<double> keys(catalog_.songs.size(), std::numeric_limits<double>::infinity());
        for (const auto index : visibleSongIndices_)
        {
            const auto &patterns = catalog_.songs[index].patterns;
            if (!patterns.empty())
            {
                const double bpm = patterns.front().pattern.baseBpm;
                if (std::isfinite(bpm) && bpm > 0) keys[index] = bpm;
            }
        }
        std::stable_sort(visibleSongIndices_.begin(), visibleSongIndices_.end(),
                         [&keys](const auto left, const auto right) { return keys[left] < keys[right]; });
    }
    else if (sortMode_ != SortMode::Difficulty)
    {
        std::vector<std::wstring> keys(catalog_.songs.size());
        for (const auto index : visibleSongIndices_)
            keys[index] = Lowercase(sortMode_ == SortMode::Artist ? SongArtist(catalog_.songs[index])
                                                                : SongTitle(catalog_.songs[index]));
        std::stable_sort(visibleSongIndices_.begin(), visibleSongIndices_.end(),
            [&keys](const auto left, const auto right) { return keys[left] < keys[right]; });
    }

    focusedSongPosition_ = 0;
    if (previousCatalogIndex.has_value())
    {
        const auto found = std::ranges::find(visibleSongIndices_, *previousCatalogIndex);
        if (found != visibleSongIndices_.end())
        {
            focusedSongPosition_ =
                static_cast<std::size_t>(std::distance(visibleSongIndices_.begin(), found));
        }
    }
    const auto patterns = focusedSongPosition_ < visibleSongIndices_.size()
        ? VisiblePatternIndices(visibleSongIndices_[focusedSongPosition_]) : std::vector<std::size_t>{};
    selectedPatternIndex_ = patterns.empty() ? 0 : patterns.front();
}

void SongSelectionState::MoveSongFocus(const int delta)
{
    if (visibleSongIndices_.empty())
    {
        return;
    }
    const std::ptrdiff_t target =
        std::clamp<std::ptrdiff_t>(static_cast<std::ptrdiff_t>(focusedSongPosition_) + delta, 0,
                                   static_cast<std::ptrdiff_t>(visibleSongIndices_.size() - 1));
    SelectVisibleSong(static_cast<std::size_t>(target));
}

void SongSelectionState::MoveDifficultyFocus(const int delta)
{
    if (visibleSongIndices_.empty())
    {
        return;
    }
    const auto patterns = VisiblePatternIndices(visibleSongIndices_[focusedSongPosition_]);
    const auto found = std::ranges::find(patterns, selectedPatternIndex_);
    const auto position = found == patterns.end() ? 0 : static_cast<std::size_t>(found - patterns.begin());
    if (delta < 0)
    {
        if (!patterns.empty() && position > 0)
        {
            SelectPattern(patterns[position - 1]);
            return;
        }
        if (focusedSongPosition_ > 0)
        {
            SelectVisibleSong(focusedSongPosition_ - 1);
        }
        return;
    }
    if (!patterns.empty() && position + 1 < patterns.size())
    {
        SelectPattern(patterns[position + 1]);
        return;
    }
    if (focusedSongPosition_ + 1 < visibleSongIndices_.size())
    {
        SelectVisibleSong(focusedSongPosition_ + 1);
    }
}

void SongSelectionState::SelectVisibleSong(const std::size_t visiblePosition)
{
    if (visiblePosition >= visibleSongIndices_.size())
    {
        return;
    }
    focusedSongPosition_ = visiblePosition;
    const auto patterns = VisiblePatternIndices(visibleSongIndices_[focusedSongPosition_]);
    selectedPatternIndex_ = patterns.empty() ? 0 : patterns.front();
    ClearLaunchError();
}

void SongSelectionState::SelectPattern(std::size_t index)
{
    if (focusedSongPosition_ >= visibleSongIndices_.size())
        return;
    const auto &patterns = catalog_.songs[visibleSongIndices_[focusedSongPosition_]].patterns;
    if (index >= patterns.size())
        return;
    if (patternsFiltered_)
    {
        const auto visible = VisiblePatternIndices(visibleSongIndices_[focusedSongPosition_]);
        if (std::ranges::find(visible, index) == visible.end()) return;
    }
    selectedPatternIndex_ = index;
    ClearLaunchError();
}

std::size_t SongSelectionState::VisiblePatternCount(std::size_t catalogIndex) const noexcept
{
    if (catalogIndex >= catalog_.songs.size()) return 0;
    if (!patternsFiltered_) return catalog_.songs[catalogIndex].patterns.size();
    return catalogIndex < filteredPatterns_.size() ? filteredPatterns_[catalogIndex].size() : 0;
}

std::vector<std::size_t> SongSelectionState::VisiblePatternIndices(std::size_t catalogIndex) const
{
    if (catalogIndex >= catalog_.songs.size()) return {};
    if (patternsFiltered_)
        return catalogIndex < filteredPatterns_.size() ? filteredPatterns_[catalogIndex] : std::vector<std::size_t>{};
    std::vector<std::size_t> indices(catalog_.songs[catalogIndex].patterns.size());
    std::iota(indices.begin(), indices.end(), std::size_t{0});
    return indices;
}

void SongSelectionState::SetLaunchError(const LaunchErrorKind kind, std::string detail)
{
    launchErrorKind_ = kind;
    launchErrorDetail_ = std::move(detail);
}

void SongSelectionState::ClearLaunchError() noexcept
{
    launchErrorKind_ = LaunchErrorKind::None;
    launchErrorDetail_.clear();
}
