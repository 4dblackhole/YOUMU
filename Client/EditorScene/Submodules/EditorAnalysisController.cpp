#include "EditorAnalysisController.h"
#include "PhantomCore.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

struct EditorAnalysisJob
{
    std::stop_source cancellation;
    std::atomic<bool> ready{};
    std::shared_ptr<const AnalysisBatch> result;
    std::exception_ptr failure;
};

struct EditorSpectrumJob
{
    std::stop_source cancellation;
    std::atomic<bool> ready{};
    std::shared_ptr<const SpectrumTile> result;
    std::exception_ptr failure;
    bool published{}; // only read/written by the editor thread
};

namespace
{
    // One process-owned worker bounds concurrency. Jobs own only copied paths and
    // result state, so exiting an editor never joins or leaves dangling observers.
    class AudioAnalysisWorker final
    {
    public:
        static AudioAnalysisWorker &Instance()
        {
            static AudioAnalysisWorker worker;
            return worker;
        }
        void Enqueue(std::function<void()> operation)
        {
            {
                std::scoped_lock lock(mutex_);
                operations_.push_back(std::move(operation));
            }
            wake_.notify_one();
        }
    private:
        AudioAnalysisWorker() : thread_([this](std::stop_token stop) { Run(stop); }) {}
        ~AudioAnalysisWorker()
        {
            thread_.request_stop();
            wake_.notify_all();
        }
        void Run(std::stop_token stop)
        {
            while (!stop.stop_requested())
            {
                std::function<void()> operation;
                if (!WaitAndDequeue(stop, operation))
                    break;
                // Queue access has finished and its lock has left scope before
                // running work, including destruction of retired audio data.
                try { operation(); }
                catch (...) { /* Keep the worker available if a cleanup task fails. */ }
            }
        }
        bool WaitAndDequeue(std::stop_token stop, std::function<void()> &operation)
        {
            std::unique_lock lock(mutex_);
            if (!wake_.wait(lock, stop, [this] { return !operations_.empty(); }))
                return false;
            operation = std::move(operations_.front());
            operations_.pop_front();
            // Both the dequeue and cancellation paths release through the same
            // RAII lifetime. No manual unlock is mixed with scope-based release.
            return true;
        }
        std::mutex mutex_;
        std::condition_variable_any wake_;
        std::deque<std::function<void()>> operations_;
        std::jthread thread_;
    };

    void AnalyzeFiles(const std::shared_ptr<EditorAnalysisJob> &job,
                      const std::map<std::string, std::filesystem::path> &files)
    {
        const auto stop = job->cancellation.get_token();
        if (stop.stop_requested())
            return;
        auto result = std::make_shared<AnalysisBatch>();
        for (const auto &[id, path] : files)
        {
            if (stop.stop_requested())
                return;
            try
            {
                result->sounds.emplace(id, AnalyzeAudio(path, stop, DecodeAudioPcm));
            }
            catch (const std::exception &error)
            {
                result->errors += id + ": " + error.what() + "; ";
            }
        }
        job->result = std::move(result);
        job->ready.store(true, std::memory_order_release);
    }

    void RetireAnalysis(std::shared_ptr<const AnalysisBatch> result) noexcept
    {
        if (!result)
            return;
        try
        {
            AudioAnalysisWorker::Instance().Enqueue([result = std::move(result)] {});
        }
        catch (...)
        {
            // Resource release must remain safe during low-memory shutdown.
        }
    }
} // namespace

void EditorAnalysisController::Stop() noexcept
{
    StopSpectrum();
    if (job_)
    {
        job_->cancellation.request_stop();
        if (job_->ready.load(std::memory_order_acquire))
            RetireAnalysis(std::move(job_->result));
        job_.reset();
    }
    RetireAnalysis(std::move(analysis_));
    analysisFiles.clear();
    ++filesRevision_;
    analysisDirty = true;
    ++analysisRevision_;
}

bool EditorAnalysisController::Update(const IEditorDocument &document, const IEditorAudioSource &mode,
                                      const GameplayLaunchRequest &request, std::string &status)
{
    if (analysisDirty)
    {
        analysisDirty = false;
        auto files = mode.AudioFiles(document, request);
        if (files != analysisFiles)
        {
            StopSpectrum();
            if (job_)
            {
                job_->cancellation.request_stop();
                if (job_->ready.load(std::memory_order_acquire))
                    RetireAnalysis(std::move(job_->result));
            }
            auto next = std::make_shared<EditorAnalysisJob>();
            AudioAnalysisWorker::Instance().Enqueue([next, files] {
                try { AnalyzeFiles(next, files); }
                catch (...)
                {
                    next->failure = std::current_exception();
                    next->ready.store(true, std::memory_order_release);
                }
            });
            job_ = std::move(next);
            analysisFiles = std::move(files);
            ++filesRevision_;
        }
    }
    if (!job_ || !job_->ready.load(std::memory_order_acquire))
        return false;
    if (job_->failure)
    {
        try { std::rethrow_exception(job_->failure); }
        catch (const std::exception &error) { status = error.what(); }
        catch (...) { status = "Audio analysis failed."; }
        job_.reset();
        return true;
    }
    StopSpectrum();
    RetireAnalysis(std::move(analysis_));
    analysis_ = std::move(job_->result);
    analyzedFilesRevision_ = filesRevision_;
    ++analysisRevision_;
    job_.reset();
    if (!analysis_->errors.empty())
        status = analysis_->errors;
    return true;
}

std::optional<double> EditorAnalysisController::MusicDurationSeconds() const
{
    // Old FFT data may stay visible while a new source is analyzed, but it
    // must never supply a seek destination for the replacement music file.
    if (analysisDirty || job_ || !analysis_ || analyzedFilesRevision_ != filesRevision_)
        return std::nullopt;
    const auto music = analysis_->sounds.find("Music");
    return music == analysis_->sounds.end() ? std::nullopt : std::optional{music->second.durationSeconds};
}

void EditorAnalysisController::CacheAudioMarkers(const IEditorDocument &document, const IEditorAudioSource &mode)
{
    if (audioMarkerDocument == &document && audioMarkerMode == &mode && audioMarkerRevision == document.Revision())
        return;
    auto markers = mode.AudioMarkers(document);
    audioMarkers = std::move(markers);
    audioMarkerDocument = &document;
    audioMarkerMode = &mode;
    audioMarkerRevision = document.Revision();
}

void EditorAnalysisController::StopSpectrum() noexcept
{
    for (auto &[request, job] : spectrumJobs_) job->cancellation.request_stop();
    spectrumJobs_.clear();
    spectrumMarkers_.reset();
    spectrumDocument_ = nullptr;
}

void EditorAnalysisController::PrepareSpectrumTiles(std::span<const EditorSpectrumRequest> requests)
{
    if (!analysis_) return;
    if (spectrumAnalysisRevision_ != analysisRevision_ || spectrumDocument_ != audioMarkerDocument ||
        spectrumMarkerRevision_ != audioMarkerRevision || !spectrumMarkers_)
    {
        StopSpectrum();
        spectrumAnalysisRevision_ = analysisRevision_;
        spectrumDocument_ = audioMarkerDocument;
        spectrumMarkerRevision_ = audioMarkerRevision;
        spectrumMarkers_ = std::make_shared<const std::vector<AudioMarker>>(audioMarkers);
    }
    // A viewport plus one neighbour on either side is the complete cache budget.
    // Cancel obsolete work before enqueueing new zoom/seek/frequency requests.
    for (auto it = spectrumJobs_.begin(); it != spectrumJobs_.end();)
        if (std::ranges::find(requests, it->first) == requests.end())
        {
            it->second->cancellation.request_stop();
            it = spectrumJobs_.erase(it);
        }
        else ++it;
    for (const auto &request : requests)
    {
        if (spectrumJobs_.contains(request)) continue;
        auto job = std::make_shared<EditorSpectrumJob>();
        AudioAnalysisWorker::Instance().Enqueue([job, request, data = analysis_, markers = spectrumMarkers_] {
            const auto stop = job->cancellation.get_token();
            if (stop.stop_requested()) return;
            try
            {
                auto result = std::make_shared<SpectrumTile>();
                result->columns = request.region.columns;
                result->bands = request.region.bands;
                result->levels.resize(result->columns * result->bands);
                const auto accumulate = [&](const AudioAnalysis &sound, double offset) {
                    if (sound.pcm) AccumulateAudioSpectrum(*result, *sound.pcm, request.region, offset, stop);
                };
                if (request.track == 0)
                {
                    if (const auto music = data->sounds.find("Music"); music != data->sounds.end())
                        accumulate(music->second, 0);
                }
                else
                {
                    double longest = 0;
                    for (const auto &[id, sound] : data->sounds)
                        if (id != "Music") longest = std::max(longest, sound.durationSeconds);
                    for (auto marker = std::ranges::lower_bound(*markers, request.region.begin - longest, {}, &AudioMarker::seconds);
                         marker != markers->end() && marker->seconds < request.region.begin + request.region.seconds &&
                         !stop.stop_requested(); ++marker)
                        if (const auto sound = data->sounds.find(marker->sound); sound != data->sounds.end())
                            accumulate(sound->second, marker->seconds);
                }
                if (stop.stop_requested()) return;
                job->result = std::move(result);
            }
            catch (...) { job->failure = std::current_exception(); }
            job->ready.store(true, std::memory_order_release);
        });
        spectrumJobs_.emplace(request, std::move(job));
    }
}

std::shared_ptr<const SpectrumTile> EditorAnalysisController::Spectrum(const EditorSpectrumRequest &request) const
{
    const auto job = spectrumJobs_.find(request);
    if (job == spectrumJobs_.end() || !job->second->ready.load(std::memory_order_acquire)) return {};
    return job->second->result;
}

bool EditorAnalysisController::PollSpectrum(std::string &status)
{
    bool changed = false;
    for (auto &[request, job] : spectrumJobs_)
        if (!job->published && job->ready.load(std::memory_order_acquire))
        {
            job->published = changed = true;
            if (job->failure)
            {
                try { std::rethrow_exception(job->failure); }
                catch (const std::exception &error) { status = error.what(); }
                catch (...) { status = "Detailed audio analysis failed."; }
            }
        }
    return changed;
}

bool EditorAnalysisController::SpectrumRunning() const noexcept
{
    return std::ranges::any_of(spectrumJobs_, [](const auto &entry) {
        return !entry.second->ready.load(std::memory_order_acquire);
    });
}
