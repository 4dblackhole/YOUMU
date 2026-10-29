#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <cstdint>
#include <functional>
#include <memory>
#include <compare>
#include <span>
#include <stop_token>
#include <vector>

inline constexpr std::size_t SpectrumBandCount = 96;
inline constexpr float SpectrumFloorDb = -90.0F;
struct SpectrumFrame
{
    float peak{};
    float minimum{}, maximum{};
    std::array<float, SpectrumBandCount> bands{};
};
struct WaveformPeak
{
    float minimum{}, maximum{};
};
struct AudioPcm
{
    std::uint32_t channels{}, sampleRate{};
    std::vector<float> samples;
    // Level zero summarizes 64 sample frames; each next level groups four blocks.
    std::vector<std::vector<WaveformPeak>> waveform;
};
struct SpectrumRegion
{
    double begin{}, seconds{};
    std::size_t columns{}, bands{};
    double minimumHz{20}, maximumHz{20'000};
    auto operator<=>(const SpectrumRegion &) const = default;
};
struct SpectrumTile
{
    std::size_t columns{}, bands{};
    std::vector<float> levels; // column-major, normalized from SpectrumFloorDb to 0 dB
};
struct AudioAnalysis
{
    double secondsPerFrame{};
    double durationSeconds{};
    unsigned int sampleRate{};
    double minimumFrequencyHz{20}, maximumFrequencyHz{};
    std::vector<SpectrumFrame> frames;
    std::shared_ptr<const AudioPcm> pcm;
};
using PcmConsumer = std::function<void(std::uint32_t channels, std::uint32_t sampleRate,
                                       std::span<const float> samples)>;
using PcmReader = std::function<void(const std::filesystem::path&, std::stop_token, const PcmConsumer&)>;
// Source decoding is supplied by the host; this domain owns only PCM/FFT.
AudioAnalysis AnalyzeAudio(const std::filesystem::path &path, std::stop_token stop, const PcmReader &read);

// Queries original sample extrema through the multiresolution peak pyramid.
std::vector<WaveformPeak> BuildAudioWaveform(const AudioPcm &, double begin, double seconds, std::size_t columns);
// Reanalyzes the requested pixel grid from PCM. Multiple offset clips may be
// max-pooled into the same tile without cancelling opposite stereo phases.
void AccumulateAudioSpectrum(SpectrumTile &, const AudioPcm &, const SpectrumRegion &,
                             double offset, std::stop_token);
