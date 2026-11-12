#pragma once

#include "Audio/GameplayAudioRouter.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Mode/Submodules/PlaySession.h"
#include "PhantomCore.h"
#include "Time/RhythmTimer.h"

#include <span>
#include <string>
#include <string_view>

// Owns one play session's audio: the router, sound registration, resource
// readiness, music preparation and the metronome cursor. The controller owns
// the transport (state, timer and start deadline) and tells this object when
// playback pauses, seeks or starts.
class GameplayAudioSession final
{
  public:
    explicit GameplayAudioSession(PhantomCore::AudioPlaybackManager &playback) : router_(playback) {}

    // Registers mode hit sounds, chart samples, the optional metronome and the
    // music track, then applies play options. Registration failures are kept
    // as Error(); resources fail to prepare until Register is called again.
    void Register(PhantomCore::AudioSystem &audio, const PlaySession &session, const GameplayLaunchRequest &request,
                  double playbackRate, RhythmTime time, const RhythmTimer &timer);
    [[nodiscard]] bool Registered() const noexcept { return registered_; }

    // Unthrottled readiness check before the Scene starts.
    [[nodiscard]] PhantomCore::AudioPreparationState PollResourcesNow();
    // Throttled per-frame readiness check. Ready and Failed are latched until
    // RestartResourcePolling.
    [[nodiscard]] PhantomCore::AudioPreparationState PollResources(double totalSeconds);
    [[nodiscard]] PhantomCore::AudioPreparationState Resources() const noexcept { return resources_; }
    void RestartResourcePolling() noexcept;
    // The next throttled poll runs on the next frame.
    void ResetPollDeadline() noexcept { nextPoll_ = 0; }
    [[nodiscard]] RhythmTime MusicDuration() const;

    // Throttled music seek before a start. Ready also when there is no music
    // to prepare (debug playback or a session without a music track).
    [[nodiscard]] PhantomCore::AudioPreparationState PrepareMusicAt(double totalSeconds, RhythmTime time,
                                                                    bool skipMusic);
    // Starts the music at the timer's anchored deadline; true without music.
    [[nodiscard]] bool StartPreparedMusic(RhythmTime time, const RhythmTimer &timer);
    [[nodiscard]] bool SetEditorTestRate(double rate, RhythmTime time, const RhythmTimer &timer);

    void SeekMetronome(std::span<const MetronomeBeat> beats, RhythmTime time) noexcept;
    // Schedules beats within the next 100 ms of chart time at most once each.
    void ScheduleMetronome(std::span<const MetronomeBeat> beats, RhythmTime time, double playbackRate,
                           const RhythmTimer &timer);

    void PlayNow(std::span<const AudioCueRequest> cues) { router_.PlayNow(cues); }
    void ApplyAutomation(std::span<const AutomationValue> values) { router_.ApplyAutomation(values); }
    void ResetAutomation() { router_.ResetAutomation(); }
    void PauseMusicAndStopSamples() { router_.PauseMusicAndStopSamples(); }
    void StopAllVoices() noexcept { router_.StopAllVoices(); }
    void Update() { router_.Update(); }
    [[nodiscard]] std::string_view Error() const noexcept;
    void Shutdown() noexcept;

  private:
    static constexpr double PollIntervalSeconds = 0.002;

    GameplayAudioRouter router_;
    std::string error_;
    bool registered_{};
    bool musicRegistered_{};
    PhantomCore::AudioPreparationState resources_{PhantomCore::AudioPreparationState::Loading};
    double nextPoll_{};
    std::size_t metronomeCursor_{};
};
