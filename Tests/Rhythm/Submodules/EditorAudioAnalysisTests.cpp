#include "TestSupport.h"
#include <numbers>

namespace tests
{
    void TestEditorAudioAnalysis()
    {
        // The domain consumes injected PCM; codec/filesystem integration is
        // exercised by the Client's editor presentation regression runner.
        constexpr std::uint32_t rate = 48'000, channels = 2;
        std::vector<float> samples(static_cast<std::size_t>(rate) * channels);
        for (std::uint32_t frame = 0; frame < rate; ++frame)
        {
            const auto value = frame < rate * 7 / 10
                ? .5F * static_cast<float>(std::sin(2 * std::numbers::pi * 1000 * frame / rate)) : 0.0F;
            samples[frame * channels] = value;
            samples[frame * channels + 1] = -value;
        }
        std::size_t reads{};
        const PcmReader reader = [&](const std::filesystem::path &, std::stop_token stop, const PcmConsumer &consume) {
            ++reads;
            constexpr std::size_t chunk = 4096 * channels;
            const auto pcm = std::span<const float>(samples);
            for (std::size_t offset = 0; offset < pcm.size() && !stop.stop_requested(); offset += chunk)
                consume(channels, rate, pcm.subspan(offset, std::min(chunk, pcm.size() - offset)));
        };
        const auto analysis = AnalyzeAudio({}, {}, reader);
        Require(reads == 1 && analysis.sampleRate == rate && analysis.frames.size() == 100,
                "Analysis must retain injected PCM sample rate and exact duration.");
        Require(std::abs(analysis.durationSeconds - 1) < 1e-9,
                "Analysis must retain injected PCM sample rate and exact duration.");
        const auto &frame = analysis.frames[30];
        const auto strongest = std::ranges::max_element(frame.bands);
        const auto band = std::distance(frame.bands.begin(), strongest);
        const double frequency = analysis.minimumFrequencyHz *
            std::pow(analysis.maximumFrequencyHz / analysis.minimumFrequencyHz,
                     (static_cast<double>(band) + .5) / static_cast<double>(frame.bands.size()));
        const float db = SpectrumFloorDb * (1 - *strongest);
        Require(std::abs(frequency - 1000) < 80 && db > -9 && db < -4,
                "Opposite-phase stereo must preserve its 1 kHz, half-scale spectrum and waveform.");
        Require(frame.minimum < -.49F && frame.maximum > .49F,
                "Opposite-phase stereo must preserve its 1 kHz, half-scale spectrum and waveform.");
        Require(std::ranges::all_of(analysis.frames[90].bands, [](float level) { return level < .001F; }) &&
                    analysis.frames[90].peak < .0001F,
                "Injected silence must remain at the spectrum floor.");

        Require(analysis.pcm && analysis.pcm->samples == samples && !analysis.pcm->waveform.empty(),
                "Analysis must retain immutable original PCM and its waveform pyramid.");
        const PcmReader fragmented = [&](const std::filesystem::path &, std::stop_token,
                                         const PcmConsumer &consume) {
            const auto pcm = std::span<const float>(samples);
            constexpr std::size_t chunk = 91 * channels;
            for (std::size_t offset = 0; offset < pcm.size(); offset += chunk)
                consume(channels, rate, pcm.subspan(offset, std::min(chunk, pcm.size() - offset)));
        };
        const auto fragmentedAnalysis = AnalyzeAudio({}, {}, fragmented);
        Require(fragmentedAnalysis.frames.size() == analysis.frames.size(),
                "PCM chunk boundaries must not change the analysis time grid.");
        for (std::size_t index = 0; index < analysis.frames.size(); ++index)
        {
            const auto &expected = analysis.frames[index], &actual = fragmentedAnalysis.frames[index];
            Require(actual.minimum == expected.minimum && actual.maximum == expected.maximum &&
                    actual.peak == expected.peak,
                    "Windows spanning PCM chunks must retain the same extrema and spectrum.");
            Require(actual.bands == expected.bands,
                    "Windows spanning PCM chunks must retain the same extrema and spectrum.");
        }
        for (const auto region : {std::pair{.301, .002}, std::pair{-.01, 1.02}, std::pair{1.1, .25}})
        {
            const auto peaks = BuildAudioWaveform(*analysis.pcm, region.first, region.second, 113);
            for (std::size_t column = 0; column < peaks.size(); ++column)
            {
                const auto at = [&](std::size_t index) {
                    return static_cast<std::size_t>(std::clamp(std::ceil((region.first + region.second *
                        static_cast<double>(index) / peaks.size()) * rate), 0.0, static_cast<double>(rate)));
                };
                const auto first = at(column), last = at(column + 1);
                float minimum = first < last ? samples[first * channels] : 0;
                float maximum = minimum;
                for (auto i = first * channels; i < last * channels; ++i)
                {
                    minimum = std::min(minimum, samples[i]);
                    maximum = std::max(maximum, samples[i]);
                }
                Require(peaks[column].minimum == minimum && peaks[column].maximum == maximum,
                        "Every waveform pixel must preserve exact PCM extrema at close and overview zooms.");
            }
        }
        AudioPcm impulse;
        impulse.channels = 1;
        impulse.sampleRate = rate;
        impulse.samples.resize(rate / 10);
        impulse.samples[480] = 1;
        impulse.samples[481] = -.75F;
        const auto precise = BuildAudioWaveform(impulse, .009, .004, 192);
        Require(std::ranges::count_if(precise, [](auto peak) { return peak.maximum == 1; }) == 1 &&
                    std::ranges::count_if(precise, [](auto peak) { return peak.minimum == -.75F; }) == 1,
                "Single-sample attacks must not be stretched into the old 10 ms frame width.");

        const SpectrumRegion region{.2, .02, 128, 212, 20, 20'000};
        SpectrumTile detail;
        AccumulateAudioSpectrum(detail, *analysis.pcm, region, 0, {});
        Require(detail.columns == 128 && detail.bands == 212 && detail.levels.size() == 128 * 212,
                "Detailed STFT must follow the requested time and frequency pixel grid.");
        // 212 bands from 20 Hz to 20 kHz; -6 dBFS is level 238 of 255.
        const auto strongestIn = [](const SpectrumTile &tile, std::size_t column) {
            const auto levels = std::span(tile.levels).subspan(column * 212, 212);
            const auto strongest = std::ranges::max_element(levels);
            const auto band = static_cast<double>(std::distance(levels.begin(), strongest));
            return std::pair{20 * std::pow(1000.0, (band + .5) / 212.0), *strongest};
        };
        // A wide view (8 ms per column) keeps the full 2048-point frequency precision.
        SpectrumTile wide;
        AccumulateAudioSpectrum(wide, *analysis.pcm, {0, 1.024, 128, 212, 20, 20'000}, 0, {});
        const auto [wideFrequency, wideLevel] = strongestIn(wide, 37);
        Require(std::abs(wideFrequency - 1000) < 60 && wideLevel > 224,
                "PCM detail must preserve the tone and opposite-phase stereo at finer band spacing.");
        // The closest view trades frequency precision for time: the tone stays within a third of an octave.
        const auto [closeFrequency, closeLevel] = strongestIn(detail, 64);
        Require(std::abs(std::log2(closeFrequency / 1000)) < 1.0 / 3 && closeLevel > 224,
                "A close view must keep the tone at its level near its frequency.");
        SpectrumTile shifted;
        auto shiftedRegion = region;
        shiftedRegion.begin += 1;
        AccumulateAudioSpectrum(shifted, *analysis.pcm, shiftedRegion, 1, {});
        Require(shifted.levels == detail.levels, "Expected hit offsets must share the exact music time grid.");
        SpectrumTile overviewAttack;
        AccumulateAudioSpectrum(overviewAttack, impulse, {0, .1, 1, 32, 20, 20'000}, 0, {});
        Require(std::ranges::max(overviewAttack.levels) > 25,
                "A wide spectrum pixel must retain attacks away from its central FFT window.");

        // Closest view: 0.125 ms columns over 4..20 ms; the click at 10 ms is column 48.
        SpectrumTile attack;
        AccumulateAudioSpectrum(attack, impulse, {.004, .016, 128, 212, 20, 20'000}, 0, {});
        const auto levelAt = [&](std::size_t column, std::size_t band) { return attack.levels[column * 212 + band]; };
        constexpr std::size_t highBand = 190, lowBand = 49; // about 9.9 kHz and 100 Hz
        Require(levelAt(48, highBand) > 100 && levelAt(40, highBand) == 0 && levelAt(56, highBand) == 0,
                "A close view must show an attack within a millisecond of its sample in the high bands.");
        Require(levelAt(0, lowBand) > 0 && levelAt(127, lowBand) > 0,
                "A low band keeps a window of at least four of its periods instead of following the zoom.");

        // A steady low tone must stay a steady row: a window shorter than its period would pulse with each cycle.
        AudioPcm bass;
        bass.channels = 1;
        bass.sampleRate = rate;
        bass.samples.resize(rate / 2);
        for (std::size_t sample = 0; sample < bass.samples.size(); ++sample)
            bass.samples[sample] = .5F * static_cast<float>(std::sin(2 * std::numbers::pi * 100 * sample / rate));
        SpectrumTile steady;
        AccumulateAudioSpectrum(steady, bass, {.2, .016, 128, 212, 20, 20'000}, 0, {});
        std::uint8_t quietest = 255, loudest = 0;
        for (std::size_t column = 0; column < 128; ++column)
        {
            quietest = std::min(quietest, steady.levels[column * 212 + lowBand]);
            loudest = std::max(loudest, steady.levels[column * 212 + lowBand]);
        }
        Require(quietest > 200 && loudest - quietest <= 4,
                "A steady low tone must not be drawn as a row of false attacks in a close view.");

        // Neighbouring tiles share one analysis grid: two half-width tiles equal the whole one.
        SpectrumTile left, right;
        AccumulateAudioSpectrum(left, *analysis.pcm, {.2, .01, 64, 212, 20, 20'000}, 0, {});
        AccumulateAudioSpectrum(right, *analysis.pcm, {.21, .01, 64, 212, 20, 20'000}, 0, {});
        Require(std::ranges::equal(left.levels, std::span(detail.levels).first(64 * 212)) &&
                    std::ranges::equal(right.levels, std::span(detail.levels).subspan(64 * 212)),
                "Tiles of one visible range must join without a seam.");
        SpectrumTile aboveNyquist;
        AccumulateAudioSpectrum(aboveNyquist, *analysis.pcm, {.2, .02, 16, 32, 24'000, 30'000}, 0, {});
        Require(std::ranges::all_of(aboveNyquist.levels, [](auto value) { return value == 0; }),
                "Frequencies above the source Nyquist limit must remain silent.");

        std::stop_source stop;
        stop.request_stop();
        SpectrumTile cancelled;
        AccumulateAudioSpectrum(cancelled, *analysis.pcm, region, 0, stop.get_token());
        Require(std::ranges::all_of(cancelled.levels, [](auto value) { return value == 0; }),
                "Cancelled detail work must not run FFTs or publish nonempty signal levels.");
        Require(AnalyzeAudio({}, stop.get_token(), reader).frames.empty() && reads == 1,
                "An already-cancelled analysis must not invoke the PCM reader.");
    }
}
