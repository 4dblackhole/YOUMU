#include "Catalog/SongCatalog.h"

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

    struct CatalogFiles final
    {
        std::vector<std::filesystem::directory_entry> music;
        std::vector<std::filesystem::directory_entry> patterns;
        std::map<std::filesystem::path, std::filesystem::directory_entry> metadata;
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
            if constexpr (requires { parsed.document.notes; })
            {
                if (parsed.Succeeded() && countNotes)
                    noteCount = countNotes(parsed.document);
                if (parsed.Succeeded())
                    try { bpmRange = MusicalTimeline(parsed.document, {}, stop).BpmRange(); }
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
                std::make_shared<const Parsed>(std::move(parsed)), noteCount, bpmRange};
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

    [[nodiscard]] CatalogFiles DiscoverCatalogFiles(
        const std::filesystem::path& songsRoot,
        SongCatalogLoadResult& result, std::stop_token stop)
    {
        CheckChartLoadCancellation(stop);
        CatalogFiles files;
        std::error_code error;
        std::filesystem::recursive_directory_iterator iterator(
            songsRoot,
            std::filesystem::directory_options::skip_permission_denied,
            error);
        const std::filesystem::recursive_directory_iterator end;
        if (error)
        {
            AddCatalogError(
                result,
                songsRoot,
                "Unable to scan the Songs catalog: " + error.message());
            return files;
        }

        while (iterator != end)
        {
            CheckChartLoadCancellation(stop);
            const std::filesystem::path path = iterator->path();
            std::error_code typeError;
            if (iterator->is_regular_file(typeError))
            {
                const std::wstring extension = LowercaseExtension(path);
                if (extension == L".ymm")
                {
                    files.music.push_back(*iterator);
                }
                else if (extension == L".ymp")
                {
                    files.patterns.push_back(*iterator);
                }
                if (extension == L".ymm" || extension == L".ymp" || extension == L".yme")
                    files.metadata.emplace(path, *iterator);
            }
            else if (typeError)
            {
                AddCatalogError(
                    result,
                    path,
                    "Unable to inspect a catalog entry: " +
                        typeError.message());
            }

            iterator.increment(error);
            if (error)
            {
                AddCatalogError(
                    result,
                    path,
                    "Unable to continue scanning a catalog directory: " +
                        error.message());
                error.clear();
            }
        }

        const auto byPath = [](const auto &entry) -> const std::filesystem::path & { return entry.path(); };
        const auto less = [stop](const auto& left, const auto& right)
        {
            CheckChartLoadCancellation(stop);
            return left < right;
        };
        std::ranges::sort(files.music, less, byPath);
        std::ranges::sort(files.patterns, less, byPath);
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
    std::map<std::string, std::size_t, std::less<>> songByMetadataPath;
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
    result.discoveredMusicFiles = files.music.size();
    result.discoveredPatternFiles = files.patterns.size();

    // Load music first so pattern references can be joined in one pass.
    for (const auto &file : files.music)
    {
        CheckChartLoadCancellation(stop);
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
        const std::size_t index = result.songs.size();
        songByMetadataPath.emplace(PathKey(path), index);
        result.songs.push_back(std::move(song));
        if (progress) progress(result.songs.back());
    }

    for (const auto &file : files.patterns)
    {
        CheckChartLoadCancellation(stop);
        const auto &path = file.path();
        std::shared_ptr<const ParseResult<PatternDocument>> parsed;
        std::optional<std::size_t> noteCount;
        std::optional<std::pair<double, double>> bpmRange;
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

        const std::filesystem::path referencedMetadata =
            songsRoot / parsed->document.musicMetadataFile;
        const auto target = songByMetadataPath.find(
            PathKey(referencedMetadata));
        if (target == songByMetadataPath.end())
        {
            result.diagnostics.push_back({
                DiagnosticSeverity::Error,
                {path, 1, 1},
                "The pattern references a Music metadata file that was not found."});
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
        auto& song = result.songs[target->second];
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
