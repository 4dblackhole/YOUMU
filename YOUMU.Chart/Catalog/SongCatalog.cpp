#include "Catalog/SongCatalog.h"
#include "Catalog/ChartHash.h"

#include "Catalog/SongLayout.h"
#include "Parsing/ChartParser.h"
#include "Timing/MusicalTimeline.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    // Song filter Length: the span of the chart's notes, from the first note to the last note entry, so a
    // trailing long note counts to its end. Notes inside removed timing spans do not play and are skipped.
    [[nodiscard]] std::optional<double> ChartLengthSeconds(const MusicalTimeline &timeline,
                                                           const PatternDocument &pattern)
    {
        std::optional<RhythmTime> first, last;
        for (const auto &note : pattern.notes)
        {
            if (!timeline.IsPositionActive(note.position))
                continue;
            const auto time = timeline.Compile(note.position);
            first = first ? std::min(*first, time) : time;
            last = last ? std::max(*last, time) : time;
        }
        if (!first)
            return std::nullopt;
        return std::chrono::duration<double>(*last - *first).count();
    }

    [[nodiscard]] std::string PathKey(
        const std::filesystem::path& path)
    {
        const std::u8string utf8 = path.lexically_normal().generic_u8string();
        std::string key(
            reinterpret_cast<const char*>(utf8.data()),
            utf8.size());
        std::ranges::transform(
            key,
            key.begin(),
            [](const unsigned char value)
            {
                return static_cast<char>(std::tolower(value));
            });
        return key;
    }

    [[nodiscard]] std::string DisplaySortKey(
        const SongCatalogEntry& song)
    {
        std::string value = song.music.names.empty()
            ? PathKey(song.metadataPath.stem())
            : song.music.names.front();
        std::ranges::transform(
            value,
            value.begin(),
            [](const unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    void AppendDiagnostics(
        std::vector<Diagnostic>& destination,
        const std::vector<Diagnostic>& source)
    {
        destination.insert(
            destination.end(),
            source.begin(), source.end());
    }

    void AddCatalogError(
        SongCatalogLoadResult& result,
        const std::filesystem::path& path,
        std::string message)
    {
        result.diagnostics.push_back({
            DiagnosticSeverity::Error,
            {path, 1, 1},
            std::move(message)});
    }

    [[nodiscard]] std::wstring LowercaseExtension(
        const std::filesystem::path& path)
    {
        std::wstring extension = path.extension().wstring();
        std::ranges::transform(
            extension,
            extension.begin(),
            [](const wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            });
        return extension;
    }

    struct SongFolderFiles final
    {
        std::filesystem::path folder;
        // Set only when the Music folder holds exactly one YMM.
        std::optional<std::filesystem::directory_entry> music;
        std::vector<std::filesystem::directory_entry> patterns;
    };

    struct CatalogFiles final
    {
        std::vector<SongFolderFiles> songs;
        std::map<std::filesystem::path, std::filesystem::directory_entry> metadata;
        std::size_t musicCount{};
        std::size_t patternCount{};
    };

    template <class Cache, class Parser>
    auto ParseChangedFile(Cache &cache, const std::filesystem::directory_entry &file, Parser parse,
                         bool &dirty, bool metadataOnly = false,
                         const std::function<std::optional<std::size_t>(const PatternDocument&)> &countNotes = {},
                         std::stop_token stop = {})
    {
        CheckChartLoadCancellation(stop);
        const auto &path = file.path();
        using Parsed = typename Cache::mapped_type::Parsed;
        const auto read = [&](auto modified, auto size)
        {
            CheckChartLoadCancellation(stop);
            auto parsed = parse(path);
            CheckChartLoadCancellation(stop);
            std::optional<std::size_t> noteCount;
            std::optional<std::pair<double, double>> bpmRange;
            std::optional<double> chartLength;
            std::string contentHash;
            if constexpr (requires { parsed.document.notes; })
            {
                try
                {
                    contentHash = ChartFileHash(path);
                }
                catch (const std::exception &)
                {
                    // An unreadable file has already failed parsing; it simply has no identity.
                }
                if (parsed.Succeeded() && countNotes)
                    noteCount = countNotes(parsed.document);
                if (parsed.Succeeded())
                    try
                    {
                        const MusicalTimeline timeline(parsed.document, {}, stop);
                        bpmRange = timeline.BpmRange();
                        chartLength = ChartLengthSeconds(timeline, parsed.document);
                    }
                    catch (const ChartLoadCancelled&) { throw; }
                    catch (const std::exception &)
                    {
                        // Keep the parsed entry available when only its BPM summary fails.
                    }
                if (metadataOnly)
                {
                    decltype(parsed.document.notes){}.swap(parsed.document.notes);
                    decltype(parsed.document.timing){}.swap(parsed.document.timing);
                    decltype(parsed.document.systemBreakMeasures){}.swap(parsed.document.systemBreakMeasures);
                }
            }
            if constexpr (requires { parsed.document.commands; })
                if (metadataOnly) parsed.document = {};
            CheckChartLoadCancellation(stop);
            return typename Cache::mapped_type{modified, size,
                std::make_shared<const Parsed>(std::move(parsed)), noteCount, bpmRange, chartLength,
                std::move(contentHash)};
        };
        std::error_code error;
        const auto modified = file.last_write_time(error);
        if (error)
            return read(std::filesystem::file_time_type{}, std::uintmax_t{});
        const auto size = file.file_size(error);
        if (error)
            return read(std::filesystem::file_time_type{}, std::uintmax_t{});
        if (const auto found = cache.find(path);
            found != cache.end() && found->second.modified == modified && found->second.size == size)
            return found->second;
        auto entry = read(modified, size);
        cache.insert_or_assign(path, entry);
        dirty = true;
        return entry;
    }

    template <class Patterns, class Effects>
    void InvalidateChangedPatternEffect(Patterns& patterns, const Effects& effects,
        const std::filesystem::path& path)
    {
        const auto previous = patterns.find(path);
        if (previous == patterns.end()) return;
        const auto& document = previous->second.parsed->document;
        if (document.effectFile.empty()) return;
        const auto dependency = (path.parent_path() / document.effectFile).lexically_normal();
        const auto cached = effects.find(dependency);
        std::error_code error;
        const std::filesystem::directory_entry file(dependency, error);
        if (error || cached == effects.end()) { patterns.erase(previous); return; }
        const auto modified = file.last_write_time(error);
        if (error || modified != cached->second.modified) { patterns.erase(previous); return; }
        const auto size = file.file_size(error);
        if (error || size != cached->second.size) patterns.erase(previous);
    }

    // One directory level in path order. A folder that cannot be read reports one catalog error.
    [[nodiscard]] std::vector<std::filesystem::directory_entry> ListDirectory(
        const std::filesystem::path& folder, SongCatalogLoadResult& result, std::stop_token stop)
    {
        std::vector<std::filesystem::directory_entry> entries;
        std::error_code error;
        std::filesystem::directory_iterator iterator(
            folder, std::filesystem::directory_options::skip_permission_denied, error);
        for (const std::filesystem::directory_iterator end; !error && iterator != end; iterator.increment(error))
        {
            CheckChartLoadCancellation(stop);
            entries.push_back(*iterator);
        }
        if (error)
            AddCatalogError(result, folder, "Unable to scan a catalog directory: " + error.message());
        std::ranges::sort(entries, {},
            [](const auto& entry) -> const std::filesystem::path& { return entry.path(); });
        return entries;
    }

    // Lowercase extension of a regular file; empty for anything else.
    [[nodiscard]] std::wstring CatalogFileExtension(
        const std::filesystem::directory_entry& entry, SongCatalogLoadResult& result)
    {
        std::error_code error;
        if (entry.is_regular_file(error))
            return LowercaseExtension(entry.path());
        if (error)
            AddCatalogError(result, entry.path(), "Unable to inspect a catalog entry: " + error.message());
        return {};
    }

    [[nodiscard]] bool IsDirectory(const std::filesystem::path& path)
    {
        std::error_code error;
        return std::filesystem::is_directory(path, error);
    }

    // Music holds the song's single YMM; a second YMM would make the song ambiguous.
    void DiscoverMusic(SongFolderFiles& song, CatalogFiles& files, SongCatalogLoadResult& result,
        std::stop_token stop)
    {
        const auto musicFolder = song.folder / SongLayout::MusicFolder;
        std::size_t count{};
        if (IsDirectory(musicFolder))
            for (const auto& entry : ListDirectory(musicFolder, result, stop))
            {
                if (CatalogFileExtension(entry, result) != L".ymm")
                    continue;
                ++count;
                song.music = entry;
                files.metadata.emplace(entry.path(), entry);
            }
        files.musicCount += count;
        if (count == 1)
            return;
        song.music.reset();
        AddCatalogError(result, musicFolder, count == 0
            ? "The song folder needs one YMM file in its Music folder."
            : "The song folder's Music folder holds more than one YMM file.");
    }

    // Patterns are read from Chart/<chart set>/ only; the set folder's name carries no meaning.
    void DiscoverCharts(SongFolderFiles& song, CatalogFiles& files, SongCatalogLoadResult& result,
        std::stop_token stop)
    {
        for (const auto& set : ListDirectory(song.folder / SongLayout::ChartFolder, result, stop))
        {
            if (!IsDirectory(set.path()))
            {
                if (CatalogFileExtension(set, result) == L".ymp")
                    result.diagnostics.push_back({DiagnosticSeverity::Warning, {set.path(), 1, 1},
                        "Skipped a YMP outside a chart set folder; it belongs in Chart/<chart set>/."});
                continue;
            }
            for (const auto& entry : ListDirectory(set.path(), result, stop))
            {
                const std::wstring extension = CatalogFileExtension(entry, result);
                if (extension == L".ymp")
                    song.patterns.push_back(entry);
                if (extension == L".ymp" || extension == L".yme")
                    files.metadata.emplace(entry.path(), entry);
            }
        }
        files.patternCount += song.patterns.size();
    }

    // Every direct child of Songs is one song folder (SongLayout). Nothing is searched recursively,
    // so a pattern's song is decided by where the file is, not by a reference inside it.
    [[nodiscard]] CatalogFiles DiscoverCatalogFiles(
        const std::filesystem::path& songsRoot,
        SongCatalogLoadResult& result, std::stop_token stop)
    {
        CheckChartLoadCancellation(stop);
        CatalogFiles files;
        for (const auto& entry : ListDirectory(songsRoot, result, stop))
        {
            if (!IsDirectory(entry.path()))
                continue;
            SongFolderFiles song{entry.path()};
            const bool hasChart = IsDirectory(song.folder / SongLayout::ChartFolder);
            if (!hasChart && !IsDirectory(song.folder / SongLayout::MusicFolder))
            {
                result.diagnostics.push_back({DiagnosticSeverity::Warning, {song.folder, 1, 1},
                    "Skipped a folder that is not a song folder: it has no Music or Chart folder."});
                continue;
            }
            DiscoverMusic(song, files, result, stop);
            if (hasChart)
                DiscoverCharts(song, files, result, stop);
            files.songs.push_back(std::move(song));
        }
        return files;
    }
}

std::size_t SongCatalogLoadResult::PatternCount() const noexcept
{
    std::size_t count{};
    for (const SongCatalogEntry& song : songs)
    {
        count += song.patterns.size();
    }
    return count;
}

bool SongCatalogLoadResult::Succeeded() const noexcept
{
    return std::ranges::none_of(
        diagnostics,
        [](const Diagnostic& diagnostic)
        {
            return diagnostic.severity == DiagnosticSeverity::Error;
        });
}

SongCatalogLoadResult SongCatalog::Load(
    const std::filesystem::path& songsRoot, std::stop_token stop,
    ProgressHandler progress) const
{
    CheckChartLoadCancellation(stop);
    if (metadataOnly_ && cachedRoot_ != songsRoot)
    {
        musicCache_.clear();
        patternCache_.clear();
        effectCache_.clear();
        ReadIndex(songsRoot, stop);
        cachedRoot_ = songsRoot;
    }
    SongCatalogLoadResult result;
    std::error_code rootError;
    if (!std::filesystem::is_directory(songsRoot, rootError))
    {
        AddCatalogError(
            result,
            songsRoot,
            rootError
                ? "Unable to inspect the Songs catalog directory: " +
                    rootError.message()
                : "The Songs catalog directory does not exist.");
        return result;
    }

    ChartParser parser(stop);
    CatalogFiles files = DiscoverCatalogFiles(songsRoot, result, stop);
    const auto deleted = [&files, stop](const auto &entry)
    {
        CheckChartLoadCancellation(stop);
        if (files.metadata.contains(entry.first)) return false;
        std::error_code error;
        const bool exists = std::filesystem::exists(entry.first, error);
        return !exists && !error;
    };
    cacheDirty_ |= std::erase_if(musicCache_, deleted) != 0;
    cacheDirty_ |= std::erase_if(patternCache_, deleted) != 0;
    cacheDirty_ |= std::erase_if(effectCache_, deleted) != 0;
    result.discoveredMusicFiles = files.musicCount;
    result.discoveredPatternFiles = files.patternCount;

    // Load every song's YMM first so list rows appear before their patterns. A song folder
    // whose YMM is unusable has already reported why, so its patterns are not read.
    std::vector<std::pair<const std::filesystem::directory_entry*, std::size_t>> patterns;
    for (const auto &folder : files.songs)
    {
        CheckChartLoadCancellation(stop);
        if (!folder.music)
            continue;
        const auto &file = *folder.music;
        const auto &path = file.path();
        std::shared_ptr<const ParseResult<MusicDocument>> parsed;
        try
        {
            parsed = ParseChangedFile(musicCache_, file,
                                      [&parser](const auto &file) { return parser.ParseMusicFile(file); }, cacheDirty_, false, {}, stop).parsed;
        }
        catch (const ChartLoadCancelled&) { throw; }
        catch (const std::exception& exception)
        {
            AddCatalogError(result, path, exception.what());
            continue;
        }
        const bool valid = parsed->Succeeded();
        AppendDiagnostics(result.diagnostics, parsed->diagnostics);
        if (!valid)
        {
            continue;
        }

        SongCatalogEntry song;
        song.metadataPath = path;
        song.audioPath = path.parent_path() /
            parsed->document.audioFile;
        std::error_code audioError;
        const bool audioExists =
            std::filesystem::exists(song.audioPath, audioError);
        if (audioError || !audioExists)
        {
            AddCatalogError(
                result,
                song.audioPath,
                audioError
                    ? "Unable to inspect the referenced audio file: " +
                        audioError.message()
                    : "The referenced audio file does not exist.");
            continue;
        }
        std::error_code audioTypeError;
        if (!std::filesystem::is_regular_file(
                song.audioPath,
                audioTypeError))
        {
            AddCatalogError(
                result,
                song.audioPath,
                audioTypeError
                    ? "Unable to inspect the referenced audio file: " +
                        audioTypeError.message()
                    : "The referenced audio path is not a regular file.");
            continue;
        }
        song.music = parsed->document;
        for (const auto &pattern : folder.patterns)
            patterns.emplace_back(&pattern, result.songs.size());
        result.songs.push_back(std::move(song));
        if (progress) progress(result.songs.back());
    }

    for (const auto &[patternFile, songIndex] : patterns)
    {
        CheckChartLoadCancellation(stop);
        const auto &file = *patternFile;
        const auto &path = file.path();
        std::shared_ptr<const ParseResult<PatternDocument>> parsed;
        std::optional<std::size_t> noteCount;
        std::optional<std::pair<double, double>> bpmRange;
        std::optional<double> chartLength;
        std::string chartHash;
        try
        {
            InvalidateChangedPatternEffect(patternCache_, effectCache_, path);
            const auto countNotes = [&](const PatternDocument& document) -> std::optional<std::size_t>
            {
                if (!countNotes_) return std::nullopt;
                EffectDocument effects;
                if (!document.effectFile.empty())
                {
                    auto parsedEffects = parser.ParseEffectFile(
                        (path.parent_path() / document.effectFile).lexically_normal());
                    if (!parsedEffects.Succeeded()) return std::nullopt;
                    effects = std::move(parsedEffects.document);
                }
                return countNotes_(document, effects, stop);
            };
            const auto cached = ParseChangedFile(patternCache_, file,
                [&parser](const auto &file) { return parser.ParsePatternFile(file); },
                cacheDirty_, metadataOnly_, countNotes, stop);
            parsed = cached.parsed;
            noteCount = cached.noteCount;
            bpmRange = cached.bpmRange;
            chartLength = cached.chartLengthSeconds;
            chartHash = cached.contentHash;
        }
        catch (const ChartLoadCancelled&) { throw; }
        catch (const std::exception& exception)
        {
            AddCatalogError(result, path, exception.what());
            continue;
        }
        const bool valid = parsed->Succeeded();
        AppendDiagnostics(result.diagnostics, parsed->diagnostics);
        if (!valid)
        {
            continue;
        }

        SongCatalogPattern pattern;
        pattern.patternPath = path;
        const std::filesystem::path effectPath =
            parsed->document.effectFile.empty() ? std::filesystem::path{} :
            (path.parent_path() / parsed->document.effectFile).lexically_normal();
        std::error_code effectError;
        const auto discoveredEffect = files.metadata.find(effectPath);
        const bool effectExists =
            discoveredEffect != files.metadata.end() ||
            (!effectPath.empty() && std::filesystem::exists(effectPath, effectError));
        if (effectError)
        {
            AddCatalogError(
                result,
                effectPath,
                "Unable to inspect the matching effect file: " +
                    effectError.message());
            continue;
        }
        if (!effectPath.empty() && !effectExists)
        {
            AddCatalogError(result, effectPath, "Referenced Effect file was not found.");
            continue;
        }
        if (effectExists)
        {
            std::error_code effectTypeError;
            if (discoveredEffect == files.metadata.end() && !std::filesystem::is_regular_file(
                    effectPath,
                    effectTypeError))
            {
                AddCatalogError(
                    result,
                    effectPath,
                    effectTypeError
                        ? "Unable to inspect the matching effect file: " +
                            effectTypeError.message()
                        : "The matching effect path is not a regular file.");
                continue;
            }
            std::shared_ptr<const ParseResult<EffectDocument>> effects;
            try
            {
                const auto effectFile = discoveredEffect == files.metadata.end()
                    ? std::filesystem::directory_entry{effectPath} : discoveredEffect->second;
                effects = ParseChangedFile(effectCache_, effectFile,
                    [&parser](const auto &file) { return parser.ParseEffectFile(file); }, cacheDirty_, metadataOnly_, {}, stop).parsed;
            }
            catch (const ChartLoadCancelled&) { throw; }
            catch (const std::exception& exception)
            {
                AddCatalogError(result, effectPath, exception.what());
                continue;
            }
            const bool effectsValid = effects->Succeeded();
            AppendDiagnostics(
                result.diagnostics,
                effects->diagnostics);
            if (!effectsValid)
            {
                continue;
            }
            pattern.effectPath = effectPath;
        }
        pattern.pattern = parsed->document;
        pattern.noteCount = noteCount;
        pattern.bpmRange = bpmRange;
        pattern.chartLengthSeconds = chartLength;
        pattern.chartHash = std::move(chartHash);
        auto& song = result.songs[songIndex];
        song.patterns.push_back(std::move(pattern));
        if (progress)
        {
            SongCatalogEntry delta;
            delta.metadataPath = song.metadataPath;
            delta.audioPath = song.audioPath;
            delta.music = song.music;
            delta.patterns.push_back(song.patterns.back());
            progress(std::move(delta));
        }
    }

    for (SongCatalogEntry& song : result.songs)
    {
        CheckChartLoadCancellation(stop);
        std::ranges::stable_sort(
            song.patterns,
            [stop](const SongCatalogPattern& left,
               const SongCatalogPattern& right)
            {
                CheckChartLoadCancellation(stop);
                return left.pattern.name < right.pattern.name;
            });
    }
    // Normalize once per song, rather than allocating strings in every comparison.
    std::vector<std::pair<std::string, std::size_t>> order;
    order.reserve(result.songs.size());
    for (std::size_t i = 0; i < result.songs.size(); ++i)
    {
        CheckChartLoadCancellation(stop);
        order.emplace_back(DisplaySortKey(result.songs[i]), i);
    }
    std::ranges::stable_sort(order, [stop](const auto& left, const auto& right)
    {
        CheckChartLoadCancellation(stop);
        return left.first < right.first;
    });
    std::vector<SongCatalogEntry> sorted;
    sorted.reserve(result.songs.size());
    for (const auto &[key, index] : order)
    {
        CheckChartLoadCancellation(stop);
        sorted.push_back(std::move(result.songs[index]));
    }
    result.songs = std::move(sorted);
    CheckChartLoadCancellation(stop);
    if (metadataOnly_ && cacheDirty_)
        WriteIndex(songsRoot, stop);
    return result;
}
