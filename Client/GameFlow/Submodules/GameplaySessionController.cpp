#include "GameFlow/GameplaySessionController.h"
#include <algorithm>
using namespace PhantomCore;

namespace
{
    bool IsKeyDownBeforeExit(const InputState &input, std::uint16_t key, bool beforeExit)
    {
        bool down = input.IsKeyDown(key);
        if (!beforeExit)
            return down;
        // The InputState snapshot includes the whole frame. Undo edges after
        // ESC so later releases cannot fail a Buzz tick at the exit time.
        const auto events = input.Events();
        for (auto event = events.rbegin(); event != events.rend(); ++event)
        {
            if (event->type == InputEventType::KeyPressed && event->code == VK_ESCAPE)
                break;
            if (event->code != key)
                continue;
            if (event->type == InputEventType::KeyPressed)
                down = false;
            else if (event->type == InputEventType::KeyReleased)
                down = true;
        }
        return down;
    }
} // namespace

void GameplaySessionController::InitializeSession()
{
    if (sessionInitialized_)
        return;
    GameplaySessionFactory::PrepareDebugLaunchRequest(launchRequest_, debugMode_);
    if (!session_)
        session_ = GameplaySessionFactory::Create(launchRequest_, debugMode_);
    if (!launchRequest_.editorTestStart && !session_->CanEnterGameplay())
        throw std::runtime_error("A chart with no playable notes is available only in editor preview/test.");
    heldKeys_.reserve(session_->InputMapping().size());
    playbackRate_ = launchRequest_.editorTestStart ? 1.0 : session_->Options().accelerate;
    if (launchRequest_.editorTestStart)
        for (const auto &lane : session_->Gear().Lanes())
            for (const auto &note : lane->Notes())
                testEndTime_ = std::max(testEndTime_, note->ExpireTime());
    time_ = initialTime_;
    sessionInitialized_ = true;
}

bool GameplaySessionController::PrepareSession()
{
    if (sessionInitialized_)
        return true;
    if (session_ || !launchRequest_.IsValid())
    {
        InitializeSession();
        return true;
    }
    session_ = sessionFactory_.Poll(launchRequest_);
    if (!session_)
        return false;
    InitializeSession();
    return true;
}

void GameplaySessionController::RegisterAudio(AudioSystem &audio)
{
    audio_.Register(audio, *session_, launchRequest_, playbackRate_, time_, timer_);
}

AudioPreparationState GameplaySessionController::PrepareAudioResources(AudioSystem &audio)
{
    if (!audio_.Registered())
        RegisterAudio(audio);
    const auto prepared = audio_.PollResourcesNow();
    if (launchRequest_.editorTestStart && prepared == AudioPreparationState::Ready)
        testEndTime_ = std::max(testEndTime_, audio_.MusicDuration());
    return prepared;
}
void GameplaySessionController::Start(const EngineServices &services,
                                      const RhythmTime initialTime)
{
    initialTime_ = initialTime;
    session_->SetResultInitialTime(initialTime);
    if (state_ == PlayState::Ending)
        return;
    static_cast<void>(PrepareAudioResources(services.audio));
    time_ = initialTime_;
    audio_.RestartResourcePolling();
    playbackRequested_ = state_ == PlayState::Loading;
}
void GameplaySessionController::Update(const UpdateContext &context)
{
    feedback_.clear();
    const auto previousState = state_;
    switch (state_)
    {
    case PlayState::Loading:
        UpdateLoading(context);
        break;
    case PlayState::Gameplay:
        UpdateGameplay(context);
        break;
    case PlayState::Pause:
        UpdatePause(context);
        break;
    case PlayState::Ending:
        break;
    }
    // Keep events timestamped after the scheduled start even when this is the
    // update that finishes Loading/Pause. Earlier preparation input is ignored.
    if (previousState != PlayState::Gameplay && state_ == PlayState::Gameplay)
        UpdateGameplay(context);
    audio_.Update();
}
void GameplaySessionController::UpdateGameplay(const UpdateContext &context)
{
    const auto clock = context.audio.CaptureClockSnapshot();
    ProcessControlKeys(context);
    if (state_ != PlayState::Gameplay)
        return;
    ProcessDebugTimeline(context.input, clock, context.deltaSeconds);
    // Freeze gameplay at ESC's timestamp, not at the later frame
    // clock. Inputs after ESC must not turn unfinished notes into hits/misses.
    std::optional<std::int64_t> exitTicks;
    for (const auto &event : context.input.Events())
        if (event.type == InputEventType::KeyPressed && event.code == VK_ESCAPE &&
            event.performanceCounterTicks >= gameplayStartTicks_)
        {
            exitTicks = event.performanceCounterTicks;
            break;
        }
    ProcessRhythmInput(context, exitTicks);
    if (state_ != PlayState::Gameplay)
    {
        // A Space resume can start asynchronous music preparation before ESC
        // later in this same batch. Keep ESC's request instead of losing it.
        if (exitTicks && state_ == PlayState::Pause)
            PauseAt(*exitTicks, true);
        return;
    }
    time_ = timer_.Now(exitTicks.value_or(clock.performanceCounterTicks));
    UpdateSession(context.input, time_, exitTicks.has_value());
    audio_.ApplyAutomation(session_->EvaluateAutomation(time_));
    if (!exitTicks && timer_.CurrentState() == RhythmTimer::State::Running)
        audio_.ScheduleMetronome(session_->metronomeBeats, time_, playbackRate_, timer_);
    if (exitTicks)
    {
        if (!launchRequest_.editorTestStart && IsPatternComplete())
            EnterEnding(PlayEndReason::Completed);
        else
            PauseAt(*exitTicks, true);
        return;
    }
    if (launchRequest_.editorTestStart && !pauseMenuOpen_ &&
        timer_.CurrentState() == RhythmTimer::State::Running && time_ >= testEndTime_ && IsPatternComplete())
        EnterEnding(PlayEndReason::Completed);
    else if (!launchRequest_.editorTestStart && !debugMode_ && !pauseMenuOpen_ && timer_.CurrentState() == RhythmTimer::State::Running &&
        IsPatternComplete())
    {
        completedElapsedSeconds_ += context.deltaSeconds;
        if (completedElapsedSeconds_ >= 3.0)
            EnterEnding(PlayEndReason::Completed);
    }
}
std::string_view GameplaySessionController::ErrorMessage() const noexcept
{
    if (session_ && !session_->AutomationError().empty())
        return session_->AutomationError();
    return audio_.Error();
}
void GameplaySessionController::Shutdown() noexcept
{
    timer_.Stop();
    state_ = PlayState::Ending;
    result_.reset();
    playbackStart_.reset();
    playbackRequested_ = false;
    audio_.Shutdown();
    session_.reset();
    sessionInitialized_ = false;
    feedback_.clear();
    pauseMenuOpen_ = false;
    completedElapsedSeconds_ = 0;
}
std::unique_ptr<IPlayResult> GameplaySessionController::TakeResult()
{
    return std::move(result_);
}

void GameplaySessionController::UpdateSession(const InputState &input,
                                              const RhythmTime time, bool beforeExit)
{
    heldKeys_.clear();
    // Capacity was reserved when the session was initialized.
    for (const auto &[key, action] : session_->InputMapping())
        if (IsKeyDownBeforeExit(input, key, beforeExit))
            heldKeys_.push_back(key);
    ConsumeResult(session_->Update(time, heldKeys_));
}

bool GameplaySessionController::IsPatternComplete() const noexcept
{
    if (session_ == nullptr || session_->Gear().Lanes().empty())
    {
        return false;
    }
    return std::ranges::all_of(session_->Gear().Lanes(), [](const std::unique_ptr<Lane> &lane)
                               { return lane != nullptr && lane->CurrentNote() == nullptr; });
}

void GameplaySessionController::ConsumeResult(NoteProcessResult result, bool pressed)
{
    audio_.PlayNow(result.audioCues);
    feedback_.push_back({false, pressed, std::move(result)});
}
