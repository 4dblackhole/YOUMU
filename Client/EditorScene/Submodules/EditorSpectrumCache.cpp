#include "EditorSpectrumCache.h"
#include <iterator>

std::shared_ptr<const SpectrumTile> EditorSpectrumCache::Find(const EditorSpectrumRequest &request)
{
    const auto found = entries_.find(request);
    if (found == entries_.end())
        return {};
    recent_.splice(recent_.begin(), recent_, found->second);
    return found->second->tile;
}

void EditorSpectrumCache::Store(const EditorSpectrumRequest &request, std::shared_ptr<const SpectrumTile> tile)
{
    if (!tile)
        return;
    if (const auto existing = entries_.find(request); existing != entries_.end())
        Remove(existing->second);
    // The levels dominate; the fixed part covers the tile, its list node and its map node.
    const std::size_t bytes = tile->levels.size() + sizeof(SpectrumTile) + sizeof(Entry) + sizeof(EditorSpectrumRequest);
    recent_.push_front({request, std::move(tile), bytes});
    entries_.emplace(request, recent_.begin());
    bytes_ += bytes;
    // The tile just stored always stays, even when it alone exceeds the limit.
    while (bytes_ > byteLimit_ && recent_.size() > 1)
        Remove(std::prev(recent_.end()));
}

void EditorSpectrumCache::Clear() noexcept
{
    entries_.clear();
    recent_.clear();
    bytes_ = 0;
}

void EditorSpectrumCache::ClearTrack(const std::size_t track) noexcept
{
    for (auto entry = recent_.begin(); entry != recent_.end();)
    {
        const auto next = std::next(entry);
        if (entry->request.track == track)
            Remove(entry);
        entry = next;
    }
}

void EditorSpectrumCache::Remove(const Order::iterator entry) noexcept
{
    bytes_ -= entry->bytes;
    entries_.erase(entry->request);
    recent_.erase(entry);
}
