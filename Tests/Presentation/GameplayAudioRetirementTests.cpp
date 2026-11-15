#include "Audio/GameplayAudioRouter.h"
#include "PhantomCore.h"
#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    enum class StopBehavior { Succeed, Fail, Throw };
    struct ClipProbe
    {
        StopBehavior stop{};
        AudioClipLoadState state{AudioClipLoadState::Ready};
        bool playing{true};
        unsigned destroyed{};
    };
    std::shared_ptr<ClipProbe> nextClip;

    void Check(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }

    class ProbeVoice final : public IAudioVoiceBackend
    {
    public:
        explicit ProbeVoice(std::shared_ptr<ClipProbe> probe) : probe_(std::move(probe)) {}
        bool IsPlaying() const noexcept override { return probe_->playing; }
        bool Restart(const AudioPlaybackSettings&, IAudioBusBackend*, std::string& error) override
        { probe_->playing = true; error.clear(); return true; }
        bool Stop(std::string& error) override
        {
            if (probe_->stop == StopBehavior::Throw)
                throw std::runtime_error("Injected stop exception");
            if (probe_->stop == StopBehavior::Fail)
            { error = "Injected stop failure"; return false; }
            probe_->playing = false;
            error.clear();
            return true;
        }
        bool SetPaused(bool, std::string& error) override { error.clear(); return true; }
        bool SetVolume(float, std::string& error) override { error.clear(); return true; }
        bool SetPitch(float, std::string& error) override { error.clear(); return true; }
        bool SetDelay(std::uint64_t, std::uint64_t, std::string& error) override
        { error.clear(); return true; }
    private:
        std::shared_ptr<ClipProbe> probe_;
    };

    class ProbeClip final : public IAudioClipBackend
    {
    public:
        explicit ProbeClip(std::shared_ptr<ClipProbe> probe) : probe_(std::move(probe)) {}
        ~ProbeClip() override { ++probe_->destroyed; }
        bool GetDurationMicroseconds(std::uint64_t& duration, std::string& error) const override
        { duration = 1'000'000; error.clear(); return true; }
        AudioClipLoadState LoadState(std::string& error) const override
        { error.clear(); return probe_->state; }
        std::unique_ptr<IAudioVoiceBackend> Play(
            const AudioPlaybackSettings&, IAudioBusBackend*, std::string& error) override
        { error.clear(); return std::make_unique<ProbeVoice>(probe_); }
    private:
        std::shared_ptr<ClipProbe> probe_;
    };

    std::unique_ptr<IAudioClipBackend> CreateClip(
        IAudioBackend&, const std::filesystem::path&, AudioLoadMode, std::string&)
    {
        return std::make_unique<ProbeClip>(nextClip);
    }

    // Bus volumes by name, as the router sets them.
    std::map<std::string, float, std::less<>> busVolumes;
    class ProbeBus final : public IAudioBusBackend
    {
    public:
        explicit ProbeBus(std::string name) : name_(std::move(name)) {}
        std::string_view Name() const noexcept override { return name_; }
        bool SetVolume(float volume, std::string& error) override
        { busVolumes[name_] = volume; error.clear(); return true; }
        bool SetPitch(float, std::string& error) override { error.clear(); return true; }
        bool SetMuted(bool, std::string& error) override { error.clear(); return true; }
        bool AddFadePoint(std::uint64_t, float, bool, std::string& error) override { error.clear(); return true; }
        bool ClearFadePoints(std::uint64_t, std::uint64_t, std::string& error) override { error.clear(); return true; }
        std::unique_ptr<IAudioEffectBackend> AddEffect(AudioEffectType, std::string& error) override
        { error = "No effects in the bus probe."; return {}; }
    private:
        std::string name_;
    };
    class BusBackend final : public IAudioBackend
    {
    public:
        bool Initialize(const AudioConfig&, std::string& error) override { error.clear(); return true; }
        void Update() override {}
        void Shutdown() noexcept override {}
        std::string_view Name() const noexcept override { return "Bus probe"; }
        AudioOutputBackend RequestedOutput() const noexcept override { return AudioOutputBackend::NoSound; }
        AudioOutputBackend ActiveOutput() const noexcept override { return AudioOutputBackend::NoSound; }
        bool SetOutputBackend(AudioOutputBackend, std::string& error) override { error.clear(); return true; }
        int DriverCount() const noexcept override { return 0; }
        const std::vector<AudioDeviceInfo>& OutputDrivers() const noexcept override { return devices_; }
        int ActiveDriverIndex() const noexcept override { return -1; }
        bool SetOutputDriver(int, std::string&) override { return false; }
        int RequestedSampleRate() const noexcept override { return 48000; }
        int SampleRate() const noexcept override { return 48000; }
        std::uint32_t DspBufferLength() const noexcept override { return 256; }
        int DspBufferCount() const noexcept override { return 4; }
        double EstimatedDspLatencyMilliseconds() const noexcept override { return 0; }
        bool SetSampleRate(int, std::string&) override { return false; }
        bool SetDspBufferSize(std::uint32_t, int, std::string&) override { return false; }
        std::uint64_t DspClock() const noexcept override { return 0; }
        std::unique_ptr<IAudioBusBackend> CreateBus(std::string_view name, IAudioBusBackend*, std::string& error) override
        { error.clear(); return std::make_unique<ProbeBus>(std::string(name)); }
    private:
        std::vector<AudioDeviceInfo> devices_;
    };
    std::unique_ptr<IAudioBackend> CreateBusBackend() { return std::make_unique<BusBackend>(); }

    // The player's music volume scales the Music bus, effects every other bus, under chart automation.
    void TestUserVolumes()
    {
        busVolumes.clear();
        AudioSystem audio;
        std::string error;
        Check(audio.Initialize({}, &CreateBusBackend, &CreateClip, error), error.c_str());
        AudioPlaybackManager playback;
        GameplayAudioRouter router(playback);
        Check(router.Initialize(audio, error), error.c_str());
        Check(busVolumes.at("Music") == 1 && busVolumes.at("HitSound") == 1, "Buses start at full volume.");
        router.SetVolumes(.5F, .25F);
        Check(busVolumes.at("Music") == .5F && busVolumes.at("HitSound") == .25F && busVolumes.at("TickSound") == .25F &&
                  busVolumes.at("UserInputFeedback") == .25F && busVolumes.at("UI") == .25F,
              "Music volume scales the Music bus and effects volume every other bus.");
        const std::array automation{AutomationValue{EffectCommandType::BusVolume, "HitSound", .8}};
        router.ApplyAutomation(automation);
        Check(busVolumes.at("HitSound") == .2F, "Chart volume automation multiplies the player's volume.");
        router.SetVolumes(1, .5F);
        Check(busVolumes.at("HitSound") == .4F && busVolumes.at("Music") == 1,
              "A new player volume keeps the bus's current automation value.");
        router.ResetAutomation();
        Check(busVolumes.at("HitSound") == .5F, "Resetting automation returns the bus to the player's volume.");
        Check(router.Initialize(audio, error) && busVolumes.at("Music") == 1 && busVolumes.at("TickSound") == .5F,
              "Buses created again keep the player's volume.");
        router.Shutdown();
        audio.Shutdown();
    }

    void TestRetirement(StopBehavior behavior)
    {
        const auto probe = std::make_shared<ClipProbe>();
        probe->stop = behavior;
        nextClip = probe;
        AudioSystem audio;
        AudioConfig config;
        config.preferredBackend = AudioOutputBackend::NoSound;
        std::string error;
        const bool initialized = audio.Initialize(config, nullptr, &CreateClip, error);
        Check(initialized, error.c_str());
        AudioPlaybackManager playback;
        GameplayAudioRouter router(playback);
        const bool routerReady = router.Initialize(audio, error);
        Check(routerReady, error.c_str());
        const std::array<SoundId, 2> aliases{"Alias.Left", "Alias.Right"};
        const bool registered = router.RegisterSoundAliases(aliases, "fake-clip.wav", error);
        Check(registered, error.c_str());
        const std::array cues{AudioCueRequest{"Alias.Left"}};
        router.PlayNow(cues);
        Check(playback.PlaybackCount() == 1, "The aliased clip must have one managed voice.");

        // Simulate an unfinished source operation when the Scene releases its aliases.
        probe->state = AudioClipLoadState::Loading;
        router.Shutdown();
        Check(playback.PlaybackCount() == (behavior == StopBehavior::Succeed ? 0U : 1U),
            "Failed or throwing stops must keep the voice owned by its manager.");
        probe->playing = false;
        playback.Update();
        Check(playback.PlaybackCount() == 0, "Ended voices must be collected.");
        audio.Update();
        Check(probe->destroyed == 0,
            "A pending clip must remain retired after the final voice releases it.");
        probe->state = AudioClipLoadState::Ready;
        audio.Update();
        Check(probe->destroyed == 1,
            "The retired aliased clip must release exactly once after preparation finishes.");
        audio.Shutdown();
        nextClip.reset();
    }
}

int main()
{
    unsigned failures{};
    try
    {
        TestUserVolumes();
        std::cout << "PASS player volumes\n";
    }
    catch (const std::exception& error)
    {
        ++failures;
        std::cerr << "FAIL player volumes: " << error.what() << '\n';
    }
    for (const auto behavior : {StopBehavior::Succeed, StopBehavior::Fail, StopBehavior::Throw})
    {
        try
        {
            TestRetirement(behavior);
            std::cout << "PASS stop behavior " << static_cast<int>(behavior) << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "FAIL stop behavior " << static_cast<int>(behavior) << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
