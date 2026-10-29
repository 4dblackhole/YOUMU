#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
#include <cmath>

void GameplaySessionController::ResetTimeline()
{
    feedback_.push_back({true});
    session_->Reset();
    audioRouter_.PauseMusicAndStopSamples();
    audioRouter_.ResetAutomation();
    timer_.Stop();
    time_ = initialTime_;
    state_ = PlayState::Loading;
    result_.reset();
    playbackStart_.reset();
    playbackRequested_ = true;
    pauseMenuOpen_ = false;
    completedElapsedSeconds_ = 0;
    nextAudioPreparationPoll_ = 0;
}

void GameplaySessionController::ProcessControlKeys(const UpdateContext &context)
{
    const auto &input = context.input;
    const auto pressed = [&](std::uint16_t key)
    {
        return std::ranges::any_of(
            input.Events(),
            [&](const auto &event)
            {
                return event.type == InputEventType::KeyPressed && event.code == key &&
                       (state_ != PlayState::Gameplay || event.performanceCounterTicks >= gameplayStartTicks_);
            });
    };
    if (pressed('R'))
    {
        ResetTimeline();
        return;
    }
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
        audioRouter_.PauseMusicAndStopSamples();
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
    audioRouter_.PauseMusicAndStopSamples();
    if (after < before)
    {
        feedback_.push_back({true});
        session_->RewindJudgements(after);
        audioRouter_.ResetAutomation();
    }
    timer_.Seek(after, clock.performanceCounterTicks);
    timer_.AnchorDspClock(after, clock.dspClock, clock.sampleRate);
    metronomeCursor_ = static_cast<std::size_t>(std::ranges::lower_bound(
        session_->metronomeBeats, after, {}, &MetronomeBeat::time) - session_->metronomeBeats.begin());
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
        if ((exitTicks && event.performanceCounterTicks > *exitTicks) ||
            (event.type == InputEventType::KeyPressed && event.code == VK_ESCAPE))
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
