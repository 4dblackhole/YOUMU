#pragma once

#include "App/AudioVolumes.h"
#include "App/ControlKeys.h"
#include "Model/Submodules/NotationText.h"
#include "PhantomCore.h"
#include "Texts/TextCatalog.h"

#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

// FMOD mixer buffer lengths offered in the options, in samples (four buffers each).
inline constexpr std::array<std::uint32_t, 4> DspBufferLengths{128, 256, 512, 1024};

struct OptionValues
{
    Language language{Language::Korean};
    std::wstring skinSet{L"Default Skin"};
    std::string audioMiddleware{"FMOD"};
    PhantomCore::AudioOutputBackend audioOutput{PhantomCore::AudioOutputBackend::Automatic};
    int driverIndex{-1};
    std::string driverName;
    AudioVolumes volumes;
    // [Audio] DspBufferLength, one of DspBufferLengths. The engine reads it when it starts, so a change
    // applies from the next start (the mixer cannot restart while clips are loaded).
    std::uint32_t dspBufferLength{256};
    std::uint32_t windowWidth{1280}, windowHeight{720};
    // Framed window, or an unframed picture centred on a black monitor-sized backdrop.
    PhantomCore::WindowMode windowMode{PhantomCore::WindowMode::Windowed};
    // Song, artist, chart and maker texts: the first notation, or the romanized line.
    TextNotation notation{TextNotation::First};
    // Key beam opacity for every mode, 0..100%; 100 draws the beam as it always was.
    int keyBeamOpacity{100};
    // Play-screen background brightness, 0 (black) .. 100 (the image as it is); osu!'s background dim.
    int backgroundBrightness{100};
    // How often the engine renders: the display refresh rate, twice that rate, or after every update.
    PhantomCore::RenderRateMode renderRate{PhantomCore::RenderRateMode::Display};
    std::int32_t globalOffsetMs{};
    std::filesystem::path songsPath; // Empty selects the development/deployment default.
    std::uint16_t restartKey{DefaultRestartKey}; // [Controls] RestartKey; a missing entry keeps the default.
    std::map<std::string, std::map<std::string, std::vector<std::uint16_t>>> modeKeys;
};

// Owns the persistent values, independently of the panel and audio handles.
// A successful atomic file replacement is required before publishing a change.
class OptionSettings final
{
  public:
    explicit OptionSettings(std::filesystem::path path);
    [[nodiscard]] static std::filesystem::path NextToExecutable();
    [[nodiscard]] const std::filesystem::path &Path() const noexcept { return path_; }
    [[nodiscard]] const OptionValues &Values() const noexcept { return values_; }
    [[nodiscard]] std::filesystem::path SongsRoot() const;
    [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
    [[nodiscard]] const std::string &LoadWarning() const noexcept { return loadWarning_; }
    // Failure publishes defaults and a warning without overwriting the original INI.
    [[nodiscard]] bool Load(std::string &error);
    [[nodiscard]] bool Save(const OptionValues &values, std::string &error);

  private:
    std::filesystem::path path_;
    OptionValues values_;
    std::string loadWarning_;
    std::uint64_t revision_{};
};
