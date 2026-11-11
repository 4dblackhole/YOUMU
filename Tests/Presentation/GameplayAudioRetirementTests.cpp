#include "Audio/GameplayAudioRouter.h"
#include "PhantomCore.h"
#include <array>
#include <iostream>
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
