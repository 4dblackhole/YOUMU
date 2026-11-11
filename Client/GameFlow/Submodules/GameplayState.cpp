#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;

void GameplaySessionController::SetEditorTestRate(double rate, const UpdateContext &context)
{
    if (!launchRequest_.editorTestStart || !std::isfinite(rate) ||
        rate < GameplayLaunchRequest::EditorTestMinimumRate || rate > GameplayLaunchRequest::EditorTestMaximumRate)
        throw std::invalid_argument("Editor Test rate must be within 0.25..2.0.");
    if (rate == playbackRate_ || state_ == PlayState::Ending) return;
    const auto clock = context.audio.CaptureClockSnapshot();
    if (state_ == PlayState::Gameplay)
        time_ = timer_.Now(clock.performanceCounterTicks);
    const bool running = state_ == PlayState::Gameplay;
    if (playbackStart_)
    {
        audioRouter_.PauseMusicAndStopSamples();
        playbackStart_.reset();
        nextAudioPreparationPoll_ = 0;
    }
    playbackRate_ = rate;
    timer_.Start(clock.performanceCounterTicks, clock.performanceCounterFrequency, time_, rate);
    timer_.AnchorDspClock(time_, clock.dspClock, clock.sampleRate);
    if (!running) timer_.Pause(clock.performanceCounterTicks);
    std::string error;
    if (!audioRouter_.SetEditorTestRate(rate, time_, timer_, error))
    {
        initializationError_ = std::move(error);
        FailPlaybackPreparation();
    }
}

bool GameplaySessionController::PrepareResources(const UpdateContext &context)
{
    if (resourcePreparation_ == AudioPreparationState::Ready)
        return true;
    if (resourcePreparation_ == AudioPreparationState::Failed)
        return false;
    if (!audioInitialized_ || context.totalSeconds < nextAudioPreparationPoll_)
        return false;
    nextAudioPreparationPoll_ = context.totalSeconds + 0.002;
    const auto prepared = audioRouter_.PrepareResources();
    if (prepared == AudioPreparationState::Failed || !initializationError_.empty())
    {
        resourcePreparation_ = AudioPreparationState::Failed;
        FailPlaybackPreparation();
        return false;
    }
    resourcePreparation_ = prepared;
    if (resourcePreparation_ == AudioPreparationState::Ready && launchRequest_.editorTestStart)
        testEndTime_ = std::max(testEndTime_, audioRouter_.MusicDuration().value_or(RhythmTime{}));
    return resourcePreparation_ == AudioPreparationState::Ready;
}

void GameplaySessionController::UpdateLoading(const UpdateContext &context)
{
    if (launchRequest_.editorTestStart && context.input.WasKeyPressed(VK_SPACE))
    {
        PauseAt(context.audio.CaptureClockSnapshot().performanceCounterTicks, true);
        return;
    }
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        OpenPauseMenu(context);
        return;
    }
    if (PrepareResources(context))
        PrepareGameplay(context);
}

void GameplaySessionController::UpdatePause(const UpdateContext &context)
{
    if (!PrepareResources(context))
        return;
    if (launchRequest_.editorTestStart)
    {
        if (context.input.WasKeyPressed(VK_SPACE))
        {
            if (playbackRequested_)
                PauseAt(context.audio.CaptureClockSnapshot().performanceCounterTicks, true);
            else if (CanContinue())
                static_cast<void>(ContinueAt(context, time_, PlayControlType::Resume));
        }
        else if (playbackRequested_)
            PrepareGameplay(context);
        return;
    }
    if (playbackRequested_)
    {
        // ESC cancels the pending start at its frozen target. Record the newly
        // shown menu too, including a Space resume that was not yet practice.
        if (context.input.WasKeyPressed(VK_ESCAPE))
        {
            PauseAt(context.audio.CaptureClockSnapshot().performanceCounterTicks, true);
            return;
        }
        PrepareGameplay(context);
    }
    else if (!pauseMenuOpen_)
    {
        if (context.input.WasKeyPressed(VK_ESCAPE))
            OpenPauseMenu(context);
        else
            ProcessControlKeys(context);
    }
}

void GameplaySessionController::PrepareGameplay(const UpdateContext &context)
{
    using Preparation = AudioPreparationState;
    if (!playbackRequested_)
        return;
    if (playbackStart_)
    {
        const auto now = context.audio.CaptureClockSnapshot();
        if (now.performanceCounterTicks < playbackStart_->performanceCounterTicks)
            return;
        // QPC and DSP were scheduled for the same future instant. Frames that
        // arrive after it retain the actual elapsed time rather than restarting.
        timer_.Resume(playbackStart_->performanceCounterTicks);
        gameplayStartTicks_ = playbackStart_->performanceCounterTicks;
        playbackStart_.reset();
        playbackRequested_ = false;
        pauseMenuOpen_ = false;
        state_ = PlayState::Gameplay;
        return;
    }
    if (context.totalSeconds < nextAudioPreparationPoll_)
        return;
    nextAudioPreparationPoll_ = context.totalSeconds + 0.002;
    if (!debugMode_ && musicRegistered_)
    {
        const auto prepared = audioRouter_.PrepareMusicAt(time_);
        if (prepared == Preparation::Loading)
            return;
        if (prepared == Preparation::Failed)
        {
            FailPlaybackPreparation();
            return;
        }
    }
    ScheduleGameplay(context);
}

void GameplaySessionController::FailPlaybackPreparation()
{
    playbackRequested_ = false;
    playbackStart_.reset();
    audioRouter_.StopAllVoices();
    state_ = PlayState::Pause;
    pauseMenuOpen_ = true;
    session_->RecordControl({PlayControlType::Pause, time_, time_, 0, true});
}

void GameplaySessionController::ScheduleGameplay(const UpdateContext &context)
{
    audioRouter_.ApplyAutomation(session_->EvaluateAutomation(time_));
    auto start = context.audio.CaptureClockSnapshot();
    // Reserve the same 20 ms on both clocks; a DSP-only lead makes music late.
    if (!debugMode_)
    {
        start.performanceCounterTicks += start.performanceCounterFrequency / 50;
        start.dspClock += static_cast<std::uint64_t>(std::max(start.sampleRate, 0)) / 50;
    }
    timer_.Start(start.performanceCounterTicks, start.performanceCounterFrequency, time_, playbackRate_);
    timer_.Pause(start.performanceCounterTicks);
    timer_.AnchorDspClock(time_, start.dspClock, start.sampleRate);
    metronomeCursor_ = static_cast<std::size_t>(std::ranges::lower_bound(session_->metronomeBeats, time_, {}, &MetronomeBeat::time) - session_->metronomeBeats.begin());
    ScheduleMetronome();
    if (!debugMode_ && musicRegistered_ && !audioRouter_.StartPreparedMusic(time_, timer_))
    {
        FailPlaybackPreparation();
        return;
    }
    if (debugMode_)
    {
        if (!ReferenceTimeDebug)
            timer_.Resume(start.performanceCounterTicks);
        gameplayStartTicks_ = start.performanceCounterTicks;
        playbackRequested_ = false;
        pauseMenuOpen_ = false;
        state_ = PlayState::Gameplay;
    }
    else
        playbackStart_ = start;
}

void GameplaySessionController::EnterEnding(PlayEndReason reason)
{
    if (state_ == PlayState::Ending)
        return;
    state_ = PlayState::Ending;
    playbackRequested_ = false;
    playbackStart_.reset();
    pauseMenuOpen_ = false;
    audioRouter_.StopAllVoices();
    // Preserve the displayed end time, while stopping the judgement clock.
    timer_.Stop();
    result_ = session_->TakeResult(reason, time_);
}
