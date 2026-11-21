#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace PhantomCore;

bool GameplaySessionController::PrepareResources(const UpdateContext &context)
{
    // Ready and Failed are latched; only the transition is handled here.
    const auto previous = audio_.Resources();
    if (previous != AudioPreparationState::Loading)
        return previous == AudioPreparationState::Ready;
    const auto prepared = audio_.PollResources(context.totalSeconds);
    if (prepared == AudioPreparationState::Failed)
    {
        FailPlaybackPreparation();
        return false;
    }
    return prepared == AudioPreparationState::Ready;
}

void GameplaySessionController::UpdateLoading(const UpdateContext &context)
{
    if (context.input.WasKeyPressed(VK_ESCAPE))
    {
        OpenPauseMenu(context);
        return;
    }
    if (sessionInitialized_)
        ProcessLocalOffsetKeys(context.input);
    if (PrepareResources(context))
        PrepareGameplay(context);
}

void GameplaySessionController::UpdatePause(const UpdateContext &context)
{
    if (!PrepareResources(context))
        return;
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
        attemptStarted_ = true;
        return;
    }
    const auto prepared = audio_.PrepareMusicAt(context.totalSeconds, time_, debugMode_);
    if (prepared == Preparation::Loading)
        return;
    if (prepared == Preparation::Failed)
    {
        FailPlaybackPreparation();
        return;
    }
    if (startHeld_)
        return;
    ScheduleGameplay(context);
}

void GameplaySessionController::FailPlaybackPreparation()
{
    playbackRequested_ = false;
    playbackStart_.reset();
    audio_.StopAllVoices();
    state_ = PlayState::Pause;
    pauseMenuOpen_ = true;
    session_->RecordControl({PlayControlType::Pause, time_, time_, 0, true});
}

void GameplaySessionController::ScheduleGameplay(const UpdateContext &context)
{
    audio_.ApplyAutomation(session_->EvaluateAutomation(time_));
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
    if (launchRequest_.timingProbe != nullptr)
        launchRequest_.timingProbe->OnClockAnchored(start, time_, playbackRate_);
    audio_.SeekMetronome(session_->metronomeBeats, time_);
    audio_.ScheduleMetronome(session_->metronomeBeats, time_, playbackRate_, timer_);
    if (!debugMode_ && !audio_.StartPreparedMusic(time_, timer_))
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
        attemptStarted_ = true;
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
    audio_.StopAllVoices();
    // Preserve the displayed end time, while stopping the judgement clock.
    timer_.Stop();
    result_ = session_->TakeResult(reason, time_);
}
