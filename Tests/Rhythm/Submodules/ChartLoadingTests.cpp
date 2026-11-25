#include "TestSupport.h"
#include "GameFlow/ChartLoadJob.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include "Mode/PlayGameModeFactory.h"
#include <atomic>
#include <future>
#include <thread>

namespace tests
{
    namespace
    {
        template<class Result> Result WaitForLoad(ChartLoadJob<Result>& job)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (auto result = job.Poll()) return std::move(*result);
                std::this_thread::yield();
            }
            throw std::runtime_error("Chart load worker did not complete.");
        }
    }

    // Cancelling is an expected outcome (Parsing/LoadCancellation.h): a cancelled load returns a result marked as
    // cancelled, never a chart diagnostic and never an exception. Checks sit between the stages of a load.
    void TestChartLoadCancellation()
    {
        std::stop_source cancellation;
        static_cast<void>(cancellation.request_stop());
        const auto stop = cancellation.get_token();
        PatternDocument pattern;
        pattern.mode = "Taiko";
        const auto created = TaikoMode{}.CreateSession(pattern, {}, stop);
        Require(created.cancelled && !created.session && created.diagnostics.empty(),
                "A cancelled session build returns a cancelled result without a session or diagnostics.");
        Require(!CountPlayableNotes(pattern, {}, stop).has_value(), "A cancelled note count has no count.");
        const auto loaded = TaikoMode{}.LoadSession("missing.ymp", {}, stop);
        Require(loaded.cancelled && loaded.diagnostics.empty(),
                "Loading cancellation must not become a chart diagnostic (here: a missing file).");
        const auto missing = TaikoMode{}.LoadSession("missing.ymp");
        Require(!missing.cancelled && !missing.diagnostics.empty(),
                "A missing chart without cancellation is a diagnostic, reported without an exception.");

        TemporaryDirectory fixture("YOUMUCatalogProgress");
        const auto root = fixture.Path() / "Songs";
        WriteTextFile(root / "Song/Music/song.ymm", "Version: 1\nFile: song.mp3\nMusic Name 1: Song\n");
        WriteTextFile(root / "Song/Music/song.mp3", "audio-placeholder");
        WriteTextFile(root / "Song/Chart/Set/chart.ymp", "Version: 1\nPattern Name: Chart\n"
            "Mode: Taiko\nBase BPM: 120\n[Pattern]\n0/1,1,0\n");
        const auto cache = fixture.Path() / "index.bin";
        std::vector<SongCatalogEntry> progress;
        auto catalog = SongCatalog::MetadataIndex(cache, CountPlayableNotes);
        const auto complete = catalog.Load(root, {}, [&](SongCatalogEntry entry)
        {
            progress.push_back(std::move(entry));
        });
        Require(complete.PatternCount() == 1 && progress.size() == 2 && progress[0].patterns.empty(),
                "Publish metadata first and only validated, counted patterns afterward.");
        Require(progress[1].patterns.size() == 1 && progress[1].patterns[0].noteCount == 1,
                "Publish metadata first and only validated, counted patterns afterward.");
        std::ifstream beforeFile(cache, std::ios::binary);
        const std::string before{std::istreambuf_iterator<char>{beforeFile}, {}};
        beforeFile.close();
        std::stop_source duringProgress;
        const auto stoppedAfterMetadata = SongCatalog::MetadataIndex(cache, CountPlayableNotes).Load(root,
            duringProgress.get_token(), [&](SongCatalogEntry entry)
            {
                Require(entry.patterns.empty(), "Cancellation after metadata must prevent pattern publication.");
                static_cast<void>(duringProgress.request_stop());
            });
        Require(stoppedAfterMetadata.cancelled, "A scan cancelled during progress returns a cancelled catalog.");
        std::ifstream afterFile(cache, std::ios::binary);
        const std::string after{std::istreambuf_iterator<char>{afterFile}, {}};
        Require(before == after, "Canceled loading must preserve the previously committed index.");
        std::stop_source duringCount;
        const auto canceledCache = fixture.Path() / "cancelled.bin";
        const auto counter = [&](const PatternDocument& document, const EffectDocument& effects,
                                 std::stop_token token)
        {
            static_cast<void>(duringCount.request_stop());
            return CountPlayableNotes(document, effects, token);
        };
        const auto stoppedInCount = SongCatalog::MetadataIndex(canceledCache, counter).Load(root, duringCount.get_token());
        Require(stoppedInCount.cancelled, "A scan cancelled while counting notes returns a cancelled catalog.");
        Require(!std::filesystem::exists(canceledCache), "A canceled note compilation must not commit an index.");
        for (const auto& file : std::filesystem::directory_iterator(fixture.Path()))
            Require(file.path().extension() != ".tmp", "Canceled cache writes must not leave temporary files.");
    }

    void TestChartLoadJobLifetime()
    {
        const auto started = std::make_shared<std::promise<void>>();
        auto began = started->get_future();
        const auto release = std::make_shared<std::promise<void>>();
        const auto gate = release->get_future().share();
        const auto observedStop = std::make_shared<std::promise<bool>>();
        auto stopped = observedStop->get_future();
        auto job = StartChartLoad([started, gate, observedStop](std::stop_token stop)
        {
            started->set_value();
            gate.wait();
            observedStop->set_value(stop.stop_requested());
            return 7;
        });
        const bool beganInTime = began.wait_for(std::chrono::seconds{5}) == std::future_status::ready;
        const auto start = std::chrono::steady_clock::now();
        job.Cancel();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        release->set_value();
        Require(beganInTime && elapsed < std::chrono::seconds{1} && !job.Busy(),
            "Cancel must release the caller without waiting for its running worker.");
        Require(stopped.wait_for(std::chrono::seconds{5}) == std::future_status::ready && stopped.get(),
            "Canceled work must receive its stop signal while owning its copied data.");
        auto success = StartChartLoad([](std::stop_token) { return 42; });
        Require(WaitForLoad(success) == 42 && !success.Busy(), "Polling must deliver one owned result.");
        auto failure = StartChartLoad([](std::stop_token) -> int { throw std::runtime_error("load failure"); });
        bool propagated = false;
        try { static_cast<void>(WaitForLoad(failure)); }
        catch (const std::runtime_error& error) { propagated = std::string_view(error.what()) == "load failure"; }
        Require(propagated && !failure.Busy(), "Worker failures must be delivered once through Poll.");
    }

    void TestSongCatalogProgressSelection()
    {
        SongSelectionState state;
        state.catalogLoading_ = true;
        SongCatalogEntry song;
        song.metadataPath = "song.ymm";
        song.music.names = {"Song"};
        state.MergeCatalogProgress({song});
        Require(state.visibleSongIndices_.size() == 1 && state.catalog_.PatternCount() == 0,
            "A metadata-only song is visible before its first pattern.");
        SongCatalogPattern first;
        first.patternPath = "first.ymp";
        first.pattern.mode = "Taiko";
        song.patterns = {first};
        state.MergeCatalogProgress({song});
        SongCatalogPattern second = first;
        second.patternPath = "second.ymp";
        song.patterns = {second};
        state.MergeCatalogProgress({song});
        state.SelectPattern(1);
        state.SetLaunchError(SongSelectionState::LaunchErrorKind::Pattern, "retained error");
        SongCatalogEntry other;
        other.metadataPath = "other.ymm";
        other.music.names = {"Other"};
        state.MergeCatalogProgress({other, song});
        Require(state.catalogLoading_ && state.catalog_.PatternCount() == 2 && state.selectedPatternIndex_ == 1,
                "Progress must preserve selection/errors and deduplicate patterns.");
        Require(state.launchErrorDetail_ == "retained error",
                "Progress must preserve selection/errors and deduplicate patterns.");
        SongCatalogLoadResult finished;
        song.patterns = {second, first};
        finished.songs = {other, song};
        state.FinishCatalog(std::move(finished));
        const auto index = state.visibleSongIndices_[state.focusedSongPosition_];
        Require(!state.catalogLoading_ && index == 1 && state.selectedPatternIndex_ == 0,
            "Final catalog sorting must preserve selected metadata and pattern paths.");
    }
}
