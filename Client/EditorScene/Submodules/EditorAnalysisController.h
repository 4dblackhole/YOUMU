#pragma once
#include "Audio/EditorAudioAnalysis.h"
#include "GameFlow/GameplayLaunchRequest.h"
#include "Modes/IEditorAudioSource.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
struct AnalysisBatch
{
    std::map<std::string, AudioAnalysis> sounds;
    std::string errors;
};
struct EditorAnalysisJob;
struct EditorSpectrumJob;
struct EditorSpectrumRequest
{
    std::size_t track{};
    SpectrumRegion region;
    auto operator<=>(const EditorSpectrumRequest &) const = default;
};
class EditorAnalysisController final
{
  public:
    EditorAnalysisController() = default;
    EditorAnalysisController(const EditorAnalysisController &) = delete;
    EditorAnalysisController &operator=(const EditorAnalysisController &) = delete;
    EditorAnalysisController(EditorAnalysisController &&) = delete;
    EditorAnalysisController &operator=(EditorAnalysisController &&) = delete;
    ~EditorAnalysisController()
    {
        Stop();
    }
    void Invalidate() noexcept
    {
        analysisDirty = true;
    }
    void Stop() noexcept;
    bool Update(const IEditorDocument &, const IEditorAudioSource &,
                const GameplayLaunchRequest &, std::string &status);
    void PrepareSpectrumTiles(std::span<const EditorSpectrumRequest> requests);
    std::shared_ptr<const SpectrumTile> Spectrum(const EditorSpectrumRequest &) const;
    bool PollSpectrum(std::string &status);
    void StopSpectrum() noexcept;
    bool SpectrumRunning() const noexcept;
    void CacheAudioMarkers(const IEditorDocument &, const IEditorAudioSource &);
    const AnalysisBatch &Data() const noexcept
    {
        static const AnalysisBatch empty;
        return analysis_ ? *analysis_ : empty;
    }
    const std::vector<AudioMarker> &Markers() const noexcept
    {
        return audioMarkers;
    }
    const std::map<std::string, std::filesystem::path> &Files() const noexcept { return analysisFiles; }
    std::uint64_t FilesRevision() const noexcept { return filesRevision_; }
    [[nodiscard]] std::optional<double> MusicDurationSeconds() const;
    bool Running() const noexcept
    {
        return job_ != nullptr;
    }
    std::uint64_t Revision() const noexcept { return analysisRevision_; }
    void ClearMarkers(std::uint64_t revision)
    {
        audioMarkers.clear();
        audioMarkerRevision = revision;
        audioMarkerDocument = nullptr;
        audioMarkerMode = nullptr;
    }

  private:
    std::map<EditorSpectrumRequest, std::shared_ptr<EditorSpectrumJob>> spectrumJobs_;
    std::shared_ptr<const std::vector<AudioMarker>> spectrumMarkers_;
    std::uint64_t spectrumAnalysisRevision_{}, spectrumMarkerRevision_{};
    const IEditorDocument *spectrumDocument_{};
    bool analysisDirty{true};
    std::shared_ptr<EditorAnalysisJob> job_;
    std::shared_ptr<const AnalysisBatch> analysis_;
    std::uint64_t analysisRevision_{};
    std::uint64_t filesRevision_{};
    std::uint64_t analyzedFilesRevision_{};
    std::map<std::string, std::filesystem::path> analysisFiles;
    std::vector<AudioMarker> audioMarkers;
    std::uint64_t audioMarkerRevision{};
    const IEditorDocument *audioMarkerDocument{};
    const IEditorAudioSource *audioMarkerMode{};
};
