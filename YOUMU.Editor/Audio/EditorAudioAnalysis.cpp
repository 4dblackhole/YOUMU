#include "Audio/EditorAudioAnalysis.h"
#include "Audio/Submodules/AudioDecode.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace
{

    // The overview's window and the longest detail window. Detail windows shrink with the
    // zoom down to MinimumWindow; every size is a power of two between the two.
    constexpr std::size_t Window = 2048;
    constexpr std::size_t MinimumWindow = 32;
    constexpr std::size_t WindowSizeCount = 7;

    struct SpectrumPlan
    {
        std::size_t size{};
        std::vector<std::size_t> reversed;
        std::vector<std::complex<float>> twiddles;
        std::vector<float> hann;
        float windowSum{};

        explicit SpectrumPlan(const std::size_t points)
            : size(points), reversed(points), twiddles(points - 1), hann(points)
        {
            for (std::size_t i = 1, j = 0; i < size; ++i)
            {
                std::size_t bit = size >> 1;
                for (; j & bit; bit >>= 1)
                    j ^= bit;
                j ^= bit;
                reversed[i] = j;
            }
            // Retain the original coefficient recurrence and its rounding.
            // Every block in a stage uses this same sequence.
            for (std::size_t length = 2; length <= size; length <<= 1)
            {
                const auto root = std::polar(1.0F, -2 * std::numbers::pi_v<float> / static_cast<float>(length));
                std::complex<float> w{1, 0};
                for (std::size_t j = 0; j < length / 2; ++j)
                {
                    twiddles[length / 2 - 1 + j] = w;
                    w *= root;
                }
            }
            for (std::size_t i = 0; i < size; ++i)
            {
                hann[i] = .5F - .5F * std::cos(2 * std::numbers::pi_v<float> * static_cast<float>(i) /
                                             static_cast<float>(size - 1));
                windowSum += hann[i];
            }
        }
    };

    [[nodiscard]] std::size_t WindowIndex(const std::size_t size) noexcept
    {
        return static_cast<std::size_t>(std::countr_zero(size) - std::countr_zero(MinimumWindow));
    }

    const SpectrumPlan &GetSpectrumPlan(const std::size_t size)
    {
        static const std::array<SpectrumPlan, WindowSizeCount> plans{
            SpectrumPlan{32}, SpectrumPlan{64}, SpectrumPlan{128}, SpectrumPlan{256},
            SpectrumPlan{512}, SpectrumPlan{1024}, SpectrumPlan{Window}};
        return plans[WindowIndex(size)];
    }

    // `values` holds plan.size complex numbers.
    void FFT(const SpectrumPlan &plan, std::complex<float> *const values)
    {
        // [complex.numbers.general] guarantees interleaved array access to the
        // real and imaginary components. Avoid tiny complex/array calls in the
        // Debug inner loop while retaining the same scalar operation order.
        auto *const data = reinterpret_cast<float *>(values);
        const auto *const reversed = plan.reversed.data();
        const auto size = plan.size;
        for (std::size_t i = 1; i < size; ++i)
            if (i < reversed[i])
            {
                std::swap(data[2 * i], data[2 * reversed[i]]);
                std::swap(data[2 * i + 1], data[2 * reversed[i] + 1]);
            }
        for (std::size_t length = 2; length <= size; length <<= 1)
        {
            const auto *const twiddles = reinterpret_cast<const float *>(plan.twiddles.data()) + 2 * (length / 2 - 1);
            for (std::size_t i = 0; i < size; i += length)
                for (std::size_t j = 0; j < length / 2; ++j)
                {
                    const auto left = 2 * (i + j), right = 2 * (i + j + length / 2);
                    const float ar = data[left], ai = data[left + 1];
                    const float br = data[right], bi = data[right + 1];
                    const float wr = twiddles[2 * j], wi = twiddles[2 * j + 1];
                    const float real = br * wr - bi * wi;
                    const float imaginary = br * wi + bi * wr;
                    data[left] = ar + real;
                    data[left + 1] = ai + imaginary;
                    data[right] = ar - real;
                    data[right + 1] = ai - imaginary;
                }
        }
    }

    struct PcmView
    {
        const float *samples{};
        std::size_t frames{};
        std::uint32_t channels{};
    };

    void FillSpectrumWindow(const SpectrumPlan &plan, std::complex<float> *const values, const PcmView &audio,
                            const std::uint32_t channel, const std::int64_t center)
    {
        auto *const data = reinterpret_cast<float *>(values);
        const auto *const hann = plan.hann.data();
        const auto size = plan.size;
        std::fill_n(data, 2 * size, 0.0F);
        for (std::size_t i = 0; i < size; ++i)
        {
            const auto sample = center + static_cast<std::int64_t>(i) - static_cast<std::int64_t>(size / 2);
            if (sample >= 0 && static_cast<std::size_t>(sample) < audio.frames)
                data[2 * i] = audio.samples[static_cast<std::size_t>(sample) * audio.channels + channel] * hann[i];
        }
    }

    void AccumulatePower(const SpectrumPlan &plan, const std::complex<float> *const values, float *const power)
    {
        const auto *const data = reinterpret_cast<const float *>(values);
        for (std::size_t bin = 1; bin <= plan.size / 2; ++bin)
        {
            const float real = data[2 * bin], imaginary = data[2 * bin + 1];
            power[bin] += real * real + imaginary * imaginary;
        }
    }

    // Sums every channel's power of one window into `power` (plan.size / 2 + 1 bins), after clearing it.
    // Channel power is added rather than amplitude: opposite stereo phases must not cancel the spectrum.
    void AnalyzeWindow(const SpectrumPlan &plan, const PcmView &audio, const std::int64_t center,
                       std::complex<float> *const scratch, float *const power)
    {
        std::fill_n(power, plan.size / 2 + 1, 0.0F);
        for (std::uint32_t channel = 0; channel < audio.channels; ++channel)
        {
            FillSpectrumWindow(plan, scratch, audio, channel, center);
            FFT(plan, scratch);
            AccumulatePower(plan, scratch, power);
        }
    }

    [[nodiscard]] float PowerNormalization(const SpectrumPlan &plan, const std::uint32_t channels) noexcept
    {
        return 4.0F / (plan.windowSum * plan.windowSum * static_cast<float>(channels));
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
                               const std::array<std::size_t, SpectrumBandCount + 1> &bins,
                               std::complex<float> *const scratch)
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

        const auto &plan = GetSpectrumPlan(Window);
        std::array<float, Window / 2 + 1> power{};
        AnalyzeWindow(plan, {audio.samples.data(), sampleCount, audio.channels}, static_cast<std::int64_t>(start),
                      scratch, power.data());
        const float normalization = PowerNormalization(plan, audio.channels);
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

    [[nodiscard]] bool HasPcmFormat(const AudioPcm &audio) noexcept
    {
        return audio.channels != 0 && audio.sampleRate != 0;
    }

    [[nodiscard]] bool IsValidSpectrumRegion(const SpectrumRegion &region) noexcept
    {
        if (region.columns == 0 || region.columns > 4096)
            return false;
        if (region.bands == 0 || region.bands > 512)
            return false;
        if (!std::isfinite(region.begin) || !std::isfinite(region.seconds) || region.seconds <= 0)
            return false;
        if (!std::isfinite(region.minimumHz) || !std::isfinite(region.maximumHz))
            return false;
        return region.minimumHz > 0 && region.maximumHz > region.minimumHz;
    }

    // Detail analysis. A window about four columns long keeps an attack a few pixels wide at every
    // zoom. A band never gets fewer than four periods of its own frequency: a shorter window would
    // follow the single cycles of a low tone and draw them as false attacks.
    constexpr double ZoomWindowColumns = 4;
    constexpr double MinimumBandPeriods = 4;
    // A window at least this many times longer than a column is analysed on its own coarser grid.
    constexpr std::int64_t GridStepsPerWindow = 8;

    [[nodiscard]] std::size_t WindowAtLeast(const double samples) noexcept
    {
        std::size_t size = MinimumWindow;
        while (size < Window && static_cast<double>(size) < samples)
            size <<= 1;
        return size;
    }

    struct DetailBand
    {
        std::size_t window{}, first{}, last{}, nearest{};
        float fraction{};
        bool visible{};
    };

    std::vector<DetailBand> MapDetailBands(const SpectrumRegion &region, const std::uint32_t sampleRate,
                                           const std::size_t zoomWindow)
    {
        std::vector<DetailBand> bands(region.bands);
        for (std::size_t band = 0; band < bands.size(); ++band)
        {
            const double left = region.minimumHz * std::pow(region.maximumHz / region.minimumHz,
                                                            static_cast<double>(band) / bands.size());
            const double right = region.minimumHz * std::pow(region.maximumHz / region.minimumHz,
                                                             static_cast<double>(band + 1) / bands.size());
            const double centerHz = std::sqrt(left * right);
            auto &mapping = bands[band];
            mapping.window = std::max(zoomWindow, WindowAtLeast(MinimumBandPeriods * sampleRate / centerHz));
            const double binsPerHz = static_cast<double>(mapping.window) / sampleRate;
            const double half = static_cast<double>(mapping.window) / 2;
            const double center = centerHz * binsPerHz;
            mapping.visible = left < sampleRate * .5;
            mapping.first = static_cast<std::size_t>(std::clamp(std::ceil(left * binsPerHz), 1.0, half));
            mapping.last = static_cast<std::size_t>(std::clamp(std::ceil(right * binsPerHz), 1.0, half + 1));
            mapping.nearest = static_cast<std::size_t>(std::clamp(std::floor(center), 1.0, half - 1));
            mapping.fraction = static_cast<float>(std::clamp(center - static_cast<double>(mapping.nearest), 0.0, 1.0));
        }
        return bands;
    }

    [[nodiscard]] std::uint8_t BandLevel(const DetailBand &band, const float *const power,
                                         const float normalization) noexcept
    {
        float maximum = std::lerp(power[band.nearest], power[band.nearest + 1], band.fraction);
        for (auto bin = band.first; bin < band.last; ++bin)
            maximum = std::max(maximum, power[bin]);
        const float db = 10 * std::log10(std::max(maximum * normalization, 1e-12F));
        const float level = std::clamp((db - SpectrumFloorDb) / -SpectrumFloorDb, 0.0F, 1.0F);
        return static_cast<std::uint8_t>(std::lround(level * SpectrumLevelMaximum));
    }

    // One window size while a tile is analysed. A window much longer than a column changes slowly
    // between columns, so it is analysed on a fixed grid of the source's own samples and interpolated;
    // a shorter one is analysed inside every column and max-pooled, so no attack between windows is skipped.
    struct WindowSpectra
    {
        const SpectrumPlan *plan{};
        float normalization{};
        std::int64_t gridStep{}, gridFirst{}; // gridStep is 0 while the window is analysed per column
        std::vector<float> grid;              // consecutive spectra, each plan->size / 2 + 1 bins
        std::vector<float> column;            // the current column's power
    };

    struct DetailScratch
    {
        std::vector<std::complex<float>> values = std::vector<std::complex<float>>(Window);
        std::vector<float> power = std::vector<float>(Window / 2 + 1);
    };

    // Grid positions are multiples of gridStep in the source's own samples, so neighbouring tiles
    // and every offset copy of the same clip read identical spectra.
    void AnalyzeGrid(WindowSpectra &spectra, const PcmView &audio, const std::int64_t firstSample,
                     const std::int64_t lastSample, DetailScratch &scratch, const std::stop_token &stop)
    {
        const auto bins = spectra.plan->size / 2 + 1;
        spectra.gridFirst = firstSample / spectra.gridStep;
        const auto count = static_cast<std::size_t>(lastSample / spectra.gridStep + 1 - spectra.gridFirst + 1);
        spectra.grid.assign(count * bins, 0.0F);
        for (std::size_t index = 0; index < count && !stop.stop_requested(); ++index)
            AnalyzeWindow(*spectra.plan, audio, (spectra.gridFirst + static_cast<std::int64_t>(index)) * spectra.gridStep,
                          scratch.values.data(), spectra.grid.data() + index * bins);
    }

    void InterpolateGrid(WindowSpectra &spectra, const std::int64_t center) noexcept
    {
        const auto bins = spectra.plan->size / 2 + 1;
        const auto index = static_cast<std::size_t>(center / spectra.gridStep - spectra.gridFirst);
        const float fraction = static_cast<float>(center % spectra.gridStep) / static_cast<float>(spectra.gridStep);
        const float *const before = spectra.grid.data() + index * bins;
        const float *const after = before + bins;
        float *const column = spectra.column.data();
        for (std::size_t bin = 1; bin < bins; ++bin)
            column[bin] = std::lerp(before[bin], after[bin], fraction);
    }

    void PoolColumn(WindowSpectra &spectra, const PcmView &audio, const double firstSample, const double lastSample,
                    DetailScratch &scratch, const std::stop_token &stop)
    {
        const auto bins = spectra.plan->size / 2 + 1;
        float *const column = spectra.column.data();
        float *const current = scratch.power.data();
        std::fill_n(column, bins, 0.0F);
        // Wide overview columns must not skip short attacks between FFT windows.
        const auto windows = static_cast<std::size_t>(
            std::max(1.0, std::ceil((lastSample - firstSample) / (static_cast<double>(spectra.plan->size) / 4))));
        for (std::size_t window = 0; window < windows && !stop.stop_requested(); ++window)
        {
            const auto center = static_cast<std::int64_t>(std::llround(std::lerp(
                firstSample, lastSample, (static_cast<double>(window) + .5) / static_cast<double>(windows))));
            AnalyzeWindow(*spectra.plan, audio, center, scratch.values.data(), current);
            for (std::size_t bin = 1; bin < bins; ++bin)
                column[bin] = std::max(column[bin], current[bin]);
        }
    }

    using WindowSpectraSet = std::array<WindowSpectra, WindowSizeCount>;

    // Prepares every window size the bands use for the part of the clip that the tile shows.
    void PrepareWindowSpectra(WindowSpectraSet &set, const std::vector<DetailBand> &bands, const PcmView &audio,
                              const double columnSamples, const std::int64_t firstSample,
                              const std::int64_t lastSample, DetailScratch &scratch, const std::stop_token &stop)
    {
        for (const auto &band : bands)
            set[WindowIndex(band.window)].plan = &GetSpectrumPlan(band.window);
        for (auto &spectra : set)
        {
            if (!spectra.plan)
                continue;
            spectra.normalization = PowerNormalization(*spectra.plan, audio.channels);
            spectra.column.assign(spectra.plan->size / 2 + 1, 0.0F);
            const auto step = static_cast<std::int64_t>(spectra.plan->size) / GridStepsPerWindow;
            if (static_cast<double>(step) <= columnSamples)
                continue;
            spectra.gridStep = step;
            AnalyzeGrid(spectra, audio, firstSample, lastSample, scratch, stop);
        }
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
        while (nextFrame < audio.totalFrames && !stop.stop_requested())
        {
            // Before the final block, wait until the centered window is decoded.
            if (!end && nextFrame + std::max(Window / 2, hop) > audio.totalFrames)
                break;
            result.frames.push_back(AnalyzeFrame(audio, nextFrame - audio.firstFrame, hop, bins,
                                                spectrumScratch->data()));
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
        const bool formatChanged = audio.rate != 0 && (audio.channels != channels || audio.rate != rate);
        if (channels == 0 || rate == 0 || formatChanged)
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
    const bool validColumns = columns != 0 && columns <= 4096;
    const bool validSpan = std::isfinite(begin) && std::isfinite(seconds) && seconds > 0;
    if (!HasPcmFormat(audio) || !validColumns || !validSpan)
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
    if (!HasPcmFormat(audio) || !IsValidSpectrumRegion(region) || !std::isfinite(offset))
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
    const PcmView pcm{audio.samples.data(), audio.samples.size() / audio.channels, audio.channels};
    const double duration = static_cast<double>(pcm.frames) / audio.sampleRate;
    if (offset >= region.begin + region.seconds || offset + duration <= region.begin) return;

    // Choose each band's window from the column duration, then analyse the long windows once per tile.
    const double columnSamples = region.seconds * audio.sampleRate / static_cast<double>(region.columns);
    const auto bands = MapDetailBands(region, audio.sampleRate, WindowAtLeast(columnSamples * ZoomWindowColumns));
    const double tileFirst = std::max(0.0, region.begin - offset) * audio.sampleRate;
    const double tileLast = std::min(duration, region.begin + region.seconds - offset) * audio.sampleRate;
    DetailScratch scratch;
    WindowSpectraSet set{};
    PrepareWindowSpectra(set, bands, pcm, columnSamples, static_cast<std::int64_t>(tileFirst),
                         static_cast<std::int64_t>(std::ceil(tileLast)), scratch, stop);

    for (std::size_t column = 0; column < region.columns && !stop.stop_requested(); ++column)
    {
        const double left = region.begin + region.seconds * static_cast<double>(column) / region.columns - offset;
        const double right = region.begin + region.seconds * static_cast<double>(column + 1) / region.columns - offset;
        if (left >= duration || right <= 0) continue;
        const double firstSample = std::max(0.0, left) * audio.sampleRate;
        const double lastSample = std::min(duration, right) * audio.sampleRate;
        const auto center = static_cast<std::int64_t>(std::llround((firstSample + lastSample) * .5));
        for (auto &spectra : set)
        {
            if (!spectra.plan) continue;
            if (spectra.gridStep != 0) InterpolateGrid(spectra, center);
            else PoolColumn(spectra, pcm, firstSample, lastSample, scratch, stop);
        }
        for (std::size_t band = 0; band < bands.size(); ++band)
        {
            const auto &mapping = bands[band];
            if (!mapping.visible) continue;
            const auto &spectra = set[WindowIndex(mapping.window)];
            auto &level = output.levels[column * region.bands + band];
            level = std::max(level, BandLevel(mapping, spectra.column.data(), spectra.normalization));
        }
    }
}
