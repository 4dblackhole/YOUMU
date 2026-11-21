#pragma once
#include "GameFlow/Submodules/GameplayAudioSession.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "GameFlow/GameplayFeedback.h"
#include "GameFlow/GameplayTimingProbe.h"
#include "Mode/Submodules/PlaySession.h"
#include "GameFlow/Submodules/GameplaySessionFactory.h"
#include "PhantomCore.h"
#include "Time/RhythmTimer.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// What a gameplay diagnostic comes from: an effect-file evaluation, which names its file and line,
// or the audio output and its music/hit-sound preparation.
enum class GameplayErrorSource
{
    None,
    Effect,
    Audio
};

// Every gameplay Scene uses the same lifecycle, independent of its note rules.
enum class PlayState
{
    Loading,
    Gameplay,
    Pause,
    Ending
};

class GameplaySessionController final
{
  public:
    GameplaySessionController(GameplayLaunchRequest request, PhantomCore::AudioPlaybackManager &playback,
                              bool debug, std::unique_ptr<PlaySession> prepared = {})
        : launchRequest_(std::move(request)), session_(std::move(prepared)), audio_(playback), debugMode_(debug)
    {
    }
    ~GameplaySessionController()
    {
        Shutdown();
    }
    void InitializeSession();
    bool PrepareSession();
    bool SessionReady() const noexcept { return sessionInitialized_; }
    PhantomCore::AudioPreparationState PrepareAudioResources(PhantomCore::AudioSystem &audio);
    void Start(const PhantomCore::EngineServices &services, RhythmTime initialTime);
    void Update(const PhantomCore::UpdateContext &context);
    double PlaybackRate() const noexcept { return playbackRate_; }
    PlayState CurrentState() const noexcept
    {
        return state_;
    }
    void OpenPauseMenu(const PhantomCore::UpdateContext &context);
    // true means the command was accepted once. CurrentState remains Pause
    // until background-music preparation and the shared start deadline finish.
    bool ResumeFromPause(const PhantomCore::UpdateContext &context, RhythmDuration leadIn);
    bool RewindFromPause(const PhantomCore::UpdateContext &context, std::size_t measures);
    // Closes the menu and fades the screen to black for RetryFadeSeconds before the attempt restarts.
    void RetryFromPause(const PhantomCore::UpdateContext &context);
    // Advances a chosen Retry's fade and restarts at its end; true while one is pending. Update calls
    // it; the Scene also calls it while the play screen is still being prepared.
    bool UpdatePendingRetry(const PhantomCore::UpdateContext &context);
    void RetireFromPause();
    // Black over the whole screen, 0..1: the restart key's hold progress, or a chosen Retry's fade.
    // The music volume follows it down. A completed restart drops it to 0 at once.
    [[nodiscard]] double ScreenFade() const noexcept
    {
        if (retryFadeElapsed_)
            return std::min(*retryFadeElapsed_ / RetryFadeSeconds, 1.0);
        return restartHoldStart_ ? restartHoldProgress_ : 0.0;
    }
    static constexpr double RetryFadeSeconds = 0.3;
    bool PauseMenuOpen() const noexcept
    {
        return pauseMenuOpen_;
    }
    bool CanContinue() const noexcept
    {
        return state_ == PlayState::Pause && audio_.Resources() == PhantomCore::AudioPreparationState::Ready &&
               !playbackRequested_;
    }
    void Shutdown() noexcept;
    const PlaySession &Session() const
    {
        return *session_;
    }
    void SetHpGauges(std::span<IHpGauge *const> gauges) { session_->SetHpGauges(gauges); }
    // True once the pause menu or Rewind disabled HP for this attempt.
    [[nodiscard]] bool HpDisabled() const noexcept { return session_ && session_->HpDisabled(); }
    RhythmTime Time() const noexcept
    {
        return time_;
    }
    std::span<const GameplayFeedback> Feedback() const noexcept
    {
        return feedback_;
    }
    std::string_view ErrorMessage() const noexcept;
    GameplayErrorSource ErrorSource() const noexcept;
    // False from Loading until this attempt's clock first runs; Retry clears it again. The Scene keeps
    // the loading screen while it is false, including a pause menu opened before the start.
    bool AttemptStarted() const noexcept
    {
        return attemptStarted_;
    }
    // The attempt's music or hit sounds failed to load: only Retry (reload) or Retire remain.
    bool PreparationFailed() const noexcept
    {
        return state_ == PlayState::Pause && audio_.Resources() == PhantomCore::AudioPreparationState::Failed;
    }
    bool ShouldShowResult() const noexcept
    {
        return state_ == PlayState::Ending && result_ != nullptr;
    }
    // Transfer an already finalized Ending result once; never retain play objects.
    std::unique_ptr<IPlayResult> TakeResult();

    // The pattern's local offset (osu!-style), adjustable before the first note reaches the
    // judgement line. The session was compiled with the launch request's offset; a later
    // change moves the chart clock and the music lead instead of recompiling.
    [[nodiscard]] std::int32_t LocalOffsetMs() const noexcept { return launchRequest_.localOffsetMs + offsetChangeMs_; }
    [[nodiscard]] bool LocalOffsetChanged() const noexcept { return offsetChangeMs_ != 0; }
    // Advances whenever the local offset changes, so the screen can show the new value.
    [[nodiscard]] std::uint64_t LocalOffsetRevision() const noexcept { return offsetRevision_; }
    [[nodiscard]] bool CanAdjustLocalOffset() const noexcept;
    // Adds milliseconds, clamped to LocalOffsetStore::Limit; false when not allowed or unchanged.
    bool AdjustLocalOffset(std::int32_t milliseconds);
    // While held (the pointer rests on the loading screen's play settings), a prepared attempt
    // waits before scheduling its start; the music stays prepared.
    void SetStartHeld(bool held) noexcept { startHeld_ = held; }

  private:
#if defined(_DEBUG)
    static constexpr bool ReferenceTimeDebug = true;
#else
    static constexpr bool ReferenceTimeDebug = false;
#endif
    std::vector<PhysicalKey> heldKeys_;
    void RegisterAudio(PhantomCore::AudioSystem &audio);
    void UpdateLoading(const PhantomCore::UpdateContext &context);
    void UpdateGameplay(const PhantomCore::UpdateContext &context);
    void UpdatePause(const PhantomCore::UpdateContext &context);
    bool PrepareResources(const PhantomCore::UpdateContext &context);
    void PrepareGameplay(const PhantomCore::UpdateContext &context);
    void ScheduleGameplay(const PhantomCore::UpdateContext &context);
    void FailPlaybackPreparation();
    void EnterEnding(PlayEndReason reason);
    void PauseAt(std::int64_t ticks, bool menuShown);
    bool ContinueAt(const PhantomCore::UpdateContext &context, RhythmTime target,
                    PlayControlType action);
    void ResetTimeline();
    void ProcessControlKeys(const PhantomCore::UpdateContext &context);
    // +/- (and the numeric pad) change the local offset by 5 ms; Alt makes it 1 ms, Shift 20 ms.
    void ProcessLocalOffsetKeys(const PhantomCore::InputState &input);
    // True once the launch request's restart key has been held for half a second.
    bool RestartHoldCompleted(const PhantomCore::UpdateContext &context);
    void ToggleDebugTimeline(const PhantomCore::UpdateContext &context, std::int64_t ticks);
    void ProcessDebugTimeline(const PhantomCore::InputState &input, const PhantomCore::AudioClockSnapshot &clock,
                              const double deltaSeconds);
    void ProcessRhythmInput(const PhantomCore::UpdateContext &context, std::optional<std::int64_t> exitTicks);
    void UpdateBeforeInput(const PhantomCore::InputState &input, std::size_t eventIndex,
                           RhythmTime time);
    void UpdateSession(const PhantomCore::InputState &input, const RhythmTime time,
                       bool beforeExit = false);
    bool IsPatternComplete() const noexcept;
    void ConsumeResult(NoteProcessResult result, bool pressed = false);
    GameplayLaunchRequest launchRequest_;
    std::unique_ptr<PlaySession> session_;
    GameplaySessionFactory sessionFactory_;
    bool sessionInitialized_{};
    RhythmTimer timer_;
    GameplayAudioSession audio_;
    std::vector<GameplayFeedback> feedback_;
    RhythmTime time_{};
    RhythmTime initialTime_{-2'000'000};
    PlayState state_{PlayState::Loading};
    std::unique_ptr<IPlayResult> result_;
    std::optional<PhantomCore::AudioClockSnapshot> playbackStart_;
    std::int64_t gameplayStartTicks_{};
    // QPC time the restart key went down while counting, and the latest restart; earlier presses are ignored.
    std::optional<std::int64_t> restartHoldStart_;
    std::int64_t restartHoldIgnoreTicks_{};
    double restartHoldProgress_{}; // Fraction of the hold reached while restartHoldStart_ is set.
    std::optional<double> retryFadeElapsed_; // Seconds since a pause-menu Retry was chosen.
    double completedElapsedSeconds_{};
    std::int32_t offsetChangeMs_{}; // Local offset change since the session was compiled.
    bool startHeld_{};
    std::uint64_t offsetRevision_{};
    RhythmTime firstNoteTime_{RhythmTime::max()};
    double playbackRate_{1.0};
    bool playbackRequested_{};
    bool debugMode_{};
    bool pauseMenuOpen_{};
    bool attemptStarted_{};
};
