#include "GameScene/MusicSelectScene/Submodules/OsuImportValidation.h"
#include "GameScene/MusicSelectScene/Submodules/OsuSongPlacement.h"
#include "Catalog/SongCatalog.h"
#include "Editing/ChartEditor.h"
#include "Parsing/ChartParser.h"
#include "ScopedTestFolder.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    namespace fs = std::filesystem;
    void Check(bool value, const char *message)
    {
        if (!value) throw std::runtime_error(message);
    }
    void Write(const fs::path &file, std::string_view text)
    {
        fs::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary);
        stream << text;
        Check(static_cast<bool>(stream), "Fixture write failed.");
    }
    std::string Read(const fs::path &file)
    {
        std::ifstream stream(file, std::ios::binary);
        Check(static_cast<bool>(stream), "Cannot read fixture/result.");
        return {std::istreambuf_iterator<char>{stream}, {}};
    }
    std::string JsonPath(const fs::path &path)
    {
        const auto value = path.generic_u8string();
        std::string out = "\"";
        for (const char8_t c : value)
        {
            if (c == u8'"' || c == u8'\\') out += '\\';
            out += static_cast<char>(c);
        }
        return out + '"';
    }
    // The converter's result for the three files it wrote into `folder`.
    std::string Manifest(const fs::path &folder, std::string_view objects = "1", std::string_view rows = "2")
    {
        return "{\"version\":1,\"success\":true,\"files\":{\"ymm\":" + JsonPath(folder / L"맵 [Oni].ymm") +
               ",\"ymp\":" + JsonPath(folder / L"맵 [Oni].ymp") + ",\"yme\":" + JsonPath(folder / L"맵 [Oni].yme") +
               "},\"sourceObjects\":" + std::string(objects) + ",\"noteRows\":" + std::string(rows) +
               ",\"warnings\":[{\"extra\":[true,false,null,-0.5e2]}]}";
    }
    void Rejected(std::string_view output, const fs::path &folder)
    {
        bool rejected{};
        try { static_cast<void>(ValidateConvertedOsuResult(output, folder)); }
        catch (const std::exception &) { rejected = true; }
        Check(rejected, "Invalid result was accepted.");
    }
    PatternDocument LongNotePattern()
    {
        PatternDocument pattern;
        pattern.mode = "Taiko";
        pattern.name = "Oni";
        pattern.makers = {"Test author"};
        pattern.effectFile = fs::path(L"맵 [Oni].yme");
        pattern.notes.push_back({.position = {0, {0, 1}}, .keyType = 12, .actionType = 1});
        pattern.notes.push_back({.position = {0, {1, 4}}, .keyType = 12, .actionType = 2});
        return pattern;
    }

    // The converter's JSON, the saved references and the real play session are checked in the
    // folder the converter wrote into; nothing here knows about a Songs folder.
    void TestValidation(const fs::path &root)
    {
        const auto folder = root / L"변환 ' 출력";
        Write(folder / L"맵 [Oni].ymm", "Version: 1\nFile: audio/song.mp3\nMusic Name 1: Fixture\nArtist 1: Test\nTags: \n");
        auto pattern = LongNotePattern();
        const auto patternFile = folder / L"맵 [Oni].ymp";
        Write(patternFile, ChartEditor::WritePattern(pattern));
        Write(folder / L"맵 [Oni].yme", ChartEditor::WriteEffects({}));
        const auto valid = Manifest(folder);
        const auto chart = ValidateConvertedOsuResult(valid, folder);
        Check(chart.music.audioFile == fs::path("audio/song.mp3") && chart.pattern.name == "Oni" &&
              fs::equivalent(chart.patternFile, patternFile),
              "A valid result returns the parsed chart; the audio it names is not required here.");
        Check(chart.sourceFolder.empty(),
              "A valid result returns the parsed chart; the audio it names is not required here.");
        // One long note has two serialized rows, but one playable/source object.
        Rejected(Manifest(folder, "2"), folder);
        Rejected(Manifest(folder, "1", "3"), folder);
        auto overlap = pattern;
        overlap.notes.insert(overlap.notes.begin() + 1, {.position = {0, {1, 8}}, .keyType = 1, .actionType = 0});
        Write(patternFile, ChartEditor::WritePattern(overlap));
        Rejected(Manifest(folder, "2", "3"), folder);
        Write(patternFile, ChartEditor::WritePattern(pattern));
        for (const auto number : {"-1", "1.0", "01", "1.", "-.5", "184467440737095516160"})
            Rejected(Manifest(folder, number), folder);
        Rejected(valid + "false", folder);
        Rejected("{}", folder);
        Rejected("{\"version\":1,\"version\":1}", folder);
        Rejected("{\"version\":1,\"success\":true,\"files\":{\"ymm\":\"\\uD800\"}}", folder);
        auto duplicate = valid;
        duplicate.insert(1, "\"sourceObjects\":1,");
        Rejected(duplicate, folder);
        // Files outside the folder the converter was told to use are not accepted.
        fs::create_directories(folder / "inner");
        Rejected(valid, folder / "inner");
        auto otherEffects = pattern;
        otherEffects.effectFile = "other.yme";
        Write(patternFile, ChartEditor::WritePattern(otherEffects));
        Rejected(valid, folder);
        auto unsupported = pattern;
        unsupported.mode = "Unsupported";
        Write(patternFile, ChartEditor::WritePattern(unsupported));
        Rejected(valid, folder);
        pattern.notes[0].keyType = pattern.notes[1].keyType = 999;
        Write(patternFile, ChartEditor::WritePattern(pattern));
        Rejected(valid, folder);
        Check(fs::exists(folder / L"맵 [Oni].ymm") && fs::exists(folder / L"맵 [Oni].yme"),
              "Validation must not delete the files it rejects.");
    }

    // One osu! song folder and one converted difficulty of it, written the way the converter does.
    struct Source
    {
        fs::path folder, converted;
        std::string title{"Test"}, titleUnicode{"원본 곡명"}, artist{"Test artist"}, version{"Oni"};
        std::string romanizedTitle, romanizedArtist; // The converter's romanized lines; empty writes them empty.
        std::string audio{"audio/song.mp3"}, background{"nested/bg.png"};

        ConvertedOsuChart Convert() const
        {
            fs::remove_all(converted);
            const std::string names = titleUnicode.empty() ? "Music Name 1: " + title + "\n"
                                                           : "Music Name 1: " + titleUnicode + "\nMusic Name 2: " + title + "\n";
            Write(converted / L"맵 [Oni].ymm", "Version: 1\nFile: " + audio + "\nRomanized Music Name: " + romanizedTitle + "\n" +
                                                    names + "Romanized Artist: " + romanizedArtist + "\nArtist 1: " + artist +
                                                    "\nTags: a,b\n");
            auto pattern = LongNotePattern();
            pattern.name = version;
            pattern.backgroundFile = fs::path(std::u8string(reinterpret_cast<const char8_t *>(background.data()), background.size()));
            pattern.backgroundOffsetX = 15;
            Write(converted / L"맵 [Oni].ymp", ChartEditor::WritePattern(pattern));
            Write(converted / L"맵 [Oni].yme", ChartEditor::WriteEffects({}));
            auto chart = ValidateConvertedOsuResult(Manifest(converted), converted);
            chart.sourceFolder = folder;
            return chart;
        }
    };
    template <typename Function> std::string Fails(Function function)
    {
        try { function(); }
        catch (const std::exception &error) { return error.what(); }
        throw std::runtime_error("Expected placement failure.");
    }
    void NoStagingLeft(const fs::path &parent)
    {
        for (const auto &entry : fs::directory_iterator(parent))
            Check(!entry.path().filename().native().starts_with(L".osu-import-"),
                  "A placement must remove its private staging folder.");
    }
    std::size_t Patterns(const fs::path &songs)
    {
        const auto catalog = SongCatalog{}.Load(songs);
        Check(catalog.Succeeded(), "Placed songs must load through the production catalog.");
        return catalog.PatternCount();
    }

    // Names, renamed references, reuse of identical audio and the files that must survive a refusal.
    void TestPlacement(const fs::path &root)
    {
        const auto base = root / L"배치 ' 테스트", songs = base / "Songs";
        Source source{base / L"osu 곡 폴더", base / "converted"};
        Write(source.folder / "audio/song.mp3", "audio fixture");
        Write(source.folder / "nested/bg.png", "image A");
        const auto sourceFiles = [&] {
            return static_cast<std::size_t>(std::distance(fs::recursive_directory_iterator(source.folder), {}));
        };
        const auto before = sourceFiles();

        const auto plan = PlanOsuPlacement(songs, source.Convert());
        Check(plan.problem.empty() && plan.newSong && !plan.chartExists,
              "Planning a placement reports a new song and writes nothing.");
        Check(!fs::exists(songs), "Planning a placement reports a new song and writes nothing.");
        const auto first = PlaceConvertedOsuChart(songs, source.Convert());
        const auto song = songs / "Test artist - Test", music = song / "Music";
        const auto set = song / "Chart" / "Test author - Oni";
        Check(first.musicFile == music / "Test artist - Test.ymm" && fs::exists(first.musicFile) &&
                  Read(music / "Test artist - Test.mp3") == "audio fixture",
              "New audio and its YMM use the romanized Artist - Title, independently of the first notation.");
        Check(first.patternFile == set / "Test artist - Test [Oni].ymp" &&
                  first.effectFile == set / "Test artist - Test [Oni].yme" && fs::exists(first.effectFile),
              "The chart pair shares one Creator - Version folder and matching file names.");
        ChartParser parser;
        const auto placedMusic = parser.ParseMusicFile(first.musicFile);
        const auto placedPattern = parser.ParsePatternFile(first.patternFile);
        Check(placedMusic.Succeeded() && placedMusic.document.audioFile == fs::path("Test artist - Test.mp3") &&
              placedMusic.document.names == std::vector<std::string>{"원본 곡명", "Test"},
              "The YMM points at the renamed audio and keeps every other line.");
        Check(placedMusic.document.tags == std::vector<std::string>{"a", "b"},
              "The YMM points at the renamed audio and keeps every other line.");
        Check(placedPattern.Succeeded() && placedPattern.document.effectFile == first.effectFile.filename() &&
              placedPattern.document.backgroundFile == fs::path("bg.png"),
              "The YMP points at the renamed YME and at the image's file name in the song's BG folder.");
        Check(placedPattern.document.backgroundOffsetX == 15 && placedPattern.document.notes.size() == 2,
              "The YMP points at the renamed YME and at the image's file name in the song's BG folder.");
        Check(Read(song / "BG/bg.png") == "image A" && Patterns(songs) == 1 && sourceFiles() == before,
              "The background is copied into the song's BG folder and the osu! folder is left untouched.");
        Check(!fs::exists(songs / ".osu-import.lock"), "A finished placement removes its lock marker.");

        Check(PlanOsuPlacement(songs, source.Convert()).chartExists, "An existing chart set is reported by the plan.");
        const auto pattern = Read(first.patternFile);
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(first.patternFile) == pattern, "An existing chart set is never replaced.");

        const auto musicTime = fs::last_write_time(first.musicFile), imageTime = fs::last_write_time(song / "BG/bg.png");
        source.version = "Hard";
        const auto second = PlaceConvertedOsuChart(songs, source.Convert());
        Check(!second.newSong && second.musicFile == first.musicFile && Patterns(songs) == 2,
              "Another difficulty reuses the existing YMM, byte-identical audio and identical background.");
        Check(fs::last_write_time(first.musicFile) == musicTime && fs::last_write_time(song / "BG/bg.png") == imageTime,
              "Another difficulty reuses the existing YMM, byte-identical audio and identical background.");

        // Same names, same size, different bytes: nothing of the refused import may appear.
        source.version = "Third";
        Write(source.folder / "audio/song.mp3", "audi0 fixture");
        Check(!PlanOsuPlacement(songs, source.Convert()).problem.empty(), "Different audio under the same song name is a problem.");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(!fs::exists(song / "Chart" / "Test author - Third") && Read(music / "Test artist - Test.mp3") == "audio fixture",
              "Different audio must fail without publishing a chart or replacing music.");
        Write(source.folder / "audio/song.mp3", "audio fixture");

        // Identical audio under another title joins the registered song instead of duplicating it.
        source.title = "Different";
        const auto shared = PlaceConvertedOsuChart(songs, source.Convert());
        Check(shared.musicFile == first.musicFile && !fs::exists(songs / "Test artist - Different") &&
              shared.patternFile.parent_path() == song / "Chart" / "Test author - Third",
              "Identical audio in a differently named source must reuse the registered song and join its folder.");
        Check(Patterns(songs) == 3,
              "Identical audio in a differently named source must reuse the registered song and join its folder.");
        source.title = "Test";

        // A background with the same name but other bytes is a conflict that keeps both sides intact.
        source.version = "Clash";
        Write(source.folder / "nested/bg.png", "image B");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(song / "BG/bg.png") == "image A" && !fs::exists(song / "Chart" / "Test author - Clash"),
              "A conflicting background preserves the existing image and publishes no chart.");
        Write(source.folder / "other.jpg", "image C");
        source.background = "other.jpg";
        static_cast<void>(PlaceConvertedOsuChart(songs, source.Convert()));
        Check(Read(song / "BG/bg.png") == "image A" && Read(song / "BG/other.jpg") == "image C" && Patterns(songs) == 4,
              "Another image name on another difficulty keeps both images.");

        // Missing or escaping source files never reach Songs.
        source.version = "Missing";
        source.background = "missing.png";
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        auto escaping = source.Convert();
        Write(base / "outside.png", "outside");
        escaping.pattern.backgroundFile = "../outside.png";
        Fails([&] { PlaceConvertedOsuChart(songs, escaping); });
        source.background = "other.jpg";
        source.audio = "absent.mp3";
        Check(!PlanOsuPlacement(songs, source.Convert()).problem.empty(), "Missing audio is reported by the plan.");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        source.audio = "audio/song.mp3";
        Check(!fs::exists(song / "Chart" / "Test author - Missing") && Patterns(songs) == 4,
              "A chart whose audio or image cannot be copied is not published.");
        NoStagingLeft(base);
        Check(sourceFiles() == before + 1, "Placements only read the osu! song folder.");
    }

    // File-name rules, the import lock and rollback of a publication that fails half way.
    void TestNamesLockAndRollback(const fs::path &root)
    {
        const auto base = root / "names", songs = base / "Songs";
        Source source{base / "source", base / "converted"};
        Write(source.folder / "audio/song.mp3", "first recording");
        source.background.clear();
        source.artist = "Artist/Name";
        source.title = "Song:Name";
        source.titleUnicode.clear();
        source.version = "CON";
        const auto named = PlaceConvertedOsuChart(songs, source.Convert());
        Check(named.songFolder.filename() == "Artist_Name - Song_Name" &&
              named.patternFile.parent_path().filename() == "Test author - CON" &&
              named.patternFile.filename() == "Artist_Name - Song_Name [CON].ymp",
              "Source metadata must produce valid song, chart set and file names; no background means no BG folder.");
        Check(!fs::exists(named.songFolder / "BG"),
              "Source metadata must produce valid song, chart set and file names; no background means no BG folder.");

        // Without a romanized notation the first notation names the folder.
        Write(source.folder / "audio/song.mp3", "second recording");
        source.artist = "아티스트";
        source.title = "곡";
        source.version = "Oni";
        const auto unicode = PlaceConvertedOsuChart(songs, source.Convert());
        Check(unicode.songFolder.filename() == fs::path(L"아티스트 - 곡") && Patterns(songs) == 2,
              "A song without a Roman notation is named by its first notation.");
        // The romanized lines name the folder when they are filled in.
        Write(source.folder / "audio/song.mp3", "romanized recording");
        source.romanizedTitle = "Kyoku";
        source.romanizedArtist = "Artist R";
        const auto romanized = PlaceConvertedOsuChart(songs, source.Convert());
        const auto romanizedMusic = ChartParser{}.ParseMusicFile(romanized.musicFile);
        Check(romanized.songFolder.filename() == "Artist R - Kyoku" &&
              romanized.patternFile.filename() == "Artist R - Kyoku [Oni].ymp" && romanizedMusic.Succeeded(),
              "Romanized lines name the song folder and stay in the placed YMM.");
        Check(romanizedMusic.document.romanizedName == "Kyoku" && romanizedMusic.document.names.front() == "곡",
              "Romanized lines name the song folder and stay in the placed YMM.");
        source.romanizedTitle.clear();
        source.romanizedArtist.clear();
        fs::remove_all(romanized.songFolder);

        // A user's own file at the lock path is neither opened nor deleted, and blocks the import.
        Write(source.folder / "audio/song.mp3", "third recording");
        source.artist = "Lock";
        source.title = "Song";
        Write(songs / ".osu-import.lock", "user file");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(songs / ".osu-import.lock") == "user file" && !fs::exists(songs / "Lock - Song"),
              "An occupied lock path refuses the import and preserves the file.");
        fs::remove(songs / ".osu-import.lock");

        // A chart set folder that already holds a user's file is never adopted, and the new
        // song's music is not published either.
        const auto occupied = songs / "Lock - Song" / "Chart" / "Test author - Oni";
        Write(occupied / "user.txt", "preserve");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(occupied / "user.txt") == "preserve" && !fs::exists(songs / "Lock - Song" / "Music"),
              "A refused chart set must preserve user files and never publish its new music.");

        // "Chart" exists as a file, so the chart pair cannot be published after the music was:
        // the music, the audio and the background of this import must all be withdrawn again.
        Write(source.folder / "audio/song.mp3", "fourth recording");
        Write(source.folder / "bg.png", "image");
        source.background = "bg.png";
        source.artist = "Rollback";
        const auto blocked = songs / "Rollback - Song";
        Write(blocked / "Chart", "not a folder");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        std::size_t entries{};
        for (const auto &entry : fs::recursive_directory_iterator(blocked))
            entries += entry.path().filename() == "Chart" ? 0 : 1;
        Check(entries == 0 && Read(blocked / "Chart") == "not a folder",
              "A failed publication removes its music and background and keeps what was there before.");
        // The two leftover fixture folders are not songs, so the catalog reports them; only the count matters.
        Check(SongCatalog{}.Load(songs).PatternCount() == 2 && !fs::exists(songs / ".osu-import.lock"),
              "Failures release the lock and leave the playable charts as they were.");
        NoStagingLeft(base);
    }
}

int wmain(int count, wchar_t *arguments[])
{
    try
    {
        if (count == 3)
        {
            static_cast<void>(ValidateConvertedOsuResult(Read(arguments[1]), arguments[2]));
            std::cout << "Saved converter result accepted by YOUMU.\n";
            return 0;
        }
        const ScopedTestFolder fixture(fs::absolute("build/osu-import-validation"), L"run");
        const auto &root = fixture.Path();
        TestValidation(root);
        TestPlacement(root);
        TestNamesLockAndRollback(root);
        std::cout << "Osu import: converter result validation, song folder placement, audio/background reuse, "
                     "name rules, lock and rollback passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
