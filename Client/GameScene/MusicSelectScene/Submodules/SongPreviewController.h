#pragma once
#include "PhantomCore.h"
#include "SongSelectionState.h"
#include <array>
#include <optional>

class SongPreviewController final
{
    struct PreviewSlot
    {
        std::shared_ptr<PhantomCore::AudioClip> clip;
        PhantomCore::AudioPlaybackId playbackId{PhantomCore::InvalidAudioPlaybackId};
        std::optional<std::size_t> catalogIndex;
        std::optional<std::uint64_t> durationMicroseconds;
        float volume{};
        float targetVolume{};
    };

  public:
    explicit SongPreviewController(PhantomCore::AudioPlaybackManager &playback)
        : audioPlayback_(playback)
    {
    }
    ~SongPreviewController()
    {
        StopPreviewAudio();
    }
    void Initialize(PhantomCore::AudioSystem &audio)
    {
        audioSystem_ = &audio;
    }
    void SyncPreviewToFocusedSong(const SongSelectionState &selection, bool active);
    void StartSongPreview(const SongSelectionState &selection, const std::size_t catalogIndex);
    void UpdatePreviewAudio(const double deltaSeconds);
    void StopPreviewAudio() noexcept;
    [[nodiscard]] std::optional<std::uint64_t> DurationFor(std::size_t catalogIndex) const;
    void StopPreviewSlot(PreviewSlot &slot) noexcept;

  private:
    void PollRetiredClips();
    void PlayPendingPreview(std::size_t slotIndex);
    PhantomCore::AudioPlaybackManager &audioPlayback_;
    PhantomCore::AudioSystem *audioSystem_{};
    std::array<PreviewSlot, 2> previewSlots_{};
    std::optional<std::size_t> currentPreviewSlot_;
    // A failed file is retried on another selection/scene entry, never every Update.
    std::optional<std::size_t> failedPreviewIndex_;
    std::optional<std::size_t> pendingPreviewSlot_;
    std::vector<std::shared_ptr<PhantomCore::AudioClip>> retiredClips_;
};
