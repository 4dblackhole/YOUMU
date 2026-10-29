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
        Require(reads == 1 && analysis.sampleRate == rate && analysis.frames.size() == 100 &&
                    std::abs(analysis.durationSeconds - 1) < 1e-9,
                "Analysis must retain injected PCM sample rate and exact duration.");
        const auto &frame = analysis.frames[30];
        const auto strongest = std::ranges::max_element(frame.bands);
        const auto band = std::distance(frame.bands.begin(), strongest);
        const double frequency = analysis.minimumFrequencyHz *
            std::pow(analysis.maximumFrequencyHz / analysis.minimumFrequencyHz,
                     (static_cast<double>(band) + .5) / static_cast<double>(frame.bands.size()));
        const float db = SpectrumFloorDb * (1 - *strongest);
        Require(std::abs(frequency - 1000) < 80 && db > -9 && db < -4 && frame.minimum < -.49F && frame.maximum > .49F,
                "Opposite-phase stereo must preserve its 1 kHz, half-scale spectrum and waveform.");
        Require(std::ranges::all_of(analysis.frames[90].bands, [](float level) { return level < .001F; }) &&
                    analysis.frames[90].peak < .0001F,
                "Injected silence must remain at the spectrum floor.");

        Require(analysis.pcm && analysis.pcm->samples == samples && !analysis.pcm->waveform.empty(),
                "Analysis must retain immutable original PCM and its waveform pyramid.");
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
        const auto levels = std::span(detail.levels).subspan(64 * 212, 212);
        const auto strongestDetail = std::ranges::max_element(levels);
        const auto detailBand = std::distance(levels.begin(), strongestDetail);
        const double detailFrequency = 20 * std::pow(1000.0, (detailBand + .5) / 212.0);
        Require(std::abs(detailFrequency - 1000) < 60 && *strongestDetail > .88F,
                "PCM detail must preserve the tone and opposite-phase stereo at finer band spacing.");
        SpectrumTile shifted;
        auto shiftedRegion = region;
        shiftedRegion.begin += 1;
        AccumulateAudioSpectrum(shifted, *analysis.pcm, shiftedRegion, 1, {});
        Require(shifted.levels == detail.levels, "Expected hit offsets must share the exact music time grid.");
        SpectrumTile overviewAttack;
        AccumulateAudioSpectrum(overviewAttack, impulse, {0, .1, 1, 32, 20, 20'000}, 0, {});
        Require(std::ranges::max(overviewAttack.levels) > .1F,
                "A wide spectrum pixel must retain attacks away from its central FFT window.");
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
