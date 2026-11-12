#include "GameFlow/Submodules/GameplayAudioSession.h"
#include "App/AssetPaths.h"
#include "Presentation/ModePresentation.h"

#include <algorithm>
#include <map>
#include <vector>

using namespace PhantomCore;

namespace
{
    // Each skin file is decoded once and shared by its sound ids. A missing file
    // does not stop the others; the first failure is reported to the Scene.
    void RegisterModeHitSounds(GameplayAudioRouter &router, const ModePresentation &presentation,
                               std::string &errorMessage)
    {
        std::string firstError;
        for (const ModeHitSoundFile &sound : presentation.hitSounds)
        {
            const std::vector<SoundId> ids(sound.soundIds.begin(), sound.soundIds.end());
            std::string registrationError;
            if (!router.RegisterSoundAliases(ids, ModeHitSound(presentation.hitSoundFolder, sound.file),
                                             registrationError, AudioLoadMode::SampleAsync) &&
                firstError.empty())
            {
                firstError = ids.front() + ": " + registrationError;
            }
        }
        errorMessage = std::move(firstError);
    }
} // namespace

void GameplayAudioSession::Register(AudioSystem &audio, const PlaySession &session,
                                    const GameplayLaunchRequest &request, const double playbackRate,
                                    const RhythmTime time, const RhythmTimer &timer)
{
    registered_ = true;
    error_.clear();
    std::string initializationError;
    if (!router_.Initialize(audio, initializationError))
    {
        error_ = initializationError;
        return;
    }

    std::string hitSoundError;
    if (const auto *presentation = FindModePresentation(session.InputLayout().modeId))
        RegisterModeHitSounds(router_, *presentation, hitSoundError);
    router_.PrepareAutomation(session.Effects());

    // Decode chart resources once on session entry, never on a key press.
    // Indices resolving to the same file share one sample/channel.
    std::map<std::filesystem::path, std::vector<SoundId>> chartSounds;
    for (const auto &[id, path] : session.HitSoundFiles())
        chartSounds[path].push_back(id);
    for (const auto &[path, ids] : chartSounds)
    {
        std::string error;
        if (!router_.RegisterSoundAliases(ids, path, error, AudioLoadMode::SampleAsync) && hitSoundError.empty())
            hitSoundError = ids.front() + ": " + error;
    }

    if (session.Options().metronome)
    {
        std::string error;
        for (const bool accent : {false, true})
            if (!router_.RegisterSound(accent ? "Gameplay.Metronome.Accent" : "Gameplay.Metronome.Beat",
                EditorSound(accent ? L"MetronomeAccent.wav" : L"MetronomeBeat.wav"), AudioLoadMode::SampleAsync, error, false)
                && hitSoundError.empty()) hitSoundError = error;
    }
    std::string musicError;
    if (request.IsValid())
        musicRegistered_ = router_.RegisterSound("Music.Track", request.musicPath, AudioLoadMode::StreamAsyncAccurate,
                                                 musicError);

    if (musicError.empty())
    {
        static_cast<void>(router_.ConfigurePlayback(session.Options(), musicError));
        if (musicError.empty() && request.editorTestStart)
            static_cast<void>(router_.SetEditorTestRate(playbackRate, time, timer, musicError));
    }
    if (!hitSoundError.empty() || !musicError.empty())
        error_ = !musicError.empty() ? musicError : hitSoundError;
}

AudioPreparationState GameplayAudioSession::PollResourcesNow()
{
    if (!error_.empty())
        return AudioPreparationState::Failed;
    return router_.PrepareResources();
}

AudioPreparationState GameplayAudioSession::PollResources(const double totalSeconds)
{
    if (resources_ != AudioPreparationState::Loading)
        return resources_;
    if (!registered_ || totalSeconds < nextPoll_)
        return AudioPreparationState::Loading;
    nextPoll_ = totalSeconds + PollIntervalSeconds;
    const auto prepared = router_.PrepareResources();
    resources_ = prepared == AudioPreparationState::Failed || !error_.empty() ? AudioPreparationState::Failed
                                                                               : prepared;
    return resources_;
}

void GameplayAudioSession::RestartResourcePolling() noexcept
{
    resources_ = AudioPreparationState::Loading;
    nextPoll_ = 0;
}

RhythmTime GameplayAudioSession::MusicDuration() const
{
    return router_.MusicDuration().value_or(RhythmTime{});
}

AudioPreparationState GameplayAudioSession::PrepareMusicAt(const double totalSeconds, const RhythmTime time,
                                                           const bool skipMusic)
{
    if (totalSeconds < nextPoll_)
        return AudioPreparationState::Loading;
    nextPoll_ = totalSeconds + PollIntervalSeconds;
    if (skipMusic || !musicRegistered_)
        return AudioPreparationState::Ready;
    return router_.PrepareMusicAt(time);
}

bool GameplayAudioSession::StartPreparedMusic(const RhythmTime time, const RhythmTimer &timer)
{
    return !musicRegistered_ || router_.StartPreparedMusic(time, timer);
}

bool GameplayAudioSession::SetEditorTestRate(const double rate, const RhythmTime time, const RhythmTimer &timer)
{
    std::string error;
    if (router_.SetEditorTestRate(rate, time, timer, error))
        return true;
    error_ = std::move(error);
    return false;
}

void GameplayAudioSession::SeekMetronome(const std::span<const MetronomeBeat> beats, const RhythmTime time) noexcept
{
    metronomeCursor_ = static_cast<std::size_t>(std::ranges::lower_bound(beats, time, {}, &MetronomeBeat::time) -
                                                beats.begin());
}

void GameplayAudioSession::ScheduleMetronome(const std::span<const MetronomeBeat> beats, const RhythmTime time,
                                             const double playbackRate, const RhythmTimer &timer)
{
    const auto horizon = time + RhythmDuration{static_cast<RhythmDuration::rep>(100'000 * playbackRate)};
    while (metronomeCursor_ < beats.size() && beats[metronomeCursor_].time <= horizon)
    {
        const auto &beat = beats[metronomeCursor_++];
        // Missed update deadlines never cause a burst of catch-up clicks.
        if (beat.time < time) continue;
        AudioCueRequest cue;
        cue.sound = beat.accent ? "Gameplay.Metronome.Accent" : "Gameplay.Metronome.Beat";
        cue.bus = "UI";
        cue.timelineTime = beat.time;
        router_.Schedule(std::span<const AudioCueRequest>{&cue, 1}, timer);
    }
}

std::string_view GameplayAudioSession::Error() const noexcept
{
    return router_.LastError().empty() ? std::string_view{error_} : router_.LastError();
}

void GameplayAudioSession::Shutdown() noexcept
{
    registered_ = false;
    RestartResourcePolling();
    router_.Shutdown();
    musicRegistered_ = false;
    metronomeCursor_ = 0;
}
