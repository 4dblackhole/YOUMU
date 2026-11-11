#include "TestSupport.h"
#include "Catalog/SongCatalogCacheVersion.h"
#include "Mode/PlayGameModeFactory.h"

namespace tests
{
    void VerifyMetadataNoteCounts(const std::filesystem::path& songsRoot,
        const std::map<std::filesystem::path, std::size_t>& expected)
    {
        TemporaryDirectory fixture("YOUMULogicalNoteCounts");
        const auto cache = fixture.Path() / "index.bin";
        const auto cold = SongCatalog::MetadataIndex(cache, CountPlayableNotes).Load(songsRoot);
        const auto restored = SongCatalog::MetadataIndex(cache, CountPlayableNotes).Load(songsRoot);
        for (const auto* index : {&cold, &restored})
        {
            Require(index->Succeeded() && index->PatternCount() == expected.size(),
                "Cold and restored indices must retain every validated pattern.");
            for (const auto& song : index->songs)
                for (const auto& pattern : song.patterns)
                    Require(pattern.pattern.notes.empty() &&
                        pattern.noteCount == expected.at(pattern.patternPath),
                        "The disk index must preserve logical counts without retaining note bodies.");
        }
    }

    void TestSongCatalog(const std::filesystem::path &songsRoot)
    {
        const SongCatalogLoadResult catalog = SongCatalog{CountPlayableNotes}.Load(songsRoot);
        if (!catalog.Succeeded())
        {
            for (const Diagnostic &diagnostic : catalog.diagnostics)
            {
                if (diagnostic.severity == DiagnosticSeverity::Error)
                {
                    const auto file = diagnostic.location.file.u8string();
                    std::cerr << std::string(reinterpret_cast<const char *>(file.data()), file.size()) << ':'
                              << diagnostic.location.line << ": " << diagnostic.message << '\n';
                }
            }
        }
        Require(catalog.Succeeded(), "The copied RPG song catalog must parse without errors.");
        Require(catalog.discoveredMusicFiles >= 1 &&
                    catalog.songs.size() == catalog.discoveredMusicFiles,
                "Every discovered YMM music entry must appear in SONG LIST.");
        Require(catalog.discoveredPatternFiles >= 4 &&
                    catalog.PatternCount() == catalog.discoveredPatternFiles,
                "All bundled and optional local YMP patterns must be associated with their songs.");
        TaikoMode taiko;
        std::map<std::filesystem::path, std::size_t> expectedCounts;
        bool checkedAngelDream = false;
        for (const SongCatalogEntry &song : catalog.songs)
        {
            Require(std::filesystem::is_regular_file(song.audioPath),
                    "Every YMM entry must resolve its music file.");
            if (song.metadataPath.filename() == "angel dream hand shaking.ymm")
            {
                constexpr std::array RequiredPatterns{"angeldream [measure test].ymp",
                                                      "angeldream [test].ymp", "angeldream.ymp",
                                                      "angeldream [all notes test].ymp"};
                for (const std::string_view required : RequiredPatterns)
                {
                    Require(
                        std::ranges::any_of(song.patterns,
                                            [required](const SongCatalogPattern &pattern) {
                                                return pattern.patternPath.filename() == required;
                                            }),
                        "The bundled AngelDream catalog must include every test pattern.");
                }
                checkedAngelDream = true;
            }
            for (const SongCatalogPattern &pattern : song.patterns)
            {
                if (song.metadataPath.filename() == "angel dream hand shaking.ymm")
                {
                    Require(pattern.effectPath.has_value() &&
                                pattern.effectPath->parent_path() == pattern.patternPath.parent_path() &&
                                pattern.effectPath->stem() == pattern.patternPath.stem() &&
                                pattern.patternPath.parent_path().filename() == pattern.pattern.name,
                            "Each AngelDream difficulty must own a same-directory YMP/YME pair.");
                }
                TemporaryDirectory saved("youmu-catalog-save");
                auto copy = pattern.pattern;
                copy.sourcePath = saved.Path() / pattern.patternPath.filename();
                EffectDocument effect;
                if (pattern.effectPath)
                    effect = ChartParser{}.ParseEffectFile(*pattern.effectPath).document;
                ChartEditor editing(copy, effect);
                editing.Save();
                const auto reloaded = ChartParser{}.ParsePatternFile(copy.sourcePath);
                Require(reloaded.Succeeded() &&
                            ChartEditor::WritePattern(reloaded.document) ==
                                ChartEditor::WritePattern(editing.Pattern()),
                        "Every provided chart must save/reload without source-data loss (using a "
                        "temporary copy).");
                if (pattern.patternPath.filename() == "Rapbit - Saika [test].ymp")
                {
                    const MusicalTimeline timeline(pattern.pattern);
                    const RhythmTime firstChange =
                        timeline.Compile({1, Rational{1, 8}});
                    const RhythmTime secondChange =
                        timeline.Compile({1, Rational{2, 8}});
                    const RhythmTime finalChange =
                        timeline.Compile({1, Rational{18, 8}});
                    const RhythmTime nextBarline =
                        timeline.Compile({2, Rational{0, 1}});
                    Require(secondChange - firstChange == RhythmDuration{166'667} &&
                                finalChange < nextBarline,
                            "Saika's 20/8 measure must keep every BPM anchor at its absolute N/D "
                            "position.");
                }
                ModeLoadResult loaded =
                    taiko.LoadSession(pattern.patternPath, pattern.effectPath);
                Require(loaded.Succeeded() && loaded.session->Gear().LaneCount() == 1,
                        "Every copied Taiko pattern must create one playable Lane.");
                const auto count = loaded.session->PlayableNoteCount();
                Require(pattern.noteCount == count,
                    "Catalog counts must equal actual logical note objects, including long notes.");
                expectedCounts.emplace(pattern.patternPath, count);
            }
        }
        VerifyMetadataNoteCounts(songsRoot, expectedCounts);
        Require(checkedAngelDream, "The catalog test must exercise the bundled AngelDream song.");
        std::cout << "Catalog verified: " << catalog.songs.size() << " songs, "
                  << catalog.PatternCount() << " patterns.\n";
    }

    void TestSongCatalogIsolatesInvalidFiles()
    {
        TemporaryDirectory fixture("YOUMUCatalogValidation");
        const std::filesystem::path music = fixture.Path() / "Music";
        const std::filesystem::path patterns = fixture.Path() / "Pattern";
        WriteTextFile(music / "valid.ymm", "Version: 1\nFile: valid.mp3\nMusic Name 1: Valid\n");
        WriteTextFile(music / "valid.mp3", "audio-placeholder");
        WriteTextFile(music / "invalid.ymm", "Version: nope\nMusic Name 1: Invalid\n");
        WriteTextFile(patterns / "valid.ymp", "Version: 1\nMusic metadata: Music/valid.ymm\n"
                                              "Pattern Name: Valid\nMode: Taiko\nBase BPM: 120\n"
                                              "[Pattern]\n0/1,1,0\n");
        WriteTextFile(patterns / "invalid.ymp",
                      "Version: nope\nMusic metadata: Music/valid.ymm\n"
                      "Pattern Name: Invalid\nMode: Taiko\nBase BPM: 120\n"
                      "[Pattern]\n0/1,1,0\n");

        const SongCatalogLoadResult catalog = SongCatalog{CountPlayableNotes}.Load(fixture.Path());
        Require(!catalog.Succeeded() && catalog.discoveredMusicFiles == 2 &&
                    catalog.discoveredPatternFiles == 2,
                "Catalog discovery must retain diagnostics and source counts for invalid files.");
        Require(catalog.songs.size() == 1 && catalog.PatternCount() == 1,
                "Invalid YMM/YMP files must be isolated from otherwise playable catalog entries "
                "(songs=" +
                    std::to_string(catalog.songs.size()) +
                    ", patterns=" + std::to_string(catalog.PatternCount()) + ").");
    }
    void TestSongCatalogIndex()
    {
        TemporaryDirectory fixture("YOUMUIndex");
        const auto root = fixture.Path() / L"Songs-한글";
        const auto cache = fixture.Path() / "Cache" / "index.bin";
        const auto pattern = root / "valid.ymp", effect = root / "valid.yme";
        WriteTextFile(root / "valid.ymm", "Version: 1\nFile: valid.mp3\nMusic Name 1: Title\n");
        WriteTextFile(root / "valid.mp3", "audio-placeholder");
        WriteTextFile(effect, "Version: 1\n");
        std::string source = "Version: 1\nMusic metadata: valid.ymm\nEffect file: valid.yme\n"
            "Pattern Name: Valid\nMode: Taiko\nBase BPM: 120\nJudgeLevel: 40.37\nHP: 17.25\n[Time Signature]\n1/4,#bpm 250\n1/4,#bpm 180\n[Pattern]\n0/1,1,0\n";
        WriteTextFile(pattern, source);
        std::size_t countCalculations{};
        const SongCatalog::PatternNoteCounter countNotes = [&](const PatternDocument& pattern,
            const EffectDocument& effects)
        {
            ++countCalculations;
            return CountPlayableNotes(pattern, effects);
        };
        const auto load = [&] { return SongCatalog::MetadataIndex(cache, countNotes).Load(root); };
        const auto first = load();
        Require(first.Succeeded() && first.PatternCount() == 1 &&
            first.songs.front().patterns.front().pattern.notes.empty() &&
            first.songs.front().patterns.front().noteCount == 1 &&
            first.songs.front().patterns.front().bpmRange == std::pair{120.0, 180.0} &&
            first.songs.front().patterns.front().pattern.judgementLevel == JudgeLevel{40.37} &&
            first.songs.front().patterns.front().pattern.hpLevel == HpLevel{17.25},
            "The metadata index must preserve list metadata and counts without retaining note bodies.");
        Require(std::filesystem::file_size(cache) > 24, "The index must be persisted outside source songs.");
        auto unsupported = ChartParser{}.ParsePatternFile(pattern).document;
        unsupported.mode = "UnsupportedRegressionMode";
        Require(!CountPlayableNotes(unsupported),
            "Unsupported modes must not publish a fabricated logical note count.");
        const auto savedAt = std::filesystem::last_write_time(cache);
        const auto restored = load();
        Require(restored.PatternCount() == 1 && restored.songs.front().patterns.front().bpmRange == std::pair{120.0, 180.0} &&
                restored.songs.front().patterns.front().pattern.hpLevel == HpLevel{17.25} &&
                std::filesystem::last_write_time(cache) == savedAt,
            "A new catalog instance must reuse the disk index without rewriting it.");
        const auto readVersion = [&](std::streamoff offset)
        {
            std::ifstream file(cache, std::ios::binary);
            file.seekg(offset);
            std::uint32_t value{};
            file.read(reinterpret_cast<char *>(&value), sizeof(value));
            return value;
        };

        const auto beforeEffectChange = countCalculations;
        const auto oldEffectTime = std::filesystem::last_write_time(effect);
        WriteTextFile(effect, "Version: 1\n\n");
        std::filesystem::last_write_time(effect, oldEffectTime + std::chrono::seconds{2});
        Require(load().songs.front().patterns.front().noteCount == 1 &&
            countCalculations == beforeEffectChange + 1,
            "A changed YME must invalidate the logical note count even when the YMP is unchanged.");

        const auto beforeEdit = std::filesystem::last_write_time(pattern);
        source.replace(source.find("Pattern Name: Valid"), 19, "Pattern Name: Other");
        WriteTextFile(pattern, source);
        std::filesystem::last_write_time(pattern, beforeEdit + std::chrono::seconds{2});
        Require(load().songs.front().patterns.front().pattern.name == "Other",
            "Same-size source edits must invalidate cached metadata by modification time.");
        {
            std::fstream file(cache, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(12); // Parser revision, independent of the payload checksum.
            const std::uint32_t obsolete = 0;
            file.write(reinterpret_cast<const char *>(&obsolete), sizeof(obsolete));
        }
        Require(load().PatternCount() == 1 && readVersion(12) == SongCatalogParserRevision,
            "An obsolete parser revision must rebuild from source and replace the obsolete header.");
        {
            std::fstream file(cache, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(8);
            const std::uint32_t obsolete = 0;
            file.write(reinterpret_cast<const char *>(&obsolete), sizeof(obsolete));
        }
        Require(load().PatternCount() == 1 && readVersion(8) == SongCatalogCacheSchema,
            "An obsolete cache storage schema must be disposable independently of parser revisions.");
        WriteTextFile(cache, "damaged cache");
        Require(load().PatternCount() == 1 && std::filesystem::file_size(cache) > 24,
            "A truncated cache must be rebuilt without losing playable songs.");
        {
            std::fstream file(cache, std::ios::binary | std::ios::in | std::ios::out);
            file.seekp(24);
            file.put('!');
        }
        Require(load().PatternCount() == 1, "A checksum mismatch must rebuild the disk cache.");
        const auto directoryTarget = fixture.Path() / "CacheDirectory";
        std::filesystem::create_directory(directoryTarget);
        Require(SongCatalog::MetadataIndex(directoryTarget, CountPlayableNotes).Load(root).PatternCount() == 1 &&
            std::filesystem::is_directory(directoryTarget),
            "An unusable cache path must preserve both source loading and an existing directory.");
        WriteTextFile(effect, "Version: invalid\n");
        const auto invalid = load();
        Require(invalid.PatternCount() == 0 && !invalid.Succeeded(),
            "A changed invalid YME must invalidate the cached pattern's availability.");
        Require(load().diagnostics.size() == invalid.diagnostics.size(),
            "Cached parser errors must survive a process-style catalog restart.");
        std::filesystem::remove(pattern);
        Require(load().discoveredPatternFiles == 0 && load().PatternCount() == 0,
            "Deleted patterns must disappear from a persistent index.");
        Require(SongCatalog{CountPlayableNotes}.Load(root).songs.size() == 1,
            "The full-document catalog API must remain available independently of the list index.");
    }
} // namespace tests
