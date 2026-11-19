#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
#include <cmath>

using namespace PhantomCore;

void GameplaySessionController::ResetTimeline()
{
    feedback_.push_back({true});
    restartHoldStart_.reset();
    session_->Reset();
    audio_.PauseMusicAndStopSamples();
    audio_.ResetAutomation();
    timer_.Stop();
    time_ = initialTime_;
    state_ = PlayState::Loading;
    attemptStarted_ = false;
    result_.reset();
    playbackStart_.reset();
    playbackRequested_ = true;
    pauseMenuOpen_ = false;
    completedElapsedSeconds_ = 0;
    audio_.ResetPollDeadline();
}

namespace
{
// The restart key (the options' choice, frozen in the launch request) restarts the attempt, but only
// after it has been held for half a second.
constexpr std::int64_t RestartHoldDivisor = 2;
} // namespace

bool GameplaySessionController::RestartHoldCompleted(const UpdateContext &context)
{
    const auto &input = context.input;
    const auto restartKey = launchRequest_.restartKey;
    // Follow the ordered press/release edges so the hold starts at the physical press time.
    for (const auto &event : input.Events())
    {
        if (event.code != restartKey)
            continue;
        if (event.type == InputEventType::KeyReleased)
        {
            restartHoldStart_.reset();
            continue;
        }
        if (event.type != InputEventType::KeyPressed || restartHoldStart_)
            continue;
        // A key still held from the previous restart, or from before the clock started, never counts.
        if (event.performanceCounterTicks <= restartHoldIgnoreTicks_)
            continue;
        if (state_ == PlayState::Gameplay && event.performanceCounterTicks < gameplayStartTicks_)
            continue;
        restartHoldStart_ = event.performanceCounterTicks;
    }
    if (!restartHoldStart_ || !input.IsKeyDown(restartKey))
    {
        restartHoldStart_.reset();
        return false;
    }
    const auto now = context.audio.CaptureClockSnapshot();
    const auto heldTicks = now.performanceCounterTicks - *restartHoldStart_;
    const auto holdTicks = std::max<std::int64_t>(now.performanceCounterFrequency / RestartHoldDivisor, 1);
    restartHoldProgress_ = std::clamp(static_cast<double>(heldTicks) / static_cast<double>(holdTicks), 0.0, 1.0);
    if (heldTicks < holdTicks)
        return false;
    // Holding the key on after the restart must not restart again until it is released and pressed anew.
    restartHoldIgnoreTicks_ = now.performanceCounterTicks;
    restartHoldStart_.reset();
    return true;
}

void GameplaySessionController::ProcessControlKeys(const UpdateContext &context)
{
    if (RestartHoldCompleted(context))
        ResetTimeline();
}

void GameplaySessionController::ToggleDebugTimeline(const UpdateContext &context,
                                                    std::int64_t ticks)
{
    if constexpr (!ReferenceTimeDebug)
        return;
    using State = RhythmTimer::State;
    if (timer_.CurrentState() == State::Running)
    {
        time_ = timer_.Now(ticks);
        timer_.Pause(ticks);
        audio_.PauseMusicAndStopSamples();
        session_->RecordControl({PlayControlType::Pause, time_, time_});
    }
    else if (timer_.CurrentState() == State::Paused)
    {
        if (debugMode_)
        {
            timer_.Resume(ticks);
            const auto clock = context.audio.CaptureClockSnapshot();
            timer_.AnchorDspClock(timer_.Now(clock.performanceCounterTicks), clock.dspClock, clock.sampleRate);
            return;
        }
        // Keep accepting note input while frozen, but wait for music seek and
        // the shared QPC/DSP start deadline when returning to normal playback.
        state_ = PlayState::Pause;
        static_cast<void>(ContinueAt(context, time_, PlayControlType::Resume));
    }
}

void GameplaySessionController::ProcessDebugTimeline(const InputState &input,
                                                     const AudioClockSnapshot &clock,
                                                     const double deltaSeconds)
{
    if constexpr (!ReferenceTimeDebug)
        return;
    if (timer_.CurrentState() != RhythmTimer::State::Paused)
        return;

    std::int64_t deltaMicroseconds{};
    if (input.IsKeyDown(static_cast<std::uint16_t>('1')))
    {
        deltaMicroseconds -=
            static_cast<std::int64_t>(std::llround(deltaSeconds * 1'000'000.0));
    }
    if (input.IsKeyDown(static_cast<std::uint16_t>('2')))
    {
        deltaMicroseconds +=
            static_cast<std::int64_t>(std::llround(deltaSeconds * 1'000'000.0));
    }
    if (input.WasKeyPressed(static_cast<std::uint16_t>('3')))
    {
        deltaMicroseconds -= 1000;
    }
    if (input.WasKeyPressed(static_cast<std::uint16_t>('4')))
    {
        deltaMicroseconds += 1000;
    }
    if (deltaMicroseconds == 0)
    {
        return;
    }

    const auto before = timer_.Now(clock.performanceCounterTicks);
    const auto after = before + RhythmDuration{deltaMicroseconds};
    audio_.PauseMusicAndStopSamples();
    if (after < before)
    {
        feedback_.push_back({true});
        session_->RewindJudgements(after);
        audio_.ResetAutomation();
    }
    timer_.Seek(after, clock.performanceCounterTicks);
    timer_.AnchorDspClock(after, clock.dspClock, clock.sampleRate);
    audio_.SeekMetronome(session_->metronomeBeats, after);
}

void GameplaySessionController::ProcessRhythmInput(const UpdateContext &context,
                                                   std::optional<std::int64_t> exitTicks)
{
    const auto &input = context.input;
    bool synchronized = false;
    const auto events = input.Events();
    for (std::size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex)
    {
        const auto &event = events[eventIndex];
        if (event.performanceCounterTicks < gameplayStartTicks_)
            continue;
        if (exitTicks && event.performanceCounterTicks > *exitTicks)
            break;
        if (event.type == InputEventType::KeyPressed && event.code == VK_ESCAPE)
            break;
        if (event.type != InputEventType::KeyPressed &&
            event.type != InputEventType::KeyReleased)
        {
            continue;
        }
        if constexpr (ReferenceTimeDebug)
        {
            if (event.type == InputEventType::KeyPressed && event.code == VK_SPACE)
            {
                // Process earlier edges on the running clock before freezing;
                // later edges in this batch use the exact frozen chart time.
                UpdateBeforeInput(input, eventIndex, timer_.Now(event.performanceCounterTicks));
                ToggleDebugTimeline(context, event.performanceCounterTicks);
                synchronized = false;
                if (state_ != PlayState::Gameplay)
                    break;
                continue;
            }
        }
        if (!session_->InputMapping().contains(event.code))
            continue;
        const auto edge = event.type == InputEventType::KeyPressed
                              ? InputEdge::Pressed
                              : InputEdge::Released;
        const auto eventTime = timer_.Now(event.performanceCounterTicks);
        if (!synchronized)
        {
            UpdateBeforeInput(input, eventIndex, eventTime);
            synchronized = true;
        }
        NoteProcessResult result = session_->ProcessInput(event.code, edge, eventTime);
        ConsumeResult(std::move(result), edge == InputEdge::Pressed);
    }
}

void GameplaySessionController::UpdateBeforeInput(const InputState &input,
                                                  const std::size_t eventIndex,
                                                  const RhythmTime time)
{
    // Raw Input's snapshot is after the entire message batch. Undo this edge
    // and every later edge to recover the physical state immediately before
    // the first judged input, including keys already held during Loading/Pause.
    heldKeys_.clear();
    const auto remaining = input.Events().subspan(eventIndex);
    for (const auto &[key, action] : session_->InputMapping())
    {
        bool down = input.IsKeyDown(key);
        for (auto event = remaining.rbegin(); event != remaining.rend(); ++event)
            if (event->code == key)
            {
                if (event->type == InputEventType::KeyPressed)
                    down = false;
                else if (event->type == InputEventType::KeyReleased)
                    down = true;
            }
        if (down)
            heldKeys_.push_back(key);
    }
    if (time != RhythmTime::min())
        ConsumeResult(session_->Update(time - RhythmDuration{1}, heldKeys_));
}
