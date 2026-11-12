#include "App/AssetPaths.h"
#include "GameFlow/GameplaySessionController.h"
#include "Presentation/ModePresentation.h"
#include <map>
#include <vector>
using namespace PhantomCore;

namespace
{
    void RegisterModeHitSounds(GameplayAudioRouter &router, const ModePresentation &presentation,
                               std::string &errorMessage);
}

void GameplaySessionController::InitializeAudio(AudioSystem &audio)
{
    initializationError_.clear();
    std::string initializationError;
    if (!audioRouter_.Initialize(audio, initializationError))
    {
        initializationError_ = initializationError;
        return;
    }

    std::string hitSoundError;
    if (const auto *presentation = FindModePresentation(session_->InputLayout().modeId))
        RegisterModeHitSounds(audioRouter_, *presentation, hitSoundError);
    audioRouter_.PrepareAutomation(session_->Effects());

    // Decode chart resources once on session entry, never on a key press.
    // Indices resolving to the same file share one sample/channel.
    std::map<std::filesystem::path, std::vector<SoundId>> chartSounds;
    for (const auto &[id, path] : session_->HitSoundFiles())
    {
        chartSounds[path].push_back(id);
    }
    for (const auto &[path, ids] : chartSounds)
    {
        std::string error;
        if (!audioRouter_.RegisterSoundAliases(ids, path, error, AudioLoadMode::SampleAsync) &&
            hitSoundError.empty())
        {
            hitSoundError = ids.front() + ": " + error;
        }
    }

    if (session_->Options().metronome)
    {
        std::string error;
        for (const bool accent : {false, true})
            if (!audioRouter_.RegisterSound(accent ? "Gameplay.Metronome.Accent" : "Gameplay.Metronome.Beat",
                EditorSound(accent ? L"MetronomeAccent.wav" : L"MetronomeBeat.wav"), AudioLoadMode::SampleAsync, error, false)
                && hitSoundError.empty()) hitSoundError = error;
    }
    std::string musicError;
    if (launchRequest_.IsValid())
    {
        musicRegistered_ = audioRouter_.RegisterSound("Music.Track", launchRequest_.musicPath,
                                                      AudioLoadMode::StreamAsyncAccurate, musicError);
    }

    if (musicError.empty())
    {
        static_cast<void>(audioRouter_.ConfigurePlayback(session_->Options(), musicError));
        if (musicError.empty() && launchRequest_.editorTestStart)
            static_cast<void>(audioRouter_.SetEditorTestRate(playbackRate_, time_, timer_, musicError));
    }
    if (!hitSoundError.empty() || !musicError.empty())
    {
        initializationError_ = !musicError.empty() ? musicError : hitSoundError;
    }
}

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
    }} // namespace

void GameplaySessionController::ScheduleMetronome()
{
    const auto &beats = session_->metronomeBeats;
    const auto horizon = time_ + RhythmDuration{static_cast<RhythmDuration::rep>(100'000 * playbackRate_)};
    while (metronomeCursor_ < beats.size() && beats[metronomeCursor_].time <= horizon)
    {
        const auto &beat = beats[metronomeCursor_++];
        // Missed update deadlines never cause a burst of catch-up clicks.
        if (beat.time < time_) continue;
        AudioCueRequest cue;
        cue.sound = beat.accent ? "Gameplay.Metronome.Accent" : "Gameplay.Metronome.Beat";
        cue.bus = "UI";
        cue.timelineTime = beat.time;
        audioRouter_.Schedule(std::span<const AudioCueRequest>{&cue, 1}, timer_);
    }
}
