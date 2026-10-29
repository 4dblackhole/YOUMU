#include "Audio/EditorAudioAnalysis.h"
#include "Audio/Submodules/AudioDecode.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace
{

    constexpr std::size_t Window = 2048;
    void FFT(std::array<std::complex<float>, Window> &values)
    {
        for (std::size_t i = 1, j = 0; i < Window; ++i)
        {
            std::size_t bit = Window >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap(values[i], values[j]);
        }
        for (std::size_t length = 2; length <= Window; length <<= 1)
        {
            const auto root = std::polar(1.0F, -2 * std::numbers::pi_v<float> / static_cast<float>(length));
            for (std::size_t i = 0; i < Window; i += length)
            {
                std::complex<float> w{1, 0};
                for (std::size_t j = 0; j < length / 2; ++j)
                {
                    const auto a = values[i + j], b = values[i + j + length / 2] * w;
                    values[i + j] = a + b;
                    values[i + j + length / 2] = a - b;
                    w *= root;
                }
            }
        }
    }
    void BuildWaveformPyramid(AudioPcm &audio, std::stop_token stop)
    {
        const auto frames = audio.samples.size() / audio.channels;
        auto &base = audio.waveform.emplace_back();
        base.reserve((frames + 63) / 64);
        for (std::size_t first = 0; first < frames && !stop.stop_requested(); first += 64)
        {
            WaveformPeak peak{audio.samples[first * audio.channels], audio.samples[first * audio.channels]};
            for (auto i = first * audio.channels; i < std::min(frames, first + 64) * audio.channels; ++i)
            {
                peak.minimum = std::min(peak.minimum, audio.samples[i]);
                peak.maximum = std::max(peak.maximum, audio.samples[i]);
            }
            base.push_back(peak);
        }
        while (audio.waveform.back().size() > 1 && !stop.stop_requested())
        {
            const auto &previous = audio.waveform.back();
            std::vector<WaveformPeak> next;
            next.reserve((previous.size() + 3) / 4);
            for (std::size_t i = 0; i < previous.size(); i += 4)
            {
                auto peak = previous[i];
                for (auto j = i + 1; j < std::min(previous.size(), i + 4); ++j)
                {
                    peak.minimum = std::min(peak.minimum, previous[j].minimum);
                    peak.maximum = std::max(peak.maximum, previous[j].maximum);
                }
                next.push_back(peak);
            }
            audio.waveform.push_back(std::move(next));
        }
    }
    SpectrumFrame AnalyzeFrame(const DecodedAudio &audio, const std::size_t start, const std::size_t hop,
                               const std::array<float, Window> &hann, const float windowSum,
                               const std::array<std::size_t, SpectrumBandCount + 1> &bins,
                               std::array<std::complex<float>, Window> &values)
    {
        SpectrumFrame frame;
        const auto sampleCount = audio.samples.size() / audio.channels;
        frame.minimum = 1;
        frame.maximum = -1;
        for (std::size_t sample = start; sample < std::min(start + hop, sampleCount); ++sample)
            for (std::uint32_t channel = 0; channel < audio.channels; ++channel)
            {
                const float value = audio.samples[sample * audio.channels + channel];
                frame.minimum = std::min(frame.minimum, value);
                frame.maximum = std::max(frame.maximum, value);
                frame.peak = std::max(frame.peak, std::abs(value));
            }

        // Average channel power, rather than summing their amplitudes:
        // opposite stereo phases must not cancel the visible spectrum.
        std::array<float, Window / 2 + 1> power{};
        for (std::uint32_t channel = 0; channel < audio.channels; ++channel)
        {
            values.fill({});
            for (std::size_t i = 0; i < Window; ++i)
            {
                const auto sample = static_cast<std::int64_t>(start) + static_cast<std::int64_t>(i) -
                                    static_cast<std::int64_t>(Window / 2);
                if (sample >= 0 && static_cast<std::size_t>(sample) < sampleCount)
                    values[i] =
                        audio.samples[static_cast<std::size_t>(sample) * audio.channels + channel] * hann[i];
            }
            FFT(values);
            for (std::size_t bin = 1; bin < power.size(); ++bin)
                power[bin] += std::norm(values[bin]);
        }
        const float normalization = 4.0F / (windowSum * windowSum * static_cast<float>(audio.channels));
        for (std::size_t band = 0; band < frame.bands.size(); ++band)
        {
            float maximumPower = 0;
            const auto last = std::min(power.size(), std::max(bins[band] + 1, bins[band + 1]));
            for (auto bin = bins[band]; bin < last; ++bin)
                maximumPower = std::max(maximumPower, power[bin]);
            const float db = 10 * std::log10(std::max(maximumPower * normalization, 1e-12F));
            frame.bands[band] = std::clamp((db - SpectrumFloorDb) / -SpectrumFloorDb, 0.0F, 1.0F);
        }
        return frame;
    }
} // namespace

AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, const std::stop_token stop, const PcmReader &read)
{
    if (stop.stop_requested())
        return {};
    AudioAnalysis result;
    auto retained = std::make_shared<AudioPcm>();
    // Allocate FFT scratch once per source; reuse it across frames/channels
    // instead of growing the worker stack or allocating on every FFT.
    const auto spectrumScratch = std::make_unique<std::array<std::complex<float>, Window>>();
    std::array<float, Window> hann{};
    float windowSum = 0;
    for (std::size_t i = 0; i < Window; ++i)
    {
        hann[i] = .5F - .5F * std::cos(2 * std::numbers::pi_v<float> * static_cast<float>(i) /
                                       static_cast<float>(Window - 1));
        windowSum += hann[i];
    }
    std::array<std::size_t, SpectrumBandCount + 1> bins{};
    std::size_t nextFrame = 0, hop = 1;
    const auto initialize = [&](const DecodedAudio &audio)
    {
        // Metadata follows each source's rate; only the FFT window is retained.
        result.sampleRate = audio.rate;
        result.maximumFrequencyHz = std::min(20'000.0, audio.rate * .5);
        result.minimumFrequencyHz = std::min(20.0, result.maximumFrequencyHz * .5);
        hop = std::max<std::size_t>(1, audio.rate / 100);
        result.secondsPerFrame = static_cast<double>(hop) / audio.rate;
        for (std::size_t band = 0; band < bins.size(); ++band)
        {
            const double frequency =
                result.minimumFrequencyHz * std::pow(result.maximumFrequencyHz / result.minimumFrequencyHz,
                                                     static_cast<double>(band) / SpectrumBandCount);
            bins[band] =
                std::clamp(static_cast<std::size_t>(frequency * Window / audio.rate), std::size_t{1}, Window / 2);
        }
    };
    const auto consume = [&](DecodedAudio &audio, bool end)
    {
        if (!result.sampleRate)
            initialize(audio);
        while (nextFrame < audio.totalFrames && !stop.stop_requested() &&
               (end || nextFrame + std::max(Window / 2, hop) <= audio.totalFrames))
        {
            result.frames.push_back(AnalyzeFrame(audio, nextFrame - audio.firstFrame, hop, hann, windowSum, bins,
                                                *spectrumScratch));
            nextFrame += hop;
        }
        // Keep the preceding half-window for the next centered FFT.
        const auto keepFrom = std::min(audio.totalFrames, nextFrame > Window / 2 ? nextFrame - Window / 2 : 0);
        if (keepFrom > audio.firstFrame)
        {
            audio.samples.erase(audio.samples.begin(),
                                audio.samples.begin() +
                                    static_cast<std::ptrdiff_t>((keepFrom - audio.firstFrame) * audio.channels));
            audio.firstFrame = keepFrom;
        }
    };
    DecodedAudio audio;
    read(path, stop, [&](std::uint32_t channels, std::uint32_t rate, std::span<const float> samples)
    {
        if (channels == 0 || rate == 0 ||
            (audio.rate != 0 && (audio.channels != channels || audio.rate != rate)))
            throw std::runtime_error("Analysis source returned an invalid or changing PCM format.");
        audio.channels = channels;
        audio.rate = rate;
        AppendPcm(audio, samples, [&](DecodedAudio &decoded) { consume(decoded, false); });
        retained->channels = channels;
        retained->sampleRate = rate;
        retained->samples.insert(retained->samples.end(), samples.begin(), samples.end());
    });
    if (stop.stop_requested())
        return {};
    if (audio.rate == 0)
        throw std::runtime_error("Analysis source returned no PCM format.");
    consume(audio, true);
    result.durationSeconds = static_cast<double>(audio.totalFrames) / audio.rate;
    BuildWaveformPyramid(*retained, stop);
    if (stop.stop_requested()) return {};
    result.pcm = std::move(retained);
    return result;
}

std::vector<WaveformPeak> BuildAudioWaveform(const AudioPcm &audio, const double begin,
                                           const double seconds, const std::size_t columns)
{
    if (!audio.channels || !audio.sampleRate || !columns || columns > 4096 ||
        !std::isfinite(begin) || !std::isfinite(seconds) || seconds <= 0)
        throw std::invalid_argument("Invalid waveform viewport.");
    std::vector<WaveformPeak> result(columns);
    const auto frames = audio.samples.size() / audio.channels;
    const auto sampleAt = [&](double time) {
        return static_cast<std::size_t>(std::clamp(std::ceil(time * audio.sampleRate), 0.0,
                                                  static_cast<double>(frames)));
    };
    for (std::size_t column = 0; column < columns; ++column)
    {
        auto first = sampleAt(begin + seconds * static_cast<double>(column) / columns);
        const auto last = sampleAt(begin + seconds * static_cast<double>(column + 1) / columns);
        if (first >= last) continue;
        auto &peak = result[column];
        peak = {audio.samples[first * audio.channels], audio.samples[first * audio.channels]};
        const auto merge = [&](WaveformPeak value) {
            peak.minimum = std::min(peak.minimum, value.minimum);
            peak.maximum = std::max(peak.maximum, value.maximum);
        };
        while (first < last)
        {
            std::size_t level = 0, block = 64;
            if (first % block == 0 && last - first >= block && !audio.waveform.empty())
            {
                while (level + 1 < audio.waveform.size() && first % (block * 4) == 0 && last - first >= block * 4)
                {
                    block *= 4;
                    ++level;
                }
                merge(audio.waveform[level][first / block]);
                first += block;
            }
            else
            {
                for (std::uint32_t channel = 0; channel < audio.channels; ++channel)
                {
                    const float value = audio.samples[first * audio.channels + channel];
                    merge({value, value});
                }
                ++first;
            }
        }
    }
    return result;
}

void AccumulateAudioSpectrum(SpectrumTile &output, const AudioPcm &audio, const SpectrumRegion &region,
                             const double offset, const std::stop_token stop)
{
    if (!audio.channels || !audio.sampleRate || !region.columns || region.columns > 4096 ||
        !region.bands || region.bands > 512 || !std::isfinite(region.begin) ||
        !std::isfinite(region.seconds) || region.seconds <= 0 || !std::isfinite(offset) ||
        !std::isfinite(region.minimumHz) || !std::isfinite(region.maximumHz) ||
        region.minimumHz <= 0 || region.maximumHz <= region.minimumHz)
        throw std::invalid_argument("Invalid spectrum viewport.");
    if (output.levels.empty())
    {
        output.columns = region.columns;
        output.bands = region.bands;
        output.levels.resize(region.columns * region.bands);
    }
    if (output.columns != region.columns || output.bands != region.bands ||
        output.levels.size() != region.columns * region.bands)
        throw std::invalid_argument("Spectrum tile dimensions do not match its viewport.");
    const auto frames = audio.samples.size() / audio.channels;
    const double duration = static_cast<double>(frames) / audio.sampleRate;
    if (offset >= region.begin + region.seconds || offset + duration <= region.begin) return;
    const auto values = std::make_unique<std::array<std::complex<float>, Window>>();
    std::array<float, Window> hann{};
    float windowSum = 0;
    for (std::size_t i = 0; i < Window; ++i)
    {
        hann[i] = .5F - .5F * std::cos(2 * std::numbers::pi_v<float> * static_cast<float>(i) / (Window - 1));
        windowSum += hann[i];
    }
    struct Band { std::size_t first{}, last{}, nearest{}; float fraction{}; bool visible{}; };
    std::vector<Band> bands(region.bands);
    for (std::size_t band = 0; band < bands.size(); ++band)
    {
        const double left = region.minimumHz * std::pow(region.maximumHz / region.minimumHz,
                                                        static_cast<double>(band) / bands.size());
        const double right = region.minimumHz * std::pow(region.maximumHz / region.minimumHz,
                                                         static_cast<double>(band + 1) / bands.size());
        const double center = std::sqrt(left * right) * Window / audio.sampleRate;
        auto &mapping = bands[band];
        mapping.visible = left < audio.sampleRate * .5;
        mapping.first = static_cast<std::size_t>(std::clamp(std::ceil(left * Window / audio.sampleRate), 1.0, Window / 2.0));
        mapping.last = static_cast<std::size_t>(std::clamp(std::ceil(right * Window / audio.sampleRate), 1.0, Window / 2.0 + 1));
        mapping.nearest = static_cast<std::size_t>(std::clamp(std::floor(center), 1.0, Window / 2.0 - 1));
        mapping.fraction = static_cast<float>(std::clamp(center - mapping.nearest, 0.0, 1.0));
    }
    const float normalization = 4.0F / (windowSum * windowSum * static_cast<float>(audio.channels));
    const auto currentPower = std::make_unique<std::array<float, Window / 2 + 1>>();
    for (std::size_t column = 0; column < region.columns && !stop.stop_requested(); ++column)
    {
        const double left = region.begin + region.seconds * static_cast<double>(column) / region.columns - offset;
        const double right = region.begin + region.seconds * static_cast<double>(column + 1) / region.columns - offset;
        if (left >= duration || right <= 0) continue;
        // Wide overview columns must not skip short attacks between FFT windows.
        const double firstSample = std::max(0.0, left) * audio.sampleRate;
        const double lastSample = std::min(duration, right) * audio.sampleRate;
        const auto windows = static_cast<std::size_t>(std::max(1.0, std::ceil((lastSample - firstSample) / (Window / 4))));
        std::array<float, Window / 2 + 1> power{};
        for (std::size_t window = 0; window < windows && !stop.stop_requested(); ++window)
        {
            const auto center = static_cast<std::int64_t>(std::llround(std::lerp(firstSample, lastSample,
                (static_cast<double>(window) + .5) / static_cast<double>(windows))));
            currentPower->fill(0);
            for (std::uint32_t channel = 0; channel < audio.channels; ++channel)
            {
                values->fill({});
                for (std::size_t i = 0; i < Window; ++i)
                {
                    const auto sample = center + static_cast<std::int64_t>(i) - static_cast<std::int64_t>(Window / 2);
                    if (sample >= 0 && static_cast<std::size_t>(sample) < frames)
                        (*values)[i] = audio.samples[static_cast<std::size_t>(sample) * audio.channels + channel] * hann[i];
                }
                FFT(*values);
                for (std::size_t bin = 1; bin < power.size(); ++bin) (*currentPower)[bin] += std::norm((*values)[bin]);
            }
            for (std::size_t bin = 1; bin < power.size(); ++bin) power[bin] = std::max(power[bin], (*currentPower)[bin]);
        }
        for (std::size_t band = 0; band < bands.size(); ++band)
        {
            const auto &mapping = bands[band];
            if (!mapping.visible) continue;
            float maximum = std::lerp(power[mapping.nearest], power[mapping.nearest + 1], mapping.fraction);
            for (auto bin = mapping.first; bin < mapping.last; ++bin) maximum = std::max(maximum, power[bin]);
            const float db = 10 * std::log10(std::max(maximum * normalization, 1e-12F));
            auto &level = output.levels[column * region.bands + band];
            level = std::max(level, std::clamp((db - SpectrumFloorDb) / -SpectrumFloorDb, 0.0F, 1.0F));
        }
    }
}
