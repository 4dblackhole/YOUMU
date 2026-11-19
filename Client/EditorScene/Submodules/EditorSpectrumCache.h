#pragma once
#include "Audio/EditorAudioAnalysis.h"
#include <cstddef>
#include <list>
#include <map>
#include <memory>

struct EditorSpectrumRequest
{
    std::size_t track{};
    SpectrumRegion region;
    auto operator<=>(const EditorSpectrumRequest &) const = default;
};

// Finished spectrum tiles of every visible range and position the editor has shown, so returning
// to one does not analyse it again. Once the byte limit is passed, the tiles shown longest ago leave.
class EditorSpectrumCache final
{
  public:
    static constexpr std::size_t DefaultByteLimit = std::size_t{32} * 1024 * 1024;
    explicit EditorSpectrumCache(std::size_t byteLimit = DefaultByteLimit) noexcept : byteLimit_(byteLimit) {}
    // Returns the finished tile and marks it as the most recently shown.
    [[nodiscard]] std::shared_ptr<const SpectrumTile> Find(const EditorSpectrumRequest &);
    [[nodiscard]] bool Contains(const EditorSpectrumRequest &request) const { return entries_.contains(request); }
    void Store(const EditorSpectrumRequest &, std::shared_ptr<const SpectrumTile>);
    void Clear() noexcept;
    // Drops one track's tiles: expected hits depend on the chart, the music does not.
    void ClearTrack(std::size_t track) noexcept;
    [[nodiscard]] std::size_t Bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::size_t Count() const noexcept { return entries_.size(); }

  private:
    struct Entry
    {
        EditorSpectrumRequest request;
        std::shared_ptr<const SpectrumTile> tile;
        std::size_t bytes{};
    };
    using Order = std::list<Entry>;
    void Remove(Order::iterator entry) noexcept;
    Order recent_; // the front was shown most recently
    std::map<EditorSpectrumRequest, Order::iterator> entries_;
    std::size_t byteLimit_{};
    std::size_t bytes_{};
};
