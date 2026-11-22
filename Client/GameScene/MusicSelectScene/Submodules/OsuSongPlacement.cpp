#include "OsuSongPlacement.h"
#include "Catalog/ChartHash.h"
#include "Catalog/SongLayout.h"
#include "Model/Submodules/NotationText.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    [[noreturn]] void Fail(const std::string &message)
    {
        throw std::runtime_error(message);
    }
    std::string Utf8(const fs::path &path)
    {
        const auto text = path.generic_u8string();
        return {reinterpret_cast<const char *>(text.data()), text.size()};
    }
    fs::path FromUtf8(const std::string_view text)
    {
        return fs::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
    }
    bool IsInside(const fs::path &child, const fs::path &parent)
    {
        const auto relative = child.lexically_relative(parent);
        if (relative.empty() || relative == "." || relative.is_absolute())
            return false;
        return std::ranges::none_of(relative, [](const auto &part) { return part == ".."; });
    }
    bool SameContents(const fs::path &left, const fs::path &right)
    {
        if (fs::file_size(left) != fs::file_size(right))
            return false;
        std::ifstream a(left, std::ios::binary), b(right, std::ios::binary);
        if (!a || !b)
            Fail("Cannot compare file contents: " + Utf8(left));
        std::vector<char> x(65536), y(65536);
        do
        {
            a.read(x.data(), static_cast<std::streamsize>(x.size()));
            b.read(y.data(), static_cast<std::streamsize>(y.size()));
            if (a.bad() || b.bad())
                Fail("File content comparison failed: " + Utf8(left));
            if (a.gcount() != b.gcount() || !std::equal(x.begin(), x.begin() + a.gcount(), y.begin()))
                return false;
        } while (a.gcount() != 0);
        return true;
    }
    std::string ReadText(const fs::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            Fail("Cannot read converted file: " + Utf8(path));
        return {std::istreambuf_iterator<char>{file}, {}};
    }
    void WriteText(const fs::path &path, const std::string_view text)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(text.data(), static_cast<std::streamsize>(text.size()));
        file.close();
        if (!file)
            Fail("Cannot write staged file: " + Utf8(path));
    }
    // Replaces the value of the first "<key>: value" line and keeps every other byte.
    std::string WithHeaderValue(const std::string &text, const std::string_view key, const std::string_view value)
    {
        const std::string prefix = std::string(key) + ":";
        for (std::size_t line = 0; line < text.size();)
        {
            const auto newline = text.find('\n', line);
            const auto end = newline == std::string::npos ? text.size() : newline;
            if (text.compare(line, prefix.size(), prefix) == 0)
            {
                const auto contentEnd = end > line && text[end - 1] == '\r' ? end - 1 : end;
                return text.substr(0, line) + prefix + " " + std::string(value) + text.substr(contentEnd);
            }
            line = end + 1;
        }
        Fail("Converted file has no " + std::string(key) + " line.");
    }

    // Exclusive for one Songs root across processes. CREATE_NEW never opens a user's own file,
    // and the OS deletes the marker when the handle closes, even if the process is killed.
    class SongsImportLock final
    {
      public:
        explicit SongsImportLock(const fs::path &songsRoot)
        {
            fs::create_directories(songsRoot);
            const auto path = songsRoot / L".osu-import.lock";
            handle_ = ::CreateFileW(path.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                                    nullptr);
            if (handle_ == INVALID_HANDLE_VALUE)
                Fail("Another import into this Songs folder is running, or its lock path is occupied (Windows error " +
                     std::to_string(::GetLastError()) + "): " + Utf8(path));
        }
        ~SongsImportLock()
        {
            ::CloseHandle(handle_);
        }
        SongsImportLock(const SongsImportLock &) = delete;
        SongsImportLock &operator=(const SongsImportLock &) = delete;

      private:
        HANDLE handle_{};
    };

    // A private folder beside Songs (same volume), so the live catalog never sees half-written files.
    class StagingFolder final
    {
      public:
        explicit StagingFolder(const fs::path &parent)
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int retry = 0; retry < 100; ++retry)
            {
                path = parent / (L".osu-import-" + std::to_wstring(nonce) + L"-" + std::to_wstring(retry) + L".tmp");
                if (fs::create_directory(path))
                    return;
            }
            Fail("Cannot create a private staging folder beside Songs.");
        }
        ~StagingFolder()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
        StagingFolder(const StagingFolder &) = delete;
        StagingFolder &operator=(const StagingFolder &) = delete;
        fs::path path;
    };

    // The background of one chart on its way into <song folder>/BG. An image with the same name
    // and the same bytes is reused; different bytes are a conflict. Until Commit, the destructor
    // removes the file and the folders this object created, and nothing else.
    class StagedBackground final
    {
      public:
        StagedBackground(const ConvertedOsuChart &chart, const fs::path &songFolder, const fs::path &stage)
        {
            const auto &relative = chart.pattern.backgroundFile;
            if (relative.empty())
                return;
            const auto sourceRoot = fs::canonical(chart.sourceFolder);
            const auto source = fs::weakly_canonical(sourceRoot / relative);
            if (!IsInside(source, sourceRoot) || !fs::is_regular_file(source))
                Fail("Background image is missing or lies outside the osu! song folder: " + Utf8(relative));
            name_ = relative.filename();
            destination_ = songFolder / SongLayout::BackgroundFolder / name_;
            staged_ = stage / L"background.tmp";
            fs::copy_file(source, staged_, fs::copy_options::none);
            reused_ = MatchesExisting();
        }
        ~StagedBackground()
        {
            if (committed_)
                return;
            std::error_code ignored;
            if (published_)
                fs::remove(destination_, ignored);
            for (auto folder = created_.rbegin(); folder != created_.rend(); ++folder)
                fs::remove(*folder, ignored);
        }
        StagedBackground(const StagedBackground &) = delete;
        StagedBackground &operator=(const StagedBackground &) = delete;
        // File name inside the song's BG folder; empty when the chart has no background.
        [[nodiscard]] const fs::path &Name() const noexcept
        {
            return name_;
        }
        void Publish()
        {
            if (name_.empty() || reused_)
                return;
            CreateParents();
            // Same-volume move that never replaces a file another import created meanwhile.
            if (::MoveFileExW(staged_.c_str(), destination_.c_str(), 0))
            {
                published_ = true;
                return;
            }
            const auto code = ::GetLastError();
            const bool occupied = code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS;
            if (!occupied || !MatchesExisting())
                Fail("Cannot publish the background image (Windows error " + std::to_string(code) + ").");
            reused_ = true;
        }
        void Commit() noexcept
        {
            committed_ = true;
        }

      private:
        bool MatchesExisting() const
        {
            if (!fs::exists(destination_))
                return false;
            if (!fs::is_regular_file(destination_) || !SameContents(staged_, destination_))
                Fail("A different background image already has this name: " + Utf8(destination_));
            return true;
        }
        void CreateParents()
        {
            std::vector<fs::path> missing;
            for (auto parent = destination_.parent_path(); !fs::exists(parent); parent = parent.parent_path())
                missing.push_back(parent);
            for (auto folder = missing.rbegin(); folder != missing.rend(); ++folder)
                if (fs::create_directory(*folder))
                    created_.push_back(*folder);
        }
        fs::path name_, destination_, staged_;
        std::vector<fs::path> created_;
        bool published_{}, reused_{}, committed_{};
    };

    fs::path SourceAudio(const ConvertedOsuChart &chart)
    {
        const auto sourceRoot = fs::canonical(chart.sourceFolder);
        const auto audio = fs::weakly_canonical(sourceRoot / chart.music.audioFile);
        if (!IsInside(audio, sourceRoot) || !fs::is_regular_file(audio))
            Fail("Audio file is missing or lies outside the osu! song folder: " + Utf8(chart.music.audioFile));
        return audio;
    }
    // The YMM of a song folder that already holds byte-identical audio, whatever its name; empty for a
    // new song. The folder named after this song is tried first, so its own audio stays its home.
    fs::path FindSharedMusic(const fs::path &root, const fs::path &audio, const fs::path &preferredFolder)
    {
        const auto matches = [&](const fs::path &ymm) {
            const auto music = ChartParser{}.ParseMusicFile(ymm);
            if (!music.Succeeded())
                return false;
            const auto existing = fs::weakly_canonical(ymm.parent_path() / music.document.audioFile);
            return IsInside(existing, root) && fs::is_regular_file(existing) && SameContents(audio, existing);
        };
        std::vector<fs::path> candidates;
        std::error_code error;
        for (fs::directory_iterator entry(root, error), end; !error && entry != end; entry.increment(error))
        {
            if (!entry->is_directory())
                continue;
            const auto music = SongLayout::FindMusicMetadata(entry->path());
            if (music.status == SongLayout::MetadataStatus::Found)
                candidates.push_back(music.path);
        }
        std::ranges::sort(candidates);
        const auto own = std::ranges::find_if(candidates, [&](const fs::path &ymm) {
            return SongLayout::SongFolderOfMusic(ymm) == preferredFolder;
        });
        if (own != candidates.end())
            std::rotate(candidates.begin(), own, own + 1);
        const auto shared = std::ranges::find_if(candidates, matches);
        return shared == candidates.end() ? fs::path{} : *shared;
    }
    // The name of a new song folder. Artist - Title while no music has that name yet (a folder with only a
    // BG still takes it); otherwise the name gets the first eight digits of the audio's SHA-256,
    // `Artist - Title [3fa9c1d2]`, then sixteen and all 64 when other audio holds those names by chance
    // (user decision 2026-11-23). The suffix is a naming habit like the rest of the name: nothing reads it
    // back as a hash.
    fs::path NewSongName(const fs::path &root, const fs::path &song, const fs::path &audio)
    {
        if (!fs::exists(root / song / SongLayout::MusicFolder))
            return song;
        const auto hash = AudioFileHash(audio);
        for (const std::size_t digits : {std::size_t{8}, std::size_t{16}, hash.size()})
        {
            const auto name = fs::path(song.native() + L" [" + FromUtf8(hash.substr(0, digits)).native() + L"]");
            if (!fs::exists(root / name / SongLayout::MusicFolder))
                return name;
        }
        Fail("Every song folder name for this audio holds other music: " + Utf8(root / song));
    }
    // root is canonical. Throws when the chart can never be placed; an existing chart set is reported instead.
    OsuPlacement Resolve(const fs::path &root, const ConvertedOsuChart &chart)
    {
        OsuPlacement plan;
        const auto audio = SourceAudio(chart);
        // Song folders are named in romanized notation, by the same rule the screens use.
        const auto &music = chart.music;
        const auto song = SongLayout::FolderName(
            NotationText(music.artists, music.romanizedArtist, TextNotation::Romanized),
            NotationText(music.names, music.romanizedName, TextNotation::Romanized));
        const std::string_view maker =
            chart.pattern.makers.empty() ? std::string_view{} : std::string_view{chart.pattern.makers.front()};
        const auto preferredFolder = fs::weakly_canonical(root / song);
        if (!IsInside(preferredFolder, root))
            Fail("The song folder would lie outside Songs.");
        // The same audio joins its song wherever it is; other audio under a taken name gets a folder of its own.
        const auto shared = FindSharedMusic(root, audio, preferredFolder);
        plan.newSong = shared.empty();
        const auto songName = plan.newSong ? NewSongName(root, song, audio) : song;
        plan.songFolder = plan.newSong ? fs::weakly_canonical(root / songName) : SongLayout::SongFolderOfMusic(shared);
        if (!IsInside(plan.songFolder, root))
            Fail("The song folder would lie outside Songs.");
        plan.audioName = fs::path(songName.native() + audio.extension().native());
        plan.musicFile =
            plan.newSong ? plan.songFolder / SongLayout::MusicFolder / fs::path(songName.native() + L".ymm") : shared;
        const auto chartSet = fs::weakly_canonical(plan.songFolder / SongLayout::ChartFolder /
                                                   SongLayout::FolderName(maker, chart.pattern.name));
        if (!IsInside(chartSet, root))
            Fail("The chart set folder would lie outside Songs.");
        const auto stem = Utf8(song) + " [" + SongLayout::NamePart(chart.pattern.name) + "]";
        plan.patternFile = chartSet / FromUtf8(stem + ".ymp");
        plan.effectFile = chartSet / FromUtf8(stem + ".yme");
        plan.chartExists = fs::exists(chartSet);
        return plan;
    }
    void RequireParsed(const bool parsed, const char *message)
    {
        if (!parsed)
            Fail(message);
    }
    // The chart pair under its final names, with the YMP pointing at the renamed YME and the
    // background's file name inside the song's BG folder.
    void StageChart(const ConvertedOsuChart &chart, const OsuPlacement &plan, const fs::path &background,
                    const fs::path &stage)
    {
        const auto folder = stage / L"pattern";
        fs::create_directory(folder);
        auto text = WithHeaderValue(ReadText(chart.patternFile), "Effect file", Utf8(plan.effectFile.filename()));
        if (!background.empty())
            text = WithHeaderValue(text, "BG", Utf8(background));
        WriteText(folder / plan.patternFile.filename(), text);
        fs::copy_file(chart.effectFile, folder / plan.effectFile.filename(), fs::copy_options::none);
        ChartParser parser;
        const auto pattern = parser.ParsePatternFile(folder / plan.patternFile.filename());
        RequireParsed(pattern.Succeeded() && parser.ParseEffectFile(folder / plan.effectFile.filename()).Succeeded(),
                      "The renamed chart pair failed parsing.");
        const bool sameReferences = pattern.document.effectFile == plan.effectFile.filename() &&
                                    pattern.document.backgroundFile == background;
        RequireParsed(sameReferences && pattern.document.notes.size() == chart.pattern.notes.size(),
                      "The renamed chart no longer matches the converted chart.");
    }
    void StageMusic(const ConvertedOsuChart &chart, const OsuPlacement &plan, const fs::path &stage)
    {
        const auto folder = stage / L"music";
        fs::create_directory(folder);
        fs::copy_file(SourceAudio(chart), folder / plan.audioName, fs::copy_options::none);
        WriteText(folder / plan.musicFile.filename(),
                  WithHeaderValue(ReadText(chart.musicFile), "File", Utf8(plan.audioName)));
        const auto music = ChartParser{}.ParseMusicFile(folder / plan.musicFile.filename());
        RequireParsed(music.Succeeded() && music.document.audioFile == plan.audioName,
                      "The renamed music metadata failed parsing.");
    }
    // Music first, then the complete chart pair. Only folders this import created are removed
    // again, and only while they are empty.
    void Publish(const OsuPlacement &plan, const fs::path &stage)
    {
        const auto musicFolder = plan.musicFile.parent_path(), patternFolder = plan.patternFile.parent_path();
        const auto chartFolder = patternFolder.parent_path();
        const bool songCreated = fs::create_directories(plan.songFolder);
        bool chartCreated = false, musicPublished = false;
        try
        {
            if (plan.newSong)
            {
                fs::rename(stage / L"music", musicFolder);
                musicPublished = true;
            }
            chartCreated = fs::create_directory(chartFolder);
            fs::rename(stage / L"pattern", patternFolder);
        }
        catch (...)
        {
            std::error_code ignored;
            if (musicPublished)
            {
                fs::remove(plan.musicFile, ignored);
                fs::remove(musicFolder / plan.audioName, ignored);
                fs::remove(musicFolder, ignored);
            }
            if (chartCreated)
                fs::remove(chartFolder, ignored);
            if (songCreated)
                fs::remove(plan.songFolder, ignored);
            throw;
        }
    }
    [[noreturn]] void FailFileSystem(const fs::filesystem_error &error)
    {
        Fail("File system error " + std::to_string(error.code().value()) + ": " + Utf8(error.path1()));
    }
} // namespace

OsuPlacement PlanOsuPlacement(const fs::path &songsRoot, const ConvertedOsuChart &chart)
{
    try
    {
        return Resolve(fs::weakly_canonical(fs::absolute(songsRoot)), chart);
    }
    catch (const fs::filesystem_error &error)
    {
        OsuPlacement plan;
        plan.problem = "File system error " + std::to_string(error.code().value()) + ": " + Utf8(error.path1());
        return plan;
    }
    catch (const std::exception &error)
    {
        OsuPlacement plan;
        plan.problem = error.what();
        return plan;
    }
}

OsuPlacement PlaceConvertedOsuChart(const fs::path &songsRoot, const ConvertedOsuChart &chart)
{
    try
    {
        const auto root = fs::weakly_canonical(fs::absolute(songsRoot));
        // Declared before the staging owners so their rollback finishes before the root unlocks.
        SongsImportLock lock(root);
        const auto plan = Resolve(root, chart);
        if (plan.chartExists)
            Fail("This chart set folder already exists: " + Utf8(plan.patternFile.parent_path()));
        StagingFolder stage(root.parent_path());
        StagedBackground background(chart, plan.songFolder, stage.path);
        StageChart(chart, plan, background.Name(), stage.path);
        if (plan.newSong)
            StageMusic(chart, plan, stage.path);
        background.Publish();
        Publish(plan, stage.path);
        background.Commit();
        return plan;
    }
    catch (const fs::filesystem_error &error)
    {
        FailFileSystem(error);
    }
}
