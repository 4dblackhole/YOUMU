#pragma once
#include "PhantomCore.h"
#include "MusicPlayerState.h"
#include <array>
#include <filesystem>
#include <optional>
#include <vector>

// Plays the one song the title player and song select share, identified by its
// audio path so a refreshed catalog never interrupts it. Two slots cross-fade
// between songs; MusicPlayerState carries the transport the voice mirrors.
class SongPreviewController final
{
    struct PreviewSlot
    {
        std::shared_ptr<PhantomCore::AudioClip> clip;
        PhantomCore::AudioPlaybackId playbackId{PhantomCore::InvalidAudioPlaybackId};
        std::filesystem::path audioPath;
        float volume{};
        float targetVolume{};
        // A voice started or restarted away from the file start stays paused until its seek is prepared.
        bool awaitingPreparation{};
    };

  public:
    enum class EndBehavior : std::uint8_t
    {
        Repeat,
        Report,
    };

    explicit SongPreviewController(PhantomCore::AudioPlaybackManager &playback)
        : audioPlayback_(playback)
    {
    }
    ~SongPreviewController()
    {
        StopPreviewAudio();
    }
    SongPreviewController(const SongPreviewController &) = delete;
    SongPreviewController &operator=(const SongPreviewController &) = delete;

    void Initialize(PhantomCore::AudioSystem &audio)
    {
        audioSystem_ = &audio;
    }
    // Makes audioPath the current song. A different song starts from its beginning;
    // the same song keeps its transport, and resume plays it if it was paused or stopped.
    void SyncTrack(const std::filesystem::path &audioPath, bool resume);
    // Starts the current song again from the beginning, even when it is the same file.
    void RestartTrack();
    void Play();
    void Pause();
    void TogglePlay();
    void Stop();
    void Seek(std::uint64_t positionMicroseconds);
    // The player's music volume; voices follow it on the next update, fades included.
    void SetVolumeScale(float scale) noexcept;
    // Advances fades and the transport. Returns true once when the current song played to its end
    // under EndBehavior::Report; Repeat starts it again instead.
    bool UpdatePreviewAudio(double deltaSeconds, EndBehavior endBehavior);
    // Releases every voice but remembers the song, stopped at its beginning.
    void ReleaseAudio() noexcept;
    // Releases every voice and forgets the song.
    void StopPreviewAudio() noexcept;
    // Hands clips that are still busy to the audio system before it shuts down.
    void Shutdown() noexcept;

    [[nodiscard]] const std::filesystem::path &CurrentTrack() const noexcept { return trackPath_; }
    [[nodiscard]] const MusicPlayerState &State() const noexcept { return transport_; }
    [[nodiscard]] bool TrackFailed() const noexcept { return trackFailed_; }

  private:
    void BeginTrack(const std::filesystem::path &audioPath);
    void LoadTrack();
    void PollPendingTrack();
    void PlayPendingPreview(std::size_t slotIndex);
    [[nodiscard]] bool RestartVoice(PreviewSlot &slot);
    [[nodiscard]] bool UpdatePreparation(PreviewSlot &slot);
    void FadeOutCurrent() noexcept;
    void ReleaseSlots() noexcept;
    void StopPreviewSlot(PreviewSlot &slot) noexcept;
    void PollRetiredClips();
    [[nodiscard]] PreviewSlot *CurrentSlot() noexcept;

    PhantomCore::AudioPlaybackManager &audioPlayback_;
    PhantomCore::AudioSystem *audioSystem_{};
    std::array<PreviewSlot, 2> previewSlots_{};
    std::optional<std::size_t> currentPreviewSlot_;
    std::optional<std::size_t> pendingPreviewSlot_;
    std::filesystem::path trackPath_;
    MusicPlayerState transport_;
    float volumeScale_{1.0F}, appliedVolumeScale_{1.0F};
    // A failed file is retried on Play, another song or a scene entry, never every Update.
    bool trackFailed_{};
    std::vector<std::shared_ptr<PhantomCore::AudioClip>> retiredClips_;
};
