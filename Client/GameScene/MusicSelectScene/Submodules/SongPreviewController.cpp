#include "SongPreviewController.h"
#include <algorithm>
using namespace PhantomCore;

namespace
{
    constexpr double PreviewFadeSeconds = 0.2;
}

void SongPreviewController::SyncTrack(const std::filesystem::path &audioPath, const bool resume)
{
    if (audioSystem_ == nullptr)
        return;
    if (audioPath != trackPath_)
    {
        BeginTrack(audioPath);
        return;
    }
    // Song select keeps its song audible, but never retries a file that already failed every frame.
    const bool idle = !transport_.IsPlaying() && !trackFailed_;
    if (resume && idle)
        Play();
}

void SongPreviewController::BeginTrack(const std::filesystem::path &audioPath)
{
    FadeOutCurrent();
    trackPath_ = audioPath;
    trackFailed_ = false;
    if (trackPath_.empty())
    {
        if (pendingPreviewSlot_)
            StopPreviewSlot(previewSlots_[*pendingPreviewSlot_]);
        pendingPreviewSlot_.reset();
        transport_ = {};
        return;
    }
    transport_.BeginTrack();
    LoadTrack();
}

void SongPreviewController::RestartTrack()
{
    if (trackPath_.empty() || audioSystem_ == nullptr)
        return;
    trackFailed_ = false;
    if (auto *slot = CurrentSlot())
    {
        // The open voice rewinds in place, so the known length stays.
        transport_.Stop();
        transport_.Play();
        if (RestartVoice(*slot))
            return;
        StopPreviewSlot(*slot);
        currentPreviewSlot_.reset();
    }
    transport_.BeginTrack();
    LoadTrack();
}

void SongPreviewController::LoadTrack()
{
    if (pendingPreviewSlot_)
    {
        StopPreviewSlot(previewSlots_[*pendingPreviewSlot_]);
        pendingPreviewSlot_.reset();
    }
    // The other slot may still be fading the previous song out; it yields to the new song.
    const std::size_t slotIndex =
        currentPreviewSlot_.has_value()
            ? 1U - *currentPreviewSlot_
            : (previewSlots_[0].playbackId == InvalidAudioPlaybackId && !previewSlots_[0].clip ? 0U : 1U);
    PreviewSlot &slot = previewSlots_[slotIndex];
    StopPreviewSlot(slot);

    std::string errorMessage;
    std::unique_ptr<AudioClip> loadedClip =
        audioSystem_->LoadSound(trackPath_, AudioLoadMode::StreamAsync, errorMessage);
    if (loadedClip == nullptr)
    {
        trackFailed_ = true;
        transport_.Stop();
        return;
    }
    slot.clip = std::shared_ptr<AudioClip>(std::move(loadedClip));
    slot.audioPath = trackPath_;
    pendingPreviewSlot_ = slotIndex;
}

void SongPreviewController::PollPendingTrack()
{
    if (!pendingPreviewSlot_)
        return;
    auto &pending = previewSlots_[*pendingPreviewSlot_];
    std::string error;
    const auto state = pending.clip->LoadState(error);
    if (state == AudioClipLoadState::Loading)
        return;
    const auto slotIndex = *pendingPreviewSlot_;
    pendingPreviewSlot_.reset();
    const bool current = pending.audioPath == trackPath_;
    if (current && state == AudioClipLoadState::Ready)
    {
        std::uint64_t duration{};
        if (pending.clip->GetDurationMicroseconds(duration, error))
            transport_.SetDuration(duration);
        PlayPendingPreview(slotIndex);
        return;
    }
    if (current)
    {
        trackFailed_ = true;
        transport_.Stop();
    }
    StopPreviewSlot(pending);
}

void SongPreviewController::PlayPendingPreview(const std::size_t slotIndex)
{
    auto &slot = previewSlots_[slotIndex];
    // A song paused or seeked while it opened starts where the transport says.
    const bool seeks = transport_.PositionMicroseconds() > 0;
    AudioPlaybackSettings settings;
    settings.volume = 0.0F;
    settings.startPositionMicroseconds = transport_.PositionMicroseconds();
    settings.startPaused = seeks || !transport_.IsPlaying();
    std::string errorMessage;
    const AudioPlaybackId playbackId = audioPlayback_.Play(slot.clip, settings, nullptr, errorMessage);
    if (playbackId == InvalidAudioPlaybackId)
    {
        trackFailed_ = true;
        transport_.Stop();
        StopPreviewSlot(slot);
        return;
    }
    slot.playbackId = playbackId;
    slot.volume = 0.0F;
    slot.targetVolume = 1.0F;
    slot.awaitingPreparation = seeks;
    currentPreviewSlot_ = slotIndex;
}

bool SongPreviewController::RestartVoice(PreviewSlot &slot)
{
    // Rewinding or seeking reuses the channel. It resumes once the stream has reached the new position.
    AudioPlaybackSettings settings;
    settings.volume = slot.volume * volumeScale_;
    settings.startPositionMicroseconds = transport_.PositionMicroseconds();
    settings.startPaused = true;
    std::string errorMessage;
    if (!audioPlayback_.Restart(slot.playbackId, settings, nullptr, errorMessage))
        return false;
    slot.awaitingPreparation = true;
    return true;
}

bool SongPreviewController::UpdatePreparation(PreviewSlot &slot)
{
    AudioVoice *const voice = audioPlayback_.FindVoice(slot.playbackId);
    if (voice == nullptr)
        return true;
    std::string error;
    const auto state = voice->PreparationState(error);
    if (state == AudioPreparationState::Loading)
        return true;
    slot.awaitingPreparation = false;
    if (state == AudioPreparationState::Failed)
        return false;
    const bool current = currentPreviewSlot_ && &previewSlots_[*currentPreviewSlot_] == &slot;
    if (current && transport_.IsPlaying())
        static_cast<void>(voice->SetPaused(false, error));
    return true;
}

void SongPreviewController::SetVolumeScale(const float scale) noexcept
{
    volumeScale_ = std::max(scale, 0.0F);
}

void SongPreviewController::Play()
{
    if (trackPath_.empty() || audioSystem_ == nullptr)
        return;
    transport_.Play();
    if (auto *slot = CurrentSlot())
    {
        if (slot->awaitingPreparation)
            return;
        std::string error;
        AudioVoice *const voice = audioPlayback_.FindVoice(slot->playbackId);
        if (voice != nullptr && voice->SetPaused(false, error))
            return;
        StopPreviewSlot(*slot);
        currentPreviewSlot_.reset();
    }
    if (pendingPreviewSlot_)
        return;
    // Released or failed audio opens again from the remembered position.
    trackFailed_ = false;
    LoadTrack();
}

void SongPreviewController::Pause()
{
    transport_.Pause();
    if (auto *slot = CurrentSlot())
    {
        std::string error;
        if (AudioVoice *const voice = audioPlayback_.FindVoice(slot->playbackId))
            static_cast<void>(voice->SetPaused(true, error));
    }
}

void SongPreviewController::TogglePlay()
{
    if (transport_.IsPlaying())
        Pause();
    else
        Play();
}

void SongPreviewController::Stop()
{
    if (trackPath_.empty())
        return;
    transport_.Stop();
    auto *slot = CurrentSlot();
    if (slot == nullptr)
        return;
    std::string error;
    if (AudioVoice *const voice = audioPlayback_.FindVoice(slot->playbackId))
        static_cast<void>(voice->SetPaused(true, error));
    if (RestartVoice(*slot))
        return;
    StopPreviewSlot(*slot);
    currentPreviewSlot_.reset();
}

void SongPreviewController::Seek(const std::uint64_t positionMicroseconds)
{
    if (trackPath_.empty())
        return;
    transport_.Seek(positionMicroseconds);
    auto *slot = CurrentSlot();
    if (slot == nullptr || RestartVoice(*slot))
        return;
    StopPreviewSlot(*slot);
    currentPreviewSlot_.reset();
    if (transport_.IsPlaying() && !pendingPreviewSlot_)
        LoadTrack();
}

bool SongPreviewController::UpdatePreviewAudio(const double deltaSeconds, const EndBehavior endBehavior)
{
    PollRetiredClips();
    PollPendingTrack();
    const float fadeStep = static_cast<float>(std::max(deltaSeconds, 0.0) / PreviewFadeSeconds);
    const bool scaleChanged = volumeScale_ != appliedVolumeScale_;
    appliedVolumeScale_ = volumeScale_;

    bool currentEnded = false;
    for (std::size_t index = 0; index < previewSlots_.size(); ++index)
    {
        PreviewSlot &slot = previewSlots_[index];
        if (slot.playbackId == InvalidAudioPlaybackId)
            continue;
        const bool current = currentPreviewSlot_ == index;
        if (slot.awaitingPreparation && !UpdatePreparation(slot))
        {
            // A seek the stream could not reach fails the song like a file that would not open.
            StopPreviewSlot(slot);
            if (current)
            {
                currentPreviewSlot_.reset();
                trackFailed_ = true;
                transport_.Stop();
            }
            continue;
        }
        AudioVoice *const voice = audioPlayback_.FindVoice(slot.playbackId);
        if (voice == nullptr)
        {
            // The manager drops a voice once its stream has played out.
            StopPreviewSlot(slot);
            if (current)
            {
                currentPreviewSlot_.reset();
                currentEnded = true;
            }
            continue;
        }

        const float previousVolume = slot.volume;
        if (slot.volume < slot.targetVolume)
            slot.volume = std::min(slot.volume + fadeStep, slot.targetVolume);
        else if (slot.volume > slot.targetVolume)
            slot.volume = std::max(slot.volume - fadeStep, slot.targetVolume);

        std::string errorMessage;
        const bool volumeChanged = scaleChanged || slot.volume != previousVolume;
        const bool volumeFailed = volumeChanged && !voice->SetVolume(slot.volume * volumeScale_, errorMessage);
        const bool fadedOut = slot.targetVolume <= 0.0F && slot.volume <= 0.0F;
        if (volumeFailed || fadedOut)
        {
            StopPreviewSlot(slot);
            if (current)
                currentPreviewSlot_.reset();
        }
    }

    if (currentEnded)
    {
        // Only a playing song reaches its end; a paused voice the mixer dropped reopens on Play.
        if (!transport_.IsPlaying())
            return false;
        if (endBehavior == EndBehavior::Repeat)
        {
            transport_.BeginTrack();
            LoadTrack();
            return false;
        }
        transport_.Stop();
        return true;
    }
    const auto *slot = CurrentSlot();
    if (slot != nullptr && !slot->awaitingPreparation)
        transport_.Advance(deltaSeconds);
    return false;
}

void SongPreviewController::FadeOutCurrent() noexcept
{
    if (currentPreviewSlot_.has_value())
    {
        previewSlots_[*currentPreviewSlot_].targetVolume = 0.0F;
        currentPreviewSlot_.reset();
    }
}

SongPreviewController::PreviewSlot *SongPreviewController::CurrentSlot() noexcept
{
    if (!currentPreviewSlot_)
        return nullptr;
    auto &slot = previewSlots_[*currentPreviewSlot_];
    return slot.playbackId == InvalidAudioPlaybackId ? nullptr : &slot;
}

void SongPreviewController::ReleaseAudio() noexcept
{
    ReleaseSlots();
    transport_.Stop();
    trackFailed_ = false;
}

void SongPreviewController::StopPreviewAudio() noexcept
{
    ReleaseSlots();
    trackPath_.clear();
    transport_ = {};
    trackFailed_ = false;
}

void SongPreviewController::Shutdown() noexcept
{
    StopPreviewAudio();
    if (audioSystem_ == nullptr)
        return;
    try
    {
        for (auto &clip : retiredClips_)
            audioSystem_->RetireClip(std::move(clip));
        retiredClips_.clear();
    }
    catch (...)
    {
    }
}

void SongPreviewController::ReleaseSlots() noexcept
{
    for (PreviewSlot &slot : previewSlots_)
        StopPreviewSlot(slot);
    currentPreviewSlot_.reset();
    pendingPreviewSlot_.reset();
}

void SongPreviewController::StopPreviewSlot(PreviewSlot &slot) noexcept
{
    if (slot.playbackId != InvalidAudioPlaybackId)
    {
        try
        {
            std::string ignoredError;
            static_cast<void>(audioPlayback_.Stop(slot.playbackId, ignoredError));
        }
        catch (...)
        {
        }
    }
    // Native async loading and recently stopped streams may still be busy.
    // Keep their handles until polling reports a safe release state.
    if (slot.clip)
    {
        try
        {
            // Nobody will hear this preview: its decode stops now instead of finishing the whole song, so
            // quick song changes do not pile up decodes. It then fails and polling releases it.
            static_cast<void>(slot.clip->CancelLoading());
            std::string error;
            const auto state = slot.clip->LoadState(error);
            if (state == AudioClipLoadState::Loading || state == AudioClipLoadState::Playing)
                retiredClips_.push_back(std::move(slot.clip));
        }
        catch (...)
        {
        }
    }
    slot = {};
}

void SongPreviewController::PollRetiredClips()
{
    std::erase_if(retiredClips_, [](const auto &clip) {
        std::string error;
        const auto state = clip->LoadState(error);
        return state == AudioClipLoadState::Ready || state == AudioClipLoadState::Failed;
    });
}
