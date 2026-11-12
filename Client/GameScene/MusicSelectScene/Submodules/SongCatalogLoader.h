#pragma once
#include "Catalog/SongCatalog.h"
#include "GameFlow/ChartLoadJob.h"
#include "Mode/PlayGameModeFactory.h"
#include <atomic>
#include <cstdlib>
#include <cwctype>
#include <memory>
#include <mutex>

// KeepAlive retains copied-data jobs across play/editor transitions. A new root
// cancels the old generation; only the current generation can publish progress.
class SongCatalogLoader final
{
public:
    bool Refresh(const std::filesystem::path& root)
    {
        if (job_.Busy() && loadingRoot_ == root) return false;
        if (loadingRoot_ != root)
        {
            job_.Cancel();
            cache_.reset();
        }
        if (!cache_)
            cache_ = std::make_shared<SongCatalog>(
                SongCatalog::MetadataIndex(CachePath(root), CountPlayableNotes));
        loadingRoot_ = root;
        progress_ = std::make_shared<Progress>();
        const auto cache = cache_;
        const auto progress = progress_;
        job_ = StartChartLoad([cache, progress, root](std::stop_token stop)
        {
            return cache->Load(root, stop, [progress, stop](SongCatalogEntry entry)
            {
                CheckChartLoadCancellation(stop);
                {
                    std::scoped_lock lock(progress->mutex);
                    progress->entries.push_back(std::move(entry));
                }
                progress->pending.store(true, std::memory_order_release);
            });
        });
        return true;
    }
    void Cancel() noexcept
    {
        job_.Cancel();
        cache_.reset();
        progress_.reset();
        loadingRoot_.clear();
    }
    std::vector<SongCatalogEntry> PollProgress()
    {
        if (!progress_ || !progress_->pending.exchange(false, std::memory_order_acq_rel))
            return {};
        std::vector<SongCatalogEntry> entries;
        std::scoped_lock lock(progress_->mutex);
        entries.swap(progress_->entries);
        return entries;
    }
    std::optional<SongCatalogLoadResult> Poll()
    {
        try { return job_.Poll(); }
        catch (const ChartLoadCancelled&) { return {}; }
        catch (const std::exception& error)
        {
            SongCatalogLoadResult result;
            result.diagnostics.push_back({DiagnosticSeverity::Error, {}, error.what()});
            return result;
        }
    }

private:
    struct Progress
    {
        std::atomic<bool> pending{};
        std::mutex mutex;
        std::vector<SongCatalogEntry> entries;
    };
    static std::filesystem::path CachePath(const std::filesystem::path &root)
    {
        wchar_t *directory{};
        std::size_t count{};
        if (_wdupenv_s(&directory, &count, L"LOCALAPPDATA") != 0 || !directory)
            return {}; // Read-only in-memory index if no user cache directory is available.
        const std::unique_ptr<wchar_t, decltype(&std::free)> owned(directory, &std::free);
        std::uint64_t hash = 14695981039346656037ULL;
        for (const auto character : root.lexically_normal().generic_wstring())
            hash = (hash ^ static_cast<std::uint64_t>(std::towlower(character))) * 1099511628211ULL;
        return std::filesystem::path{directory} / L"YOUMU" / L"SongCatalog" / (std::to_string(hash) + ".bin");
    }
    std::shared_ptr<SongCatalog> cache_;
    std::shared_ptr<Progress> progress_;
    ChartLoadJob<SongCatalogLoadResult> job_;
    std::filesystem::path loadingRoot_;
};
