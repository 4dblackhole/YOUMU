// Timing measurement harness, not a pass/fail test. It plays one chart through the real
// RhythmTestScene, FMOD output and D3D12 window, records the controller's clock anchors,
// gameplay clock reads and immediate cue requests through IGameplayTimingProbe, times every
// scene Update and Render, then prints a summary and writes CSV files.
//
//   GameplayTimingProbe.exe --pattern <path substring> [--songs-root <Songs>] [--label <name>]
//       [--accelerate <0.5..2>] [--keep-pitch 0|1] [--replay] [--pause-every <s> --pause-count <n>]
//       [--minimize-at <s> --minimize-for <s>] [--max-seconds <s>] [--out <folder>]
//
// d (clock offset) = chart time from QPC - chart time of the DSP clock read in the same snapshot,
// in wall milliseconds. Positive d: the music (DSP clock) is behind the note clock.
// Only the first read of each DSP mixer block is used, the read closest to that block's mix.
#include <Windows.h>
#include "YOUMUAssets.h"
#include "App/AssetPaths.h"
#include "Catalog/SongCatalog.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "GameFlow/GameplayTimingProbe.h"
#include "GameScene/RhythmTestScene/RhythmTestScene.h"
#include "Lane/Lane.h"
#include "Mode/Submodules/PlaySession.h"
#include "Taiko/TaikoMode.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using namespace PhantomCore;

namespace PhantomCore
{
struct InputStateTestAccess
{
    static void Begin(InputState &input) { input.BeginUpdate(); }
    static void Press(InputState &input, std::uint16_t key, std::int64_t ticks) { input.SetKey(key, true, ticks); }
    static void Release(InputState &input, std::uint16_t key, std::int64_t ticks) { input.SetKey(key, false, ticks); }
};
} // namespace PhantomCore

namespace
{
    constexpr wchar_t WindowTitle[] = L"YOUMU timing probe";

    std::int64_t Qpc() noexcept
    {
        LARGE_INTEGER value{};
        QueryPerformanceCounter(&value);
        return value.QuadPart;
    }
    std::int64_t QpcFrequency() noexcept
    {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }

    struct Options
    {
        std::filesystem::path songsRoot;
        std::string pattern{"angeldream [all notes test]"};
        std::string label{"run"};
        std::filesystem::path out{"build/timing-probe"};
        double accelerate{1.0};
        bool keepPitch{true};
        bool replay{};
        double pauseEvery{};
        int pauseCount{};
        double minimizeAt{-1};
        double minimizeFor{};
        double maxSeconds{900};
    };

    struct Anchor
    {
        std::int64_t qpc{};
        std::uint64_t dsp{};
        int sampleRate{};
        std::int64_t frequency{};
        RhythmTime timeline{};
        double rate{1};
    };
    struct ClockRead
    {
        std::int64_t qpc{};
        std::uint64_t dsp{};
        RhythmTime time{};
        std::size_t anchor{};
    };
    struct CueRead
    {
        std::int64_t before{}, after{};
        RhythmTime cueTime{}, requestTime{};
        std::size_t batch{}, batchSize{};
        bool tick{};
    };
    struct Timed
    {
        std::int64_t begin{};
        float microseconds{};
        RhythmTime time{};
    };
    struct PerformanceRead
    {
        RhythmTime time{};
        std::uint64_t fps{}, ups{}, drawCalls{}, peakDrawCalls{};
    };
    struct BuzzSection
    {
        RhythmTime begin{}, end{};
        std::size_t ticks{};
    };
    struct PlannedInput
    {
        RhythmTime time{};
        std::uint16_t key{};
        bool press{};
    };

    // Everything is reported by the controller on the update thread; nothing here feeds back.
    class Recorder final : public IGameplayTimingProbe
    {
      public:
        Recorder()
        {
            reads.reserve(1 << 20);
            cues.reserve(1 << 18);
        }
        void OnClockAnchored(const AudioClockSnapshot &anchor, RhythmTime timeline, double rate) noexcept override
        {
            anchors.push_back({anchor.performanceCounterTicks, anchor.dspClock, anchor.sampleRate,
                               anchor.performanceCounterFrequency, timeline, rate});
            lastDsp_.reset();
        }
        void OnGameplayClock(const AudioClockSnapshot &clock, RhythmTime time) noexcept override
        {
            running = true;
            lastTime = time;
            ++clockReads;
            if (anchors.empty() || lastDsp_ == clock.dspClock)
                return;
            lastDsp_ = clock.dspClock;
            reads.push_back({clock.performanceCounterTicks, clock.dspClock, time, anchors.size() - 1});
        }
        void BeforeImmediateCues(std::span<const AudioCueRequest> requests) noexcept override
        {
            if (!requests.empty())
                batchBegin_ = Qpc();
        }
        void AfterImmediateCues(std::span<const AudioCueRequest> requests) noexcept override
        {
            if (requests.empty() || anchors.empty())
                return;
            const auto after = Qpc();
            const auto requestTime = TimelineAt(batchBegin_);
            for (const auto &cue : requests)
                cues.push_back({batchBegin_, after, cue.timelineTime, requestTime, batches, requests.size(),
                                cue.bus == "TickSound"});
            ++batches;
        }
        [[nodiscard]] RhythmTime TimelineAt(std::int64_t qpc) const noexcept
        {
            const auto &anchor = anchors.back();
            const long double seconds = static_cast<long double>(qpc - anchor.qpc) / anchor.frequency;
            return anchor.timeline + RhythmDuration{std::llround(seconds * 1'000'000.0L * anchor.rate)};
        }
        [[nodiscard]] std::int64_t QpcAt(RhythmTime time) const noexcept
        {
            const auto &anchor = anchors.back();
            const long double seconds =
                static_cast<long double>((time - anchor.timeline).count()) / (1'000'000.0L * anchor.rate);
            return anchor.qpc + static_cast<std::int64_t>(std::llround(seconds * anchor.frequency));
        }

        std::vector<Anchor> anchors;
        std::vector<ClockRead> reads;
        std::vector<CueRead> cues;
        std::uint64_t clockReads{};
        std::size_t batches{};
        RhythmTime lastTime{};
        bool running{};

      private:
        std::optional<std::uint64_t> lastDsp_;
        std::int64_t batchBegin_{};
    };

    // Updates run unthrottled (about 10^5 per second), so they are aggregated per 10 ms of chart
    // time into storage allocated before play; growing a vector during play stalls the loop itself.
    struct UpdateBucket
    {
        std::uint32_t count{};
        float maxMicroseconds{}, maxLoopMicroseconds{};
        double sumMicroseconds{};
    };
    // A main-loop iteration of at least StallMicroseconds, split into the engine's phases around the
    // client calls: before Render (FMOD service, BeginFrame wait), after Render (Present, messages).
    struct Stall
    {
        RhythmTime time{};
        float microseconds{}, updateMicroseconds{}, beforeRenderMicroseconds{}, renderMicroseconds{},
            afterMicroseconds{};
        bool rendered{};
    };
    constexpr float StallMicroseconds = 5000;
    constexpr RhythmTime BucketOrigin{-5'000'000};
    constexpr RhythmDuration BucketWidth{10'000};

    struct Results
    {
        Options options;
        Recorder recorder;
        std::vector<UpdateBucket> updates;
        std::vector<Timed> renders;
        std::vector<Stall> stalls;
        std::vector<PerformanceRead> performance;
        std::vector<BuzzSection> buzz;
        std::string song, output;
        int sampleRate{};
        std::uint32_t bufferLength{};
        int bufferCount{};
        double latencyMs{};
        RhythmTime end{};
        bool failed{};
        std::string error;
    };

    std::uint16_t KeyFor(NoteVisualKind kind)
    {
        const bool kat = kind == NoteVisualKind::Kat || kind == NoteVisualKind::BigKat;
        return kat || kind == NoteVisualKind::KatBuzz ? 'D' : 'F';
    }

    // Presses every note on time, holds each Buzz from its head to its end, and taps every tick.
    std::vector<PlannedInput> PlanInputs(const PlaySession &session, std::vector<BuzzSection> &buzz, RhythmTime &end)
    {
        std::vector<PlannedInput> inputs;
        const auto tap = [&inputs](RhythmTime time, std::uint16_t key) {
            inputs.push_back({time, key, true});
            inputs.push_back({time + RhythmDuration{1}, key, false});
        };
        for (const auto &lane : session.Gear().Lanes())
            for (const auto &note : lane->Notes())
            {
                const auto *view = session.FindNotePresentation(note->Id());
                if (view == nullptr)
                    continue;
                const auto key = KeyFor(view->visualKind);
                const auto &target = note->Accuracy().target;
                end = std::max(end, view->hasEndTime ? view->endTime : note->Timing());
                if (target.kind == NoteAccuracyKind::Hold)
                {
                    buzz.push_back({note->Timing(), view->endTime, view->tickTimes.size()});
                    inputs.push_back({note->Timing(), key, true});
                    inputs.push_back({view->endTime + RhythmDuration{1}, key, false});
                }
                else if (target.kind == NoteAccuracyKind::Ticks)
                {
                    for (const auto tick : view->tickTimes)
                        tap(tick, key);
                }
                else if (target.kind == NoteAccuracyKind::HitCount)
                {
                    for (std::size_t hit = 0; hit < target.hits; ++hit)
                    {
                        const bool alternate = view->visualKind == NoteVisualKind::DengDeng && hit % 2 != 0;
                        tap(note->Timing() + RhythmDuration{static_cast<long long>(hit) * 80'000},
                            alternate ? std::uint16_t{'D'} : std::uint16_t{'F'});
                    }
                }
                else
                {
                    tap(note->Timing(), key);
                    if (target.hits == 2)
                        tap(note->Timing(), key == 'F' ? std::uint16_t{'J'} : std::uint16_t{'K'});
                }
            }
        std::ranges::stable_sort(inputs, {}, &PlannedInput::time);
        return inputs;
    }

    class ProbeClient final : public IGameClient
    {
      public:
        explicit ProbeClient(Results &results) : results_(results), options_(results.options) {}

        EngineConfig GetEngineConfig() const override
        {
            EngineConfig config;
            config.windowTitle = WindowTitle;
            config.windowWidth = 1600;
            config.windowHeight = 900;
            config.showWindow = true;
            config.audio.allowNoSoundFallback = false;
            return config;
        }

        void Initialize(const EngineServices &services) override
        {
            results_.sampleRate = services.audio.SampleRate();
            results_.bufferLength = services.audio.DspBufferLength();
            results_.bufferCount = services.audio.DspBufferCount();
            results_.latencyMs = services.audio.EstimatedDspLatencyMilliseconds();
            visuals_.Initialize(services.visual2DRendering,
                                {static_cast<float>(services.windowWidth), static_cast<float>(services.windowHeight)});
            const auto catalog = SongCatalog{}.Load(options_.songsRoot);
            const SongCatalogEntry *song = nullptr;
            const SongCatalogPattern *pattern = nullptr;
            for (const auto &entry : catalog.songs)
                for (const auto &candidate : entry.patterns)
                    if (!pattern && candidate.patternPath.u8string().find(
                                        std::u8string{options_.pattern.begin(), options_.pattern.end()}) !=
                                        std::u8string::npos)
                    {
                        song = &entry;
                        pattern = &candidate;
                    }
            if (pattern == nullptr)
                throw std::runtime_error("No catalog pattern path contains --pattern.");
            const auto patternPath = pattern->patternPath.u8string();
            results_.song.assign(patternPath.begin(), patternPath.end());
            PlaySessionOptions sessionOptions;
            sessionOptions.play.accelerate = options_.accelerate;
            sessionOptions.play.preserveMusicPitch = options_.keepPitch;
            auto loaded = TaikoMode(sessionOptions).LoadSession(pattern->patternPath, pattern->effectPath);
            if (!loaded.Succeeded())
                throw std::runtime_error("The pattern did not load.");
            auto planned = PlanInputs(*loaded.session, results_.buzz, results_.end);
            if (options_.replay)
                inputs_ = std::move(planned);
            const auto span = results_.end + RhythmDuration{10'000'000} - BucketOrigin;
            results_.updates.resize(static_cast<std::size_t>(span / BucketWidth) + 1);
            results_.renders.reserve(std::size_t{1} << 22);
            results_.stalls.reserve(std::size_t{1} << 16);
            GameplayLaunchRequest request{pattern->patternPath, pattern->effectPath, song->audioPath, "Taiko"};
            request.songsRoot = options_.songsRoot;
            request.playOptions = sessionOptions.play;
            request.timingProbe = &results_.recorder;
            auto store = std::make_shared<GameplayLaunchStore>();
            store->SetValidated(request, std::move(loaded.session));
            scene_ = std::make_unique<RhythmTestScene>(store, playback_, visuals_, false, texts_, result_);
            scene_->Initialize(services);
            scene_->BeginScene();
        }

        bool Update(const UpdateContext &ctx) override
        {
            auto &recorder = results_.recorder;
            const auto now = Qpc();
            RecordIteration(now);
            InputStateTestAccess::Begin(input_);
            DriveInputs(now);
            DrivePauses(now);
            DriveWindow(now);
            const auto begin = Qpc();
            scene_->Update({ctx.deltaSeconds, ctx.totalSeconds, ctx.updateIndex, input_, ctx.audio, ctx.performance},
                           scenes_);
            const auto end = Qpc();
            playback_.Update();
            visuals_.Update(ctx.deltaSeconds);
            if (recorder.running)
            {
                auto &bucket = Bucket(recorder.lastTime);
                const float microseconds = Microseconds(end - begin);
                ++bucket.count;
                bucket.sumMicroseconds += microseconds;
                bucket.maxMicroseconds = std::max(bucket.maxMicroseconds, microseconds);
            }
            iteration_ = {now, begin, end, Qpc(), 0, 0, recorder.lastTime, recorder.running};
            if (ctx.performance.hasMeasurement && ctx.performance.measurementIndex != lastMeasurement_)
            {
                lastMeasurement_ = ctx.performance.measurementIndex;
                if (recorder.running)
                    results_.performance.push_back({recorder.lastTime, ctx.performance.framesPerSecond,
                                                    ctx.performance.updatesPerSecond,
                                                    ctx.performance.drawCallsPerFrame,
                                                    ctx.performance.peakDrawCallsPerFrame});
            }
            if (ctx.totalSeconds > options_.maxSeconds)
                return false;
            return !Finished();
        }

        void Render(const RenderContext &ctx) override
        {
            const auto begin = Qpc();
            scene_->Render(ctx);
            visuals_.Render(ctx);
            const auto end = Qpc();
            iteration_.renderBegin = begin;
            iteration_.renderEnd = end;
            if (results_.recorder.running && results_.renders.size() < results_.renders.capacity())
                results_.renders.push_back({begin, Microseconds(end - begin), results_.recorder.lastTime});
        }
        void OnResize(std::uint32_t width, std::uint32_t height) override
        {
            visuals_.OnResize(width, height);
            if (scene_)
                scene_->OnResize(width, height);
        }
        void Shutdown() noexcept override
        {
            if (scene_)
                scene_->Shutdown();
            scene_.reset();
            playback_.StopAll();
            visuals_.Shutdown();
        }

      private:
        // QPC marks of one main-loop iteration, from this client's Update start to the next one.
        struct Iteration
        {
            std::int64_t start{}, updateBegin{}, updateEnd{}, end{}, renderBegin{}, renderEnd{};
            RhythmTime time{};
            bool running{};
        };

        [[nodiscard]] float Microseconds(std::int64_t ticks) const noexcept
        {
            return static_cast<float>(static_cast<double>(ticks) * 1'000'000.0 / frequency_);
        }
        [[nodiscard]] UpdateBucket &Bucket(RhythmTime time) noexcept
        {
            const auto index = std::clamp<std::int64_t>((time - BucketOrigin) / BucketWidth, 0,
                                                        static_cast<std::int64_t>(results_.updates.size()) - 1);
            return results_.updates[static_cast<std::size_t>(index)];
        }
        // Scores the previous iteration now that the next Update has started.
        void RecordIteration(std::int64_t now)
        {
            const auto &last = iteration_;
            if (last.start == 0 || !last.running)
                return;
            const float loop = Microseconds(now - last.start);
            auto &bucket = Bucket(last.time);
            bucket.maxLoopMicroseconds = std::max(bucket.maxLoopMicroseconds, loop);
            if (loop < StallMicroseconds || results_.stalls.size() == results_.stalls.capacity())
                return;
            Stall stall{last.time, loop, Microseconds(last.updateEnd - last.updateBegin)};
            stall.rendered = last.renderBegin != 0;
            if (stall.rendered)
            {
                stall.beforeRenderMicroseconds = Microseconds(last.renderBegin - last.end);
                stall.renderMicroseconds = Microseconds(last.renderEnd - last.renderBegin);
                stall.afterMicroseconds = Microseconds(now - last.renderEnd);
            }
            else
                stall.afterMicroseconds = Microseconds(now - last.end);
            results_.stalls.push_back(stall);
        }
        [[nodiscard]] bool Finished() const noexcept
        {
            const auto &recorder = results_.recorder;
            if (!recorder.running || pausesDone_ < options_.pauseCount)
                return false;
            return recorder.lastTime > results_.end + RhythmDuration{1'000'000};
        }
        void DriveInputs(std::int64_t now)
        {
            const auto &recorder = results_.recorder;
            if (!recorder.running || recorder.anchors.empty())
                return;
            const auto time = recorder.TimelineAt(now);
            for (; nextInput_ < inputs_.size() && inputs_[nextInput_].time <= time; ++nextInput_)
            {
                const auto &input = inputs_[nextInput_];
                const auto ticks = std::min(recorder.QpcAt(input.time), now);
                if (input.press)
                    InputStateTestAccess::Press(input_, input.key, ticks);
                else
                    InputStateTestAccess::Release(input_, input.key, ticks);
            }
        }
        // ESC opens the pause menu, Enter resumes with its default lead-in; the next anchor
        // marks the restarted clocks. Pauses are spaced in wall time after each anchor.
        void DrivePauses(std::int64_t now)
        {
            auto &recorder = results_.recorder;
            if (releaseKey_)
            {
                InputStateTestAccess::Release(input_, *releaseKey_, now);
                releaseKey_.reset();
                return;
            }
            if (pausesDone_ >= options_.pauseCount || recorder.anchors.empty())
                return;
            const double sinceAnchor = static_cast<double>(now - recorder.anchors.back().qpc) / frequency_;
            if (pausedAt_ == 0 && recorder.running && sinceAnchor >= options_.pauseEvery)
            {
                InputStateTestAccess::Press(input_, VK_ESCAPE, now);
                releaseKey_ = std::uint16_t{VK_ESCAPE};
                recorder.running = false;
                pausedAt_ = now;
                return;
            }
            if (pausedAt_ != 0 && static_cast<double>(now - pausedAt_) / frequency_ >= 0.3)
            {
                InputStateTestAccess::Press(input_, VK_RETURN, now);
                releaseKey_ = std::uint16_t{VK_RETURN};
                pausedAt_ = 0;
                ++pausesDone_;
            }
        }
        void DriveWindow(std::int64_t now)
        {
            const auto &recorder = results_.recorder;
            if (options_.minimizeAt < 0 || recorder.anchors.empty() || windowDone_)
                return;
            const double since = static_cast<double>(now - recorder.anchors.front().qpc) / frequency_;
            HWND window = FindWindowW(nullptr, WindowTitle);
            if (!minimized_ && since >= options_.minimizeAt)
            {
                ShowWindow(window, SW_MINIMIZE);
                minimized_ = true;
            }
            else if (minimized_ && since >= options_.minimizeAt + options_.minimizeFor)
            {
                ShowWindow(window, SW_RESTORE);
                SetForegroundWindow(window);
                windowDone_ = true;
            }
        }

        Results &results_;
        const Options &options_;
        const double frequency_{static_cast<double>(QpcFrequency())};
        std::vector<PlannedInput> inputs_;
        std::size_t nextInput_{};
        std::optional<std::uint16_t> releaseKey_;
        std::int64_t pausedAt_{};
        int pausesDone_{};
        bool minimized_{}, windowDone_{};
        std::uint64_t lastMeasurement_{};
        Iteration iteration_;
        InputState input_;
        ScreenVisual2DManager visuals_;
        AudioPlaybackManager playback_;
        SceneManager scenes_;
        TextCatalog texts_;
        std::unique_ptr<IPlayResult> result_;
        std::unique_ptr<RhythmTestScene> scene_;
    };

    // ---- Summary ----

    double Percentile(std::vector<double> values, double fraction)
    {
        if (values.empty())
            return NAN;
        std::ranges::sort(values);
        const auto index = static_cast<std::size_t>(std::llround(fraction * static_cast<double>(values.size() - 1)));
        return values[index];
    }
    double Maximum(const std::vector<double> &values)
    {
        return values.empty() ? NAN : *std::ranges::max_element(values);
    }

    // Wall milliseconds by which the note clock is ahead of the DSP clock at this read.
    double OffsetMs(const Anchor &anchor, const ClockRead &read)
    {
        const long double dspSeconds =
            (static_cast<long double>(read.dsp) - static_cast<long double>(anchor.dsp)) / anchor.sampleRate;
        const long double qpcSeconds = static_cast<long double>(read.qpc - anchor.qpc) / anchor.frequency;
        return static_cast<double>((qpcSeconds - dspSeconds) * 1000.0L);
    }
    double SecondsSince(const Anchor &anchor, const ClockRead &read)
    {
        return static_cast<double>(read.qpc - anchor.qpc) / static_cast<double>(anchor.frequency);
    }

    struct Segment
    {
        std::size_t anchor{};
        double seconds{};
        double initialMedian{NAN}, initialP10{NAN}, initialP90{NAN};
        double start{NAN}, middle{NAN}, end{NAN}, slopePpm{NAN};
        std::size_t reads{};
    };

    std::vector<double> OffsetsBetween(const Results &results, std::size_t anchorIndex, double from, double to)
    {
        std::vector<double> values;
        const auto &anchor = results.recorder.anchors[anchorIndex];
        for (const auto &read : results.recorder.reads)
        {
            if (read.anchor != anchorIndex)
                continue;
            const double seconds = SecondsSince(anchor, read);
            if (seconds >= from && seconds < to)
                values.push_back(OffsetMs(anchor, read));
        }
        return values;
    }

    Segment Analyse(const Results &results, std::size_t anchorIndex)
    {
        Segment segment{anchorIndex};
        const auto &anchor = results.recorder.anchors[anchorIndex];
        double sumX{}, sumY{}, sumXX{}, sumXY{};
        for (const auto &read : results.recorder.reads)
        {
            if (read.anchor != anchorIndex)
                continue;
            const double x = SecondsSince(anchor, read);
            if (x < 0.5)
                continue;
            const double y = OffsetMs(anchor, read);
            ++segment.reads;
            segment.seconds = std::max(segment.seconds, x);
            sumX += x;
            sumY += y;
            sumXX += x * x;
            sumXY += x * y;
        }
        const auto initial = OffsetsBetween(results, anchorIndex, 0.5, 3.5);
        segment.initialMedian = Percentile(initial, 0.5);
        segment.initialP10 = Percentile(initial, 0.1);
        segment.initialP90 = Percentile(initial, 0.9);
        if (segment.seconds >= 20)
        {
            const double n = static_cast<double>(segment.reads);
            segment.slopePpm = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX) * 1000.0;
            segment.start = Percentile(OffsetsBetween(results, anchorIndex, 1, 6), 0.5);
            const double middle = segment.seconds * 0.5;
            segment.middle = Percentile(OffsetsBetween(results, anchorIndex, middle - 2.5, middle + 2.5), 0.5);
            segment.end = Percentile(OffsetsBetween(results, anchorIndex, segment.seconds - 5, segment.seconds), 0.5);
        }
        return segment;
    }

    void Print(std::FILE *file, const char *format, auto... values)
    {
        std::fprintf(stdout, format, values...);
        if (file)
            std::fprintf(file, format, values...);
    }

    void WriteCsv(const Results &results, const std::filesystem::path &folder)
    {
        std::filesystem::create_directories(folder);
        const auto &recorder = results.recorder;
        std::ofstream anchors(folder / "anchors.csv");
        anchors << "index,qpc,dsp,sample_rate,frequency,timeline_us,rate\n";
        for (std::size_t i = 0; i < recorder.anchors.size(); ++i)
        {
            const auto &a = recorder.anchors[i];
            anchors << i << ',' << a.qpc << ',' << a.dsp << ',' << a.sampleRate << ',' << a.frequency << ','
                    << a.timeline.count() << ',' << a.rate << '\n';
        }
        std::ofstream clock(folder / "clock.csv");
        clock << "anchor,seconds,qpc,dsp,time_us,offset_ms\n";
        for (const auto &read : recorder.reads)
        {
            const auto &anchor = recorder.anchors[read.anchor];
            clock << read.anchor << ',' << SecondsSince(anchor, read) << ',' << read.qpc << ',' << read.dsp << ','
                  << read.time.count() << ',' << OffsetMs(anchor, read) << '\n';
        }
        std::ofstream cues(folder / "cues.csv");
        cues << "batch,batch_size,tick,cue_us,request_us,late_ms,route_us\n";
        const double frequency = recorder.anchors.empty() ? 1 : static_cast<double>(recorder.anchors.front().frequency);
        for (const auto &cue : recorder.cues)
            cues << cue.batch << ',' << cue.batchSize << ',' << cue.tick << ',' << cue.cueTime.count() << ','
                 << cue.requestTime.count() << ',' << (cue.requestTime - cue.cueTime).count() / 1000.0 << ','
                 << static_cast<double>(cue.after - cue.before) * 1e6 / frequency << '\n';
        std::ofstream frames(folder / "frames.csv");
        frames << "kind,qpc,time_us,duration_us\n";
        for (const auto &render : results.renders)
            frames << "render," << render.begin << ',' << render.time.count() << ',' << render.microseconds << '\n';
        std::ofstream updates(folder / "updates.csv");
        updates << "time_us,count,mean_us,max_us,max_loop_us\n";
        for (std::size_t i = 0; i < results.updates.size(); ++i)
        {
            const auto &bucket = results.updates[i];
            if (bucket.count == 0)
                continue;
            updates << (BucketOrigin + BucketWidth * static_cast<long long>(i)).count() << ',' << bucket.count << ','
                    << bucket.sumMicroseconds / bucket.count << ',' << bucket.maxMicroseconds << ','
                    << bucket.maxLoopMicroseconds << '\n';
        }
        std::ofstream stalls(folder / "stalls.csv");
        stalls << "time_us,loop_us,update_us,rendered,before_render_us,render_us,after_us\n";
        for (const auto &stall : results.stalls)
            stalls << stall.time.count() << ',' << stall.microseconds << ',' << stall.updateMicroseconds << ','
                   << stall.rendered << ',' << stall.beforeRenderMicroseconds << ',' << stall.renderMicroseconds << ','
                   << stall.afterMicroseconds << '\n';
        std::ofstream performance(folder / "performance.csv");
        performance << "time_us,fps,ups,draw_calls,peak_draw_calls\n";
        for (const auto &read : results.performance)
            performance << read.time.count() << ',' << read.fps << ',' << read.ups << ',' << read.drawCalls << ','
                        << read.peakDrawCalls << '\n';
    }

    // A chart-time window, scored in wall time.
    void PrintWindow(std::FILE *file, const Results &results, const char *name, RhythmTime from, RhythmTime to,
                     std::size_t ticks)
    {
        const auto &recorder = results.recorder;
        const double rate = recorder.anchors.empty() ? 1 : recorder.anchors.front().rate;
        const double frequency = recorder.anchors.empty() ? 1 : static_cast<double>(recorder.anchors.front().frequency);
        const double seconds = static_cast<double>((to - from).count()) / 1e6 / rate;
        std::vector<double> late, route, render, interval;
        double updates{}, updateSum{}, updateMax{}, loopMax{};
        std::size_t tickCues{};
        std::size_t lastBatch = SIZE_MAX;
        for (const auto &cue : recorder.cues)
        {
            if (cue.cueTime < from || cue.cueTime >= to || !cue.tick)
                continue;
            ++tickCues;
            late.push_back(static_cast<double>((cue.requestTime - cue.cueTime).count()) / 1000.0 / rate);
            if (cue.batch != lastBatch)
                route.push_back(static_cast<double>(cue.after - cue.before) * 1000.0 / frequency);
            lastBatch = cue.batch;
        }
        // Loop: from one Update's start to the next, including Render, FMOD service and messages.
        for (std::size_t i = 0; i < results.updates.size(); ++i)
        {
            const auto time = BucketOrigin + BucketWidth * static_cast<long long>(i);
            if (time < from || time >= to)
                continue;
            const auto &bucket = results.updates[i];
            updates += bucket.count;
            updateSum += bucket.sumMicroseconds / 1000.0;
            updateMax = std::max(updateMax, static_cast<double>(bucket.maxMicroseconds) / 1000.0);
            loopMax = std::max(loopMax, static_cast<double>(bucket.maxLoopMicroseconds) / 1000.0);
        }
        std::int64_t previous{};
        for (const auto &entry : results.renders)
        {
            if (entry.time < from || entry.time >= to)
            {
                previous = 0;
                continue;
            }
            render.push_back(entry.microseconds / 1000.0);
            if (previous != 0)
                interval.push_back(static_cast<double>(entry.begin - previous) * 1000.0 / frequency);
            previous = entry.begin;
        }
        Print(file,
              "%-22s ticks %4zu/%4zu | tick late ms med %6.2f p95 %6.2f max %6.2f | route ms max %6.3f | "
              "UPS %7.0f upd ms mean %6.3f max %6.2f | loop ms max %6.2f | FPS %6.1f render ms p99 %6.2f max %6.2f | "
              "frame gap ms max %6.2f\n",
              name, tickCues, ticks, Percentile(late, 0.5), Percentile(late, 0.95), Maximum(late), Maximum(route),
              updates / seconds, updates > 0 ? updateSum / updates : NAN, updateMax, loopMax,
              static_cast<double>(render.size()) / seconds, Percentile(render, 0.99), Maximum(render),
              Maximum(interval));
    }

    // Main-loop iterations of 5 ms or more, longest first, with the phase that took the time.
    void PrintStalls(std::FILE *file, const Results &results)
    {
        auto stalls = results.stalls;
        std::ranges::sort(stalls, std::greater{}, &Stall::microseconds);
        Print(file, "\n[main-loop stalls >= 5 ms: %zu]  (before render = FMOD service + BeginFrame wait; "
                    "after = Present + messages, or everything after Update when nothing rendered)\n",
              stalls.size());
        for (std::size_t i = 0; i < std::min<std::size_t>(stalls.size(), 12); ++i)
        {
            const auto &stall = stalls[i];
            Print(file, "  at %8.3f s: loop %7.2f ms | update %6.2f | before render %6.2f | render %6.2f | after %6.2f%s\n",
                  stall.time.count() / 1e6, stall.microseconds / 1000.0, stall.updateMicroseconds / 1000.0,
                  stall.beforeRenderMicroseconds / 1000.0, stall.renderMicroseconds / 1000.0,
                  stall.afterMicroseconds / 1000.0, stall.rendered ? "" : " (no render)");
        }
    }

    void Summarize(const Results &results)
    {
        const auto folder = results.options.out / results.options.label;
        WriteCsv(results, folder);
        std::FILE *file{};
        fopen_s(&file, (folder / "summary.txt").string().c_str(), "w");
        const auto &recorder = results.recorder;
#if defined(_DEBUG)
        const char *configuration = "Debug";
#else
        const char *configuration = "Release";
#endif
        Print(file, "label %s | %s | %s\n", results.options.label.c_str(), configuration, results.song.c_str());
        Print(file, "accelerate %.2f keep-pitch %d replay %d | mixer %d Hz, buffer %u x %d, estimated latency %.2f ms\n",
              results.options.accelerate, results.options.keepPitch, results.options.replay, results.sampleRate,
              results.bufferLength, results.bufferCount, results.latencyMs);
        Print(file, "clock reads %llu, first-in-block reads %zu, anchors %zu, cues %zu\n",
              static_cast<unsigned long long>(recorder.clockReads), recorder.reads.size(), recorder.anchors.size(),
              recorder.cues.size());
        Print(file, "\n[clock offset d = QPC chart time - DSP chart time, wall ms]\n");
        std::vector<double> initials;
        for (std::size_t i = 0; i < recorder.anchors.size(); ++i)
        {
            const auto segment = Analyse(results, i);
            if (segment.reads == 0)
                continue;
            initials.push_back(segment.initialMedian);
            Print(file, "anchor %2zu at %8.3f s: %6.1f s, initial med %6.3f (p10 %6.3f p90 %6.3f)",
                  i, recorder.anchors[i].timeline.count() / 1e6, segment.seconds, segment.initialMedian,
                  segment.initialP10, segment.initialP90);
            if (!std::isnan(segment.slopePpm))
                Print(file, " | start %6.3f middle %6.3f end %6.3f | slope %+7.2f ppm (%+6.2f ms per 3 min)",
                      segment.start, segment.middle, segment.end, segment.slopePpm, segment.slopePpm * 0.18);
            Print(file, "\n");
        }
        if (initials.size() > 1)
            Print(file, "initial offsets over %zu anchors: min %6.3f med %6.3f max %6.3f spread %6.3f ms\n",
                  initials.size(), *std::ranges::min_element(initials), Percentile(initials, 0.5),
                  *std::ranges::max_element(initials),
                  *std::ranges::max_element(initials) - *std::ranges::min_element(initials));
        Print(file, "\n[windows: tick sounds requested late = request chart time - tick chart time, wall ms]\n");
        if (!results.buzz.empty())
        {
            const auto first = results.buzz.front().begin;
            PrintWindow(file, results, "before first buzz", first - RhythmDuration{3'000'000}, first, 0);
        }
        for (const auto &buzz : results.buzz)
        {
            char name[64];
            std::snprintf(name, sizeof name, "buzz %7.3f s", buzz.begin.count() / 1e6);
            PrintWindow(file, results, name, buzz.begin, buzz.end + RhythmDuration{1}, buzz.ticks);
        }
        PrintWindow(file, results, "whole chart", RhythmTime{}, results.end, 0);
        PrintStalls(file, results);
        if (file)
            std::fclose(file);
        std::printf("CSV and summary: %s\n", folder.string().c_str());
    }

    Options ParseOptions(int argc, char **argv)
    {
        Options options;
        options.songsRoot = UserSongs();
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view name = argv[i];
            const auto value = [&]() -> std::string {
                if (i + 1 >= argc)
                    throw std::invalid_argument("Missing value after " + std::string(name));
                return argv[++i];
            };
            if (name == "--pattern")
                options.pattern = value();
            else if (name == "--songs-root")
            {
                const auto text = value();
                options.songsRoot = std::filesystem::path(std::u8string(text.begin(), text.end()));
            }
            else if (name == "--label")
                options.label = value();
            else if (name == "--out")
                options.out = value();
            else if (name == "--accelerate")
                options.accelerate = std::stod(value());
            else if (name == "--keep-pitch")
                options.keepPitch = value() != "0";
            else if (name == "--replay")
                options.replay = true;
            else if (name == "--pause-every")
                options.pauseEvery = std::stod(value());
            else if (name == "--pause-count")
                options.pauseCount = std::stoi(value());
            else if (name == "--minimize-at")
                options.minimizeAt = std::stod(value());
            else if (name == "--minimize-for")
                options.minimizeFor = std::stod(value());
            else if (name == "--max-seconds")
                options.maxSeconds = std::stod(value());
            else
                throw std::invalid_argument("Unknown option " + std::string(name));
        }
        return options;
    }
} // namespace

int main(int argc, char **argv)
{
    try
    {
        std::string assetError;
        if (!InitializeBuiltInAssets(assetError))
            throw std::runtime_error(assetError);
        Results results;
        results.options = ParseOptions(argc, argv);
        const int code = Run(std::make_unique<ProbeClient>(results));
        if (code != 0)
            std::fprintf(stderr, "Engine run returned %d\n", code);
        Summarize(results);
        return code;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "Timing probe failed: %s\n", error.what());
        return 1;
    }
}
