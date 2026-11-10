#pragma once
#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "GameFlow/GameplayFeedback.h"
#include "Mode/Submodules/PlaySession.h"
#include "GameFlow/Submodules/GameplaySessionFactory.h"
#include "PhantomCore.h"
#include "Time/RhythmTimer.h"
#include <memory>
#include <span>
#include <string_view>
#include <vector>

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
    GameplaySessionController(GameplayLaunchRequest request, AudioPlaybackManager &playback,
                              bool debug, std::unique_ptr<PlaySession> prepared = {})
        : launchRequest_(std::move(request)), session_(std::move(prepared)), audioRouter_(playback), debugMode_(debug)
    {
    }
    ~GameplaySessionController()
    {
        Shutdown();
    }
    void InitializeSession();
    bool PrepareSession();
    bool SessionReady() const noexcept { return sessionInitialized_; }
    AudioPreparationState PrepareAudioResources(AudioSystem &audio);
    void Start(const EngineServices &services, RhythmTime initialTime);
    void Update(const UpdateContext &context);
    double PlaybackRate() const noexcept { return playbackRate_; }
    void SetEditorTestRate(double rate, const UpdateContext &context);
    PlayState CurrentState() const noexcept
    {
        return state_;
    }
    void OpenPauseMenu(const UpdateContext &context);
    // true means the command was accepted once. CurrentState remains Pause
    // until background-music preparation and the shared start deadline finish.
    bool ResumeFromPause(const UpdateContext &context, RhythmDuration leadIn);
    bool RewindFromPause(const UpdateContext &context, std::size_t measures);
    void RetryFromPause(const UpdateContext &context);
    void RetireFromPause();
    bool PauseMenuOpen() const noexcept
    {
        return pauseMenuOpen_;
    }
    bool CanContinue() const noexcept
    {
        return state_ == PlayState::Pause && resourcePreparation_ == AudioPreparationState::Ready && !playbackRequested_;
    }
    void Shutdown() noexcept;
    const PlaySession &Session() const
    {
        return *session_;
    }
    void SetHpGauges(std::span<IHpGauge *const> gauges) { session_->SetHpGauges(gauges); }
    RhythmTime Time() const noexcept
    {
        return time_;
    }
    std::span<const GameplayFeedback> Feedback() const noexcept
    {
        return feedback_;
    }
    std::string_view ErrorMessage() const noexcept;
    bool ShouldShowResult() const noexcept
    {
        return state_ == PlayState::Ending && result_ != nullptr;
    }
    // Transfer an already finalized Ending result once; never retain play objects.
    std::unique_ptr<IPlayResult> TakeResult();

  private:
#if defined(_DEBUG)
    static constexpr bool ReferenceTimeDebug = true;
#else
    static constexpr bool ReferenceTimeDebug = false;
#endif
    std::vector<PhysicalKey> heldKeys_;
    void InitializeAudio(AudioSystem &audio);
    void ScheduleMetronome();
    std::size_t metronomeCursor_{};
    void UpdateLoading(const UpdateContext &context);
    void UpdateGameplay(const UpdateContext &context);
    void UpdatePause(const UpdateContext &context);
    bool PrepareResources(const UpdateContext &context);
    void PrepareGameplay(const UpdateContext &context);
    void ScheduleGameplay(const UpdateContext &context);
    void FailPlaybackPreparation();
    void EnterEnding(PlayEndReason reason);
    void PauseAt(std::int64_t ticks, bool menuShown);
    bool ContinueAt(const UpdateContext &context, RhythmTime target,
                    PlayControlType action);
    void ResetTimeline();
    void ProcessControlKeys(const UpdateContext &context);
    void ToggleDebugTimeline(const UpdateContext &context, std::int64_t ticks);
    void ProcessDebugTimeline(const InputState &input, const AudioClockSnapshot &clock,
                              const double deltaSeconds);
    void ProcessRhythmInput(const UpdateContext &context, std::optional<std::int64_t> exitTicks);
    void UpdateBeforeInput(const InputState &input, std::size_t eventIndex,
                           RhythmTime time);
    void UpdateSession(const InputState &input, const RhythmTime time,
                       bool beforeExit = false);
    bool IsPatternComplete() const noexcept;
    void ConsumeResult(NoteProcessResult result, bool pressed = false);
    GameplayLaunchRequest launchRequest_;
    std::unique_ptr<PlaySession> session_;
    GameplaySessionFactory sessionFactory_;
    bool sessionInitialized_{};
    RhythmTimer timer_;
    GameplayAudioRouter audioRouter_;
    std::vector<GameplayFeedback> feedback_;
    RhythmTime time_{};
    RhythmTime initialTime_{-2'000'000};
    PlayState state_{PlayState::Loading};
    std::unique_ptr<IPlayResult> result_;
    std::optional<AudioClockSnapshot> playbackStart_;
    std::int64_t gameplayStartTicks_{};
    std::string initializationError_;
    double completedElapsedSeconds_{};
    double playbackRate_{1.0};
    RhythmTime testEndTime_{};
    bool musicRegistered_{};
    bool audioInitialized_{}, playbackRequested_{};
    AudioPreparationState resourcePreparation_{AudioPreparationState::Loading};
    double nextAudioPreparationPoll_{};
    bool debugMode_{};
    bool pauseMenuOpen_{};
};
