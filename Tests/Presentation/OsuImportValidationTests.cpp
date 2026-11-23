#include "GameScene/MusicSelectScene/Submodules/OsuImportValidation.h"
#include "GameScene/MusicSelectScene/Submodules/OsuSongPlacement.h"
#include "Catalog/ChartHash.h"
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
    // The folder the import gives new audio with these bytes: its SHA-256.
    fs::path HashFolder(const fs::path &songs, const std::string_view audio)
    {
        return songs / ChartBytesHash(audio);
    }
    fs::path FromUtf8(const std::string_view text)
    {
        return fs::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
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
        const auto song = HashFolder(songs, "audio fixture"), music = song / "Music";
        const auto set = song / "Chart" / "Test author - Oni";
        Check(first.songFolder == song && song.filename().native().size() == 64,
              "A new song folder is named after its audio: the SHA-256 of the bytes, all 64 digits.");
        Check(first.musicFile == music / "Test.ymm" && fs::exists(first.musicFile) &&
                  Read(music / "Test.mp3") == "audio fixture",
              "New audio and its YMM are named by the romanized Title, independently of the first notation.");
        Check(first.patternFile == set / "Test [Oni].ymp" && first.effectFile == set / "Test [Oni].yme" &&
                  fs::exists(first.effectFile),
              "The chart pair shares one Creator - Version folder and is named Title [Version].");
        ChartParser parser;
        const auto placedMusic = parser.ParseMusicFile(first.musicFile);
        const auto placedPattern = parser.ParsePatternFile(first.patternFile);
        Check(placedMusic.Succeeded() && placedMusic.document.audioFile == fs::path("Test.mp3") &&
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

        // Same names, same size, different bytes (a sped-up recording, a second mp3 of the same set): a song of its
        // own in the folder named after its audio, beside the first one.
        source.version = "Sped up";
        Write(source.folder / "audio/song.mp3", "audi0 fixture");
        const auto otherPlan = PlanOsuPlacement(songs, source.Convert());
        Check(otherPlan.problem.empty() && otherPlan.newSong, "Other audio under the same Artist - Title is a new song.");
        const auto other = PlaceConvertedOsuChart(songs, source.Convert());
        const auto otherSong = HashFolder(songs, "audi0 fixture");
        Check(other.songFolder == otherSong && other.musicFile == otherSong / "Music" / "Test.ymm" &&
                  Read(otherSong / "Music" / "Test.mp3") == "audi0 fixture",
              "Other audio with the same Artist - Title gets the folder named after its own hash.");
        Check(Read(music / "Test.mp3") == "audio fixture" &&
                  fs::last_write_time(first.musicFile) == musicTime && Patterns(songs) == 3,
              "The first song stays as it was, and both songs load.");
        Write(source.folder / "audio/song.mp3", "audio fixture");

        // Identical audio under another title joins the registered song instead of duplicating it.
        source.version = "Third";
        source.title = "Different";
        const auto shared = PlaceConvertedOsuChart(songs, source.Convert());
        Check(shared.musicFile == first.musicFile && !shared.newSong &&
                  shared.patternFile == song / "Chart" / "Test author - Third" / "Different [Third].ymp",
              "Identical audio under another title joins the registered song; its chart keeps its own Title.");
        Check(Patterns(songs) == 4,
              "Identical audio in a differently named source must reuse the registered song and join its folder.");
        source.title = "Test";

        // A song folder from before, with the old Artist - Title names, that holds the same audio is joined; the new
        // chart is named by the new rule and no hash folder is made.
        const auto legacy = songs / "Legacy artist - Legacy";
        Write(legacy / "Music/Legacy artist - Legacy.ymm",
              "Version: 1\nFile: Legacy artist - Legacy.mp3\nMusic Name 1: Legacy\nArtist 1: Legacy artist\n");
        Write(legacy / "Music/Legacy artist - Legacy.mp3", "legacy recording");
        Write(source.folder / "audio/song.mp3", "legacy recording");
        source.version = "Legacy";
        const auto joined = PlaceConvertedOsuChart(songs, source.Convert());
        Check(!joined.newSong && joined.songFolder == legacy && !fs::exists(HashFolder(songs, "legacy recording")),
              "Audio already in a song folder from before joins that folder, whatever its name.");
        Check(joined.patternFile == legacy / "Chart" / "Test author - Legacy" / "Test [Legacy].ymp",
              "Audio already in a song folder from before joins that folder, whatever its name.");
        fs::remove_all(legacy);

        // Other audio planted by hand under a hash name is reported and never replaced.
        Write(source.folder / "audio/song.mp3", "planted recording");
        const auto planted = HashFolder(songs, "planted recording");
        fs::create_directories(planted);
        fs::copy(music, planted / "Music");
        source.version = "Planted";
        Check(!PlanOsuPlacement(songs, source.Convert()).problem.empty(),
              "A hash folder holding other audio is a problem for that difficulty.");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(planted / "Music" / "Test.mp3") == "audio fixture" && !fs::exists(planted / "Chart"),
              "A hash folder holding other audio keeps its music and gets no chart.");
        fs::remove_all(planted);
        Write(source.folder / "audio/song.mp3", "audio fixture");

        // A background with the same name but other bytes is a conflict that keeps both sides intact.
        source.version = "Clash";
        Write(source.folder / "nested/bg.png", "image B");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(song / "BG/bg.png") == "image A" && !fs::exists(song / "Chart" / "Test author - Clash"),
              "A conflicting background preserves the existing image and publishes no chart.");
        Write(source.folder / "other.jpg", "image C");
        source.background = "other.jpg";
        static_cast<void>(PlaceConvertedOsuChart(songs, source.Convert()));
        Check(Read(song / "BG/bg.png") == "image A" && Read(song / "BG/other.jpg") == "image C" && Patterns(songs) == 5,
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
        Check(!fs::exists(song / "Chart" / "Test author - Missing") && Patterns(songs) == 5,
              "A chart whose audio or image cannot be copied is not published.");
        NoStagingLeft(base);
        Check(sourceFiles() == before + 1, "Placements only read the osu! song folder.");
    }

    // No surrogate pair is split: every high surrogate has its low one and the other way round.
    bool WholeSurrogates(const std::wstring &text)
    {
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const bool high = IS_HIGH_SURROGATE(text[i]), low = IS_LOW_SURROGATE(text[i]);
            if (high && (i + 1 == text.size() || !IS_LOW_SURROGATE(text[i + 1])))
                return false;
            if (low && (i == 0 || !IS_HIGH_SURROGATE(text[i - 1])))
                return false;
        }
        return true;
    }
    bool WithinWindowsLimits(const OsuPlacement &plan)
    {
        const bool folders = plan.musicFile.parent_path().native().size() <= 247 &&
                             plan.patternFile.parent_path().native().size() <= 247;
        return folders && plan.musicFile.native().size() <= 259 && plan.patternFile.native().size() <= 259;
    }
    // Long file names are shortened with "..." so every path stays within what Windows opens without long-path
    // support (user decision 2026-11-23); the hash folder, the extensions and the chart set folder never are.
    void TestShortenedNames(const fs::path &root)
    {
        const auto base = root / "short", songs = base / "Songs";
        Check(base.native().size() < 140, "The fixture folder leaves room for the long Songs root cases.");
        Source source{base / "osu", base / "converted"};
        source.background.clear();
        source.titleUnicode.clear();

        // A long title: cut where the room ends, spaces and dots before the "..." dropped. The YMM and the audio
        // end in four dots before the extension, and the extension still reads as the last one.
        Write(source.folder / "audio/song.mp3", "long title audio");
        source.title = "x";
        for (int i = 0; i < 100; ++i)
            source.title += ". ";
        source.title += "end";
        const auto longTitle = PlaceConvertedOsuChart(songs, source.Convert());
        Check(longTitle.songFolder == HashFolder(songs, "long title audio") &&
                  longTitle.musicFile.filename() == "x....ymm" && longTitle.audioName == "x....mp3",
              "A long Title is shortened with three dots in the YMM and audio names; the folder never is.");
        Check(longTitle.patternFile.filename() == "x... [Oni].ymp" &&
                  longTitle.effectFile.filename() == "x... [Oni].yme",
              "A long Title gives way in the chart names and the version stays.");
        const auto longMusic = ChartParser{}.ParseMusicFile(longTitle.musicFile);
        Check(longMusic.Succeeded() && longMusic.document.audioFile.extension() == ".mp3" &&
                  longTitle.musicFile.extension() == ".ymm",
              "Names ending in dots before the extension keep their extension and their reference.");
        Check(fs::exists(longTitle.musicFile.parent_path() / longMusic.document.audioFile),
              "Names ending in dots before the extension keep their extension and their reference.");
        Check(WithinWindowsLimits(longTitle) && Patterns(songs) == 1,
              "Shortened names keep every path within Windows' limits, and the catalog finds the song.");

        // Characters outside the BMP take two UTF-16 units; a cut never splits them, at either parity.
        std::string emoji;
        for (int i = 0; i < 150; ++i)
            emoji += "\xF0\x9F\x98\x80";
        for (const auto &[audio, title] : {std::pair{"emoji audio", emoji}, std::pair{"emoji audio 2", "a" + emoji}})
        {
            Write(source.folder / "audio/song.mp3", audio);
            source.title = title;
            const auto placed = PlaceConvertedOsuChart(songs, source.Convert());
            Check(WholeSurrogates(placed.musicFile.filename().native()) &&
                      WholeSurrogates(placed.patternFile.filename().native()),
                  "Shortening never splits a surrogate pair.");
            Check(WithinWindowsLimits(placed), "Shortening counts UTF-16 units.");
        }

        // A long version: when even a one-character Title does not leave room, the version gives way inside the
        // brackets. The chart set folder keeps its full name.
        Write(source.folder / "audio/song.mp3", "version audio");
        source.title = "Test";
        // Chart set folder 243 characters: the chart path leaves 11 for the stem, "Test [V...]".
        const auto setRoom = 226 - HashFolder(songs, "version audio").native().size();
        source.version = std::string(setRoom - 4, 'V');
        const auto longVersion = PlaceConvertedOsuChart(songs, source.Convert());
        const auto stem = longVersion.patternFile.stem().native();
        Check(stem == L"Test [V...]" && WithinWindowsLimits(longVersion) &&
                  longVersion.patternFile.parent_path().filename() == FromUtf8("Test author - " + source.version),
              "A long version is shortened inside the brackets; its chart set folder is not.");
        source.version = "Oni";

        // A long Songs root: the hash folder stays whole and the names give way. A chart set folder of exactly
        // 247 characters is created; one more fails that difficulty with a clear message and writes nothing.
        Write(source.folder / "audio/song.mp3", "root audio");
        source.title = "Long title words";
        const auto deepSongs = base / std::wstring(150 - base.native().size() - 1, L'r');
        const auto deep = PlaceConvertedOsuChart(deepSongs, source.Convert());
        Check(deep.songFolder == HashFolder(deepSongs, "root audio") && WithinWindowsLimits(deep),
              "A long Songs root shortens the chart names and keeps the hash folder.");
        Check(deep.musicFile.filename() == "Long title words.ymm" &&
                  deep.patternFile.filename() == "Long t... [Oni].ymp",
              "A long Songs root shortens the chart names and keeps the hash folder.");
        source.title = "T";
        const auto edgeSongs = base / std::wstring(158 - base.native().size() - 1, L's');
        const auto edge = PlaceConvertedOsuChart(edgeSongs, source.Convert());
        const auto edgeSet = edge.patternFile.parent_path();
        Check(edgeSet.native().size() == 247 && fs::is_directory(edgeSet),
              "A chart set folder of 247 characters is created.");
        Check(edge.patternFile.native().size() == 259 && fs::exists(edge.patternFile),
              "A chart path of 259 characters is created.");
        const auto overSongs = base / std::wstring(159 - base.native().size() - 1, L't');
        const auto overProblem = PlanOsuPlacement(overSongs, source.Convert()).problem;
        Check(overProblem.find("Windows allows 247 for a folder") != std::string::npos,
              "A chart set folder of 248 characters is a clear problem of the plan.");
        Fails([&] { PlaceConvertedOsuChart(overSongs, source.Convert()); });
        Check(!fs::exists(HashFolder(overSongs, "root audio")), "A refused chart writes nothing.");
        NoStagingLeft(base);
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
        Check(named.songFolder == HashFolder(songs, "first recording") &&
                  named.musicFile.filename() == "Song_Name.ymm" &&
                  named.patternFile.parent_path().filename() == "Test author - CON",
              "Source metadata must produce valid music, chart set and file names; no background means no BG folder.");
        Check(named.patternFile.filename() == "Song_Name [CON].ymp" && named.audioName == "Song_Name.mp3",
              "Source metadata must produce valid music, chart set and file names; no background means no BG folder.");
        Check(!fs::exists(named.songFolder / "BG"),
              "Source metadata must produce valid music, chart set and file names; no background means no BG folder.");

        // Without a romanized notation the first notation names the files.
        Write(source.folder / "audio/song.mp3", "second recording");
        source.artist = "아티스트";
        source.title = "곡";
        source.version = "Oni";
        const auto unicode = PlaceConvertedOsuChart(songs, source.Convert());
        Check(unicode.musicFile.filename() == fs::path(L"곡.ymm") && Patterns(songs) == 2,
              "A song without a Roman notation is named by its first notation.");
        // The romanized lines name the files when they are filled in.
        Write(source.folder / "audio/song.mp3", "romanized recording");
        source.romanizedTitle = "Kyoku";
        source.romanizedArtist = "Artist R";
        const auto romanized = PlaceConvertedOsuChart(songs, source.Convert());
        const auto romanizedMusic = ChartParser{}.ParseMusicFile(romanized.musicFile);
        Check(romanized.musicFile.filename() == "Kyoku.ymm" && romanized.patternFile.filename() == "Kyoku [Oni].ymp" &&
                  romanizedMusic.Succeeded(),
              "Romanized lines name the files and stay in the placed YMM.");
        Check(romanizedMusic.document.romanizedName == "Kyoku" && romanizedMusic.document.names.front() == "곡",
              "Romanized lines name the files and stay in the placed YMM.");
        source.romanizedTitle.clear();
        source.romanizedArtist.clear();
        fs::remove_all(romanized.songFolder);

        // A chart set folder longer than Windows creates is refused before anything is written (it is never
        // shortened).
        Write(source.folder / "audio/song.mp3", "long recording");
        source.version = std::string(250, 'L');
        const auto longPlan = PlanOsuPlacement(songs, source.Convert());
        Check(longPlan.problem.find("Windows allows") != std::string::npos, "A too long path is a problem of the plan.");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(!fs::exists(HashFolder(songs, "long recording")) && Patterns(songs) == 2,
              "A too long path publishes nothing.");
        source.version = "Oni";

        // A user's own file at the lock path is neither opened nor deleted, and blocks the import.
        Write(source.folder / "audio/song.mp3", "third recording");
        source.artist = "Lock";
        source.title = "Song";
        Write(songs / ".osu-import.lock", "user file");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(songs / ".osu-import.lock") == "user file" && !fs::exists(HashFolder(songs, "third recording")),
              "An occupied lock path refuses the import and preserves the file.");
        fs::remove(songs / ".osu-import.lock");

        // A chart set folder that already holds a user's file is never adopted, and the new
        // song's music is not published either.
        const auto occupied = HashFolder(songs, "third recording") / "Chart" / "Test author - Oni";
        Write(occupied / "user.txt", "preserve");
        Fails([&] { PlaceConvertedOsuChart(songs, source.Convert()); });
        Check(Read(occupied / "user.txt") == "preserve" && !fs::exists(HashFolder(songs, "third recording") / "Music"),
              "A refused chart set must preserve user files and never publish its new music.");

        // "Chart" exists as a file, so the chart pair cannot be published after the music was:
        // the music, the audio and the background of this import must all be withdrawn again.
        Write(source.folder / "audio/song.mp3", "fourth recording");
        Write(source.folder / "bg.png", "image");
        source.background = "bg.png";
        source.artist = "Rollback";
        const auto blocked = HashFolder(songs, "fourth recording");
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
        TestShortenedNames(root);
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
