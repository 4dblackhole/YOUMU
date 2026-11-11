#pragma once

#include "PhantomCore.h"

#include "Mode/PlayGameMode.h"
#include "Time/RhythmTimer.h"

#include <filesystem>
#include <map>
#include <memory>
#include <span>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class GameplayAudioRouter final
{
public:
    explicit GameplayAudioRouter(
        PhantomCore::AudioPlaybackManager& playback) noexcept;
    ~GameplayAudioRouter();

    GameplayAudioRouter(const GameplayAudioRouter&) = delete;
    GameplayAudioRouter& operator=(const GameplayAudioRouter&) = delete;

    [[nodiscard]] bool Initialize(
        PhantomCore::AudioSystem& audioSystem,
        std::string& errorMessage);
    void Shutdown() noexcept;
    [[nodiscard]] bool RegisterSound(
        SoundId soundId,
        const std::filesystem::path& path,
        PhantomCore::AudioLoadMode loadMode,
        std::string& errorMessage, bool restartWhilePlaying = true);
    [[nodiscard]] bool RegisterSoundAliases(std::span<const SoundId> soundIds,
                                            const std::filesystem::path &path, std::string &errorMessage,
                                            PhantomCore::AudioLoadMode loadMode = PhantomCore::AudioLoadMode::Sample);
    PhantomCore::AudioPreparationState PrepareResources();
    bool ConfigurePlayback(const PlayOptions &options, std::string &error);
    bool SetEditorTestRate(double rate, RhythmTime position, const RhythmTimer &timer, std::string &error);
    std::optional<RhythmTime> MusicDuration() const;

    // Input-driven cues start immediately. Timeline scheduling remains a
    // separate path for music and other deliberately future playback.
    void PlayNow(std::span<const AudioCueRequest> cues);
    void Schedule(
        std::span<const AudioCueRequest> cues,
        const RhythmTimer& timer);
    void ApplyAutomation(
        std::span<const AutomationValue> values);
    void PrepareAutomation(std::span<const CompiledEffectCommand> commands);
    void StopAllVoices() noexcept;
    // Preserve a live music voice; transient samples must never resume.
    void PauseMusicAndStopSamples();
    PhantomCore::AudioPreparationState PrepareMusicAt(RhythmTime position);
    bool StartPreparedMusic(RhythmTime position, const RhythmTimer &timer);
    void ResetAutomation();
    void Update();

    [[nodiscard]] std::string_view LastError() const noexcept;

private:
    struct RegisteredSound
    {
        std::shared_ptr<PhantomCore::AudioClip> clip;
        bool restartWhilePlaying{};
    };

    void Route(
        std::span<const AudioCueRequest> cues,
        const RhythmTimer* timer);
    [[nodiscard]] PhantomCore::AudioBus* FindBus(
        std::string_view id) noexcept;
    [[nodiscard]] PhantomCore::AudioEffect* EnsureEffect(
        std::string_view busId,
        PhantomCore::AudioEffectType type);
    void ApplyAutomationValue(const AutomationValue& value);

    float musicPitch_{1.0F}, hitSoundPitch_{1.0F};
    bool editorTestPlayback_{};
    std::map<PhantomCore::AudioPlaybackId, RhythmTime> scheduledCueTimes_;
    PhantomCore::AudioSystem* audioSystem_{};
    PhantomCore::AudioPlaybackManager& playback_;
    std::map<std::string, std::shared_ptr<PhantomCore::AudioBus>, std::less<>>
        buses_;
    std::map<std::string, RegisteredSound, std::less<>> clips_;
    std::map<std::string,
        std::map<PhantomCore::AudioEffectType,
            std::unique_ptr<PhantomCore::AudioEffect>>,
        std::less<>> effects_;
    // Only IDs for this play session. Voice ownership belongs to the Client.
    std::vector<PhantomCore::AudioPlaybackId> voices_;
    PhantomCore::AudioPlaybackId musicVoice_{PhantomCore::InvalidAudioPlaybackId};
    bool musicPositioned_{};
    bool musicPastEnd_{};
    // Aliased SoundIds share this entry by their common AudioClip address.
    // Sample playback therefore has at most one active native channel per
    // loaded sound resource.
    std::map<const PhantomCore::AudioClip*, PhantomCore::AudioPlaybackId>
        sampleVoices_;
    std::string lastError_;
    std::set<std::pair<std::string, PhantomCore::AudioEffectType>> bypassedEffects_;
    std::map<std::pair<EffectCommandType, std::string>, double> appliedValues_;
};
