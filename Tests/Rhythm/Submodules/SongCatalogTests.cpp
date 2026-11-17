#include "TestSupport.h"
#include "Catalog/SongCatalogCacheVersion.h"
#include "Catalog/SongLayout.h"
#include "Mode/PlayGameModeFactory.h"
#include <cmath>
#include <map>

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
            const bool angelDream = song.metadataPath.filename() == "Takahashi yoko - Angel Dream Handshaking.ymm";
            if (angelDream)
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
                if (angelDream)
                {
                    const std::string_view maker =
                        pattern.pattern.makers.empty() ? std::string_view{} : pattern.pattern.makers.front();
                    Require(pattern.effectPath.has_value() &&
                            pattern.effectPath->parent_path() == pattern.patternPath.parent_path() &&
                            pattern.effectPath->stem() == pattern.patternPath.stem(),
                            "Each AngelDream difficulty must own a YMP/YME pair in a Maker - Difficulty chart set.");
                    Require(pattern.patternPath.parent_path().filename() ==
                                SongLayout::FolderName(maker, pattern.pattern.name),
                            "Each AngelDream difficulty must own a YMP/YME pair in a Maker - Difficulty chart set.");
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
        const std::filesystem::path valid = fixture.Path() / "Valid";
        const std::string chart = "Pattern Name: Valid\nMode: Taiko\nBase BPM: 120\n[Pattern]\n0/1,1,0\n";
        WriteTextFile(valid / "Music/valid.ymm", "Version: 1\nFile: valid.mp3\nMusic Name 1: Valid\n");
        WriteTextFile(valid / "Music/valid.mp3", "audio-placeholder");
        // A leftover reference line from the old layout is an unknown field, not a link.
        WriteTextFile(valid / "Chart/Set/valid.ymp", "Version: 1\nMusic metadata: Music/other.ymm\n" + chart);
        WriteTextFile(valid / "Chart/Set/invalid.ymp", "Version: nope\n" + chart);
        WriteTextFile(valid / "Chart/loose.ymp", "Version: 1\n" + chart);
        WriteTextFile(fixture.Path() / "Invalid/Music/invalid.ymm", "Version: nope\nMusic Name 1: Invalid\n");
        WriteTextFile(fixture.Path() / "Invalid/Chart/Set/orphan.ymp", "Version: 1\n" + chart);
        WriteTextFile(fixture.Path() / "Double/Music/a.ymm", "Version: 1\nFile: a.mp3\nMusic Name 1: A\n");
        WriteTextFile(fixture.Path() / "Double/Music/b.ymm", "Version: 1\nFile: a.mp3\nMusic Name 1: B\n");
        WriteTextFile(fixture.Path() / "Double/Music/a.mp3", "audio-placeholder");
        WriteTextFile(fixture.Path() / "Empty/Chart/Set/orphan.ymp", "Version: 1\n" + chart);
        WriteTextFile(fixture.Path() / "Patterns/old/old.ymp", "Version: 1\nMusic metadata: Music/valid.ymm\n" + chart);
        WriteTextFile(fixture.Path() / "loose.ymm", "Version: 1\nFile: valid.mp3\nMusic Name 1: Loose\n");

        const SongCatalogLoadResult catalog = SongCatalog{CountPlayableNotes}.Load(fixture.Path());
        Require(!catalog.Succeeded() && catalog.discoveredMusicFiles == 4 &&
                    catalog.discoveredPatternFiles == 4,
                "Catalog discovery must retain diagnostics and source counts for invalid files.");
        Require(catalog.songs.size() == 1 && catalog.PatternCount() == 1 &&
                    catalog.songs.front().patterns.front().patternPath == valid / "Chart/Set/valid.ymp",
                "Invalid YMM/YMP files must be isolated from otherwise playable catalog entries "
                "(songs=" +
                    std::to_string(catalog.songs.size()) +
                    ", patterns=" + std::to_string(catalog.PatternCount()) + ").");
        const auto reported = [&catalog](DiagnosticSeverity severity, const std::filesystem::path &file,
                                         std::string_view text)
        {
            return std::ranges::any_of(catalog.diagnostics, [&](const Diagnostic &diagnostic) {
                return diagnostic.severity == severity && diagnostic.location.file == file &&
                       diagnostic.message.find(text) != std::string::npos;
            });
        };
        Require(reported(DiagnosticSeverity::Error, fixture.Path() / "Double/Music", "more than one YMM") &&
                    reported(DiagnosticSeverity::Error, fixture.Path() / "Empty/Music", "needs one YMM"),
                "A song folder must hold exactly one YMM in its Music folder.");
        Require(reported(DiagnosticSeverity::Warning, fixture.Path() / "Patterns", "not a song folder") &&
                    reported(DiagnosticSeverity::Warning, valid / "Chart/loose.ymp", "outside a chart set folder") &&
                    reported(DiagnosticSeverity::Warning, valid / "Chart/Set/valid.ymp", "Unknown pattern field"),
                "Files outside the song folder layout are skipped with a warning, never joined by reference.");
    }

    void TestSongLayout()
    {
        const std::filesystem::path song = std::filesystem::path("Songs") / "Artist - Title";
        Require(SongLayout::SongFolderOfPattern(song / "Chart/Maker - Oni/a.ymp") == song &&
                SongLayout::SongFolderOfPattern(song / "chart/Maker - Oni/a.ymp") == song &&
                SongLayout::SongFolderOfPattern(song / "Patterns/Maker - Oni/a.ymp").empty(),
                "A pattern belongs to the song folder two levels above its chart set folder.");
        Require(SongLayout::SongFolderOfPattern(song / "Chart/a.ymp").empty() &&
                SongLayout::SongFolderOfPattern({}).empty(),
                "A pattern belongs to the song folder two levels above its chart set folder.");
        Require(SongLayout::SongFolderOfMusic(song / "Music/a.ymm") == song &&
                    SongLayout::SongFolderOfMusic(song / "a.ymm").empty(),
                "A YMM belongs to the song folder that holds its Music folder.");
        Require(SongLayout::BackgroundPathOfPattern(song / "Chart/Set/a.ymp", "bg1.png") == song / "BG/bg1.png" &&
                    SongLayout::BackgroundPathOfPattern(song / "Chart/Set/a.ymp", {}).empty() &&
                    SongLayout::BackgroundPathOfPattern("a.ymp", "bg1.png").empty(),
                "Backgrounds resolve inside the song folder's BG folder.");
        Require(SongLayout::FolderName("Konpaku Sariel", "Dream") == "Konpaku Sariel - Dream" &&
                SongLayout::FolderName("", " Oni ") == "Unknown - Oni" &&
                SongLayout::FolderName("A/B", "C:D?.") == "A_B - C_D_",
                "Folder names join the first two labels and replace characters a filename cannot hold.");
        Require(SongLayout::FolderName("\xEC\x95\x84", "\xEA\xB3\xA1") == std::filesystem::path(L"\uC544 - \uACE1"),
                "Folder names join the first two labels and replace characters a filename cannot hold.");

        TemporaryDirectory fixture("YOUMUSongLayout");
        const auto folder = fixture.Path() / "Song";
        Require(SongLayout::FindMusicMetadata(folder).status == SongLayout::MetadataStatus::Missing &&
                    SongLayout::FindMusicMetadata({}).status == SongLayout::MetadataStatus::Missing,
                "A folder without a Music folder has no YMM.");
        WriteTextFile(folder / "Music/audio.mp3", "audio-placeholder");
        WriteTextFile(folder / "Music/Nested/other.ymm", "Version: 1\n");
        Require(SongLayout::FindMusicMetadata(folder).status == SongLayout::MetadataStatus::Missing,
                "Only a YMM directly inside Music counts.");
        WriteTextFile(folder / "Music/Song.YMM", "Version: 1\n");
        const auto found = SongLayout::FindMusicMetadataOfPattern(folder / "Chart/Set/a.ymp");
        Require(found.status == SongLayout::MetadataStatus::Found && found.path == folder / "Music/Song.YMM",
                "A pattern finds its song's YMM from its own path.");
        WriteTextFile(folder / "Music/second.ymm", "Version: 1\n");
        Require(SongLayout::FindMusicMetadata(folder).status == SongLayout::MetadataStatus::Multiple &&
                    SongLayout::FindMusicMetadata(folder).path.empty(),
                "Two YMM files make the song ambiguous.");
    }
    // Song filter Length: the span from the first note to the last note entry, kept in the list index.
    void TestSongCatalogChartLength()
    {
        TemporaryDirectory fixture("YOUMUChartLength");
        const auto root = fixture.Path() / "Songs";
        const auto cache = fixture.Path() / "Cache" / "index.bin";
        const auto set = root / "Song/Chart/Set";
        WriteTextFile(root / "Song/Music/song.ymm", "Version: 1\nFile: song.mp3\nMusic Name 1: Title\n");
        WriteTextFile(root / "Song/Music/song.mp3", "audio-placeholder");
        WriteTextFile(set / "long.ymp", "Version: 1\nPattern Name: Long\nMode: Taiko\n"
                                        "Base BPM: 120\n[Pattern]\n1/4,1,0\n--\n2/4,1,0\n");
        WriteTextFile(set / "empty.ymp", "Version: 1\nPattern Name: Empty\nMode: Taiko\n"
                                         "Base BPM: 120\n[Pattern]\n");
        const auto lengths = [](const SongCatalogLoadResult &result)
        {
            std::map<std::string, std::optional<double>> found;
            for (const auto &entry : result.songs.front().patterns)
                found[entry.pattern.name] = entry.chartLengthSeconds;
            return found;
        };
        const SongCatalog::PatternNoteCounter countNotes = [](const PatternDocument &pattern, const EffectDocument &effects,
                                                              std::stop_token stop) { return CountPlayableNotes(pattern, effects, stop); };
        const auto load = [&] { return SongCatalog::MetadataIndex(cache, countNotes).Load(root); };
        const auto first = lengths(load());
        // 4/4 at 120 BPM: a quarter is 0.5 s, so beat 2 of measure 1 to beat 3 of measure 2 is 2.5 s.
        Require(first.size() == 2 && first.at("Long") && std::abs(*first.at("Long") - 2.5) < 1e-9,
                "Chart length spans the notes; a pattern without notes has no length.");
        Require(!first.at("Empty"), "Chart length spans the notes; a pattern without notes has no length.");
        Require(lengths(load()) == first, "The disk index restores chart lengths.");
    }

    void TestSongCatalogIndex()
    {
        TemporaryDirectory fixture("YOUMUIndex");
        const auto root = fixture.Path() / L"Songs-한글";
        const auto cache = fixture.Path() / "Cache" / "index.bin";
        const auto song = root / L"\uACE1 \uD3F4\uB354";
        const auto pattern = song / "Chart/Set/valid.ymp", effect = song / "Chart/Set/valid.yme";
        WriteTextFile(song / "Music/valid.ymm", "Version: 1\nFile: valid.mp3\nMusic Name 1: Title\n");
        WriteTextFile(song / "Music/valid.mp3", "audio-placeholder");
        WriteTextFile(effect, "Version: 1\n");
        std::string source = "Version: 1\nEffect file: valid.yme\n"
            "Pattern Name: Valid\nMode: Taiko\nBase BPM: 120\nJudgeLevel: 40.37\nHP: 17.25\n[Time Signature]\n1/4,#bpm 250\n1/4,#bpm 180\n[Pattern]\n0/1,1,0\n";
        WriteTextFile(pattern, source);
        std::size_t countCalculations{};
        const SongCatalog::PatternNoteCounter countNotes = [&](const PatternDocument& pattern,
            const EffectDocument& effects, std::stop_token stop)
        {
            ++countCalculations;
            return CountPlayableNotes(pattern, effects, stop);
        };
        const auto load = [&] { return SongCatalog::MetadataIndex(cache, countNotes).Load(root); };
        const auto first = load();
        Require(first.Succeeded() && first.PatternCount() == 1 &&
                first.songs.front().patterns.front().pattern.notes.empty(),
                "The metadata index must preserve list metadata and counts without retaining note bodies.");
        Require(first.songs.front().patterns.front().noteCount == 1 &&
                first.songs.front().patterns.front().bpmRange == std::pair{120.0, 180.0} &&
                first.songs.front().patterns.front().pattern.judgementLevel == JudgeLevel{40.37},
                "The metadata index must preserve list metadata and counts without retaining note bodies.");
        Require(first.songs.front().patterns.front().pattern.hpLevel == HpLevel{17.25},
                "The metadata index must preserve list metadata and counts without retaining note bodies.");
        Require(std::filesystem::file_size(cache) > 24, "The index must be persisted outside source songs.");
        auto unsupported = ChartParser{}.ParsePatternFile(pattern).document;
        unsupported.mode = "UnsupportedRegressionMode";
        Require(!CountPlayableNotes(unsupported),
            "Unsupported modes must not publish a fabricated logical note count.");
        const auto savedAt = std::filesystem::last_write_time(cache);
        const auto restored = load();
        Require(restored.PatternCount() == 1 &&
                restored.songs.front().patterns.front().bpmRange == std::pair{120.0, 180.0} &&
                restored.songs.front().patterns.front().pattern.hpLevel == HpLevel{17.25},
                "A new catalog instance must reuse the disk index without rewriting it.");
        Require(std::filesystem::last_write_time(cache) == savedAt,
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
