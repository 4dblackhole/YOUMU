#include "SongFavorites.h"
#include "SongFavoritesJson.h"
#include "Catalog/ChartHash.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace
{
    void ReplaceFile(const std::filesystem::path &path, const std::string &source)
    {
        static std::atomic_uint64_t sequence{};
        std::filesystem::path temporary;
        HANDLE file = INVALID_HANDLE_VALUE;
        bool created = false;
        try
        {
            do
            {
                temporary = path;
                temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence) + L".tmp";
                file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            } while (file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS);
            if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create a temporary collection file.");
            created = true;
            std::size_t offset{};
            while (offset < source.size())
            {
                const auto count = static_cast<DWORD>((std::min)(source.size() - offset, static_cast<std::size_t>(MAXDWORD)));
                DWORD written{};
                if (!WriteFile(file, source.data() + offset, count, &written, nullptr) || written == 0)
                    throw std::runtime_error("Cannot write a collection file.");
                offset += written;
            }
            if (!FlushFileBuffers(file)) throw std::runtime_error("Cannot flush a collection file.");
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot replace a collection file.");
        }
        catch (...)
        {
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
            if (created) DeleteFileW(temporary.c_str());
            throw;
        }
    }

    // Error texts are UTF-8; characters that cannot be converted become U+FFFD.
    std::string Utf8Text(std::wstring_view text)
    {
        if (text.empty() || text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
        const auto size = static_cast<int>(text.size());
        const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), size, nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>((std::max)(count, 0)), '\0');
        if (count > 0) WideCharToMultiByte(CP_UTF8, 0, text.data(), size, result.data(), count, nullptr, nullptr);
        return result;
    }

    std::string ReadWholeFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open " + Utf8Text(path.filename().wstring()) + ".");
        std::string source{std::istreambuf_iterator<char>{file}, {}};
        if (file.bad()) throw std::runtime_error("Cannot read " + Utf8Text(path.filename().wstring()) + ".");
        return source;
    }

    std::wstring WidePath(const std::string &utf8)
    {
        if (utf8.empty() || utf8.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
        const auto size = static_cast<int>(utf8.size());
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), size, nullptr, 0);
        if (count == 0) return {};
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), size, result.data(), count)) return {};
        return result;
    }

    // Collections are listed by name, letter case aside and numbers by value ("Folder 2" before "Folder 10").
    bool NameBefore(const SongFavoriteFolder &first, const SongFavoriteFolder &second)
    {
        const int order = CompareStringEx(LOCALE_NAME_INVARIANT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS, first.name.data(),
                                          static_cast<int>(first.name.size()), second.name.data(),
                                          static_cast<int>(second.name.size()), nullptr, nullptr, 0);
        return order == CSTR_LESS_THAN;
    }

    // File names compare without letter case, as Windows does.
    std::wstring FileKey(const std::filesystem::path &path)
    {
        std::wstring key = path.filename().wstring();
        CharLowerBuffW(key.data(), static_cast<DWORD>(key.size()));
        return key;
    }

    bool IsJsonFile(const std::filesystem::directory_entry &entry)
    {
        std::error_code error;
        if (!entry.is_regular_file(error)) return false;
        return FileKey(entry.path().extension()) == L".json";
    }

    bool IsDeviceName(std::wstring_view stem)
    {
        std::wstring base(stem.substr(0, stem.find(L'.')));
        CharUpperBuffW(base.data(), static_cast<DWORD>(base.size()));
        constexpr std::array Fixed{L"CON", L"PRN", L"AUX", L"NUL"};
        if (std::ranges::find(Fixed, base) != Fixed.end()) return true;
        const bool numbered = base.size() == 4 && base[3] >= L'1' && base[3] <= L'9';
        return numbered && (base.starts_with(L"COM") || base.starts_with(L"LPT"));
    }

    void JoinCharts(SongFavoriteFolder &into, const SongFavoriteFolder &from)
    {
        for (const auto &[hash, label] : from.charts)
        {
            auto &kept = into.charts[hash];
            if (kept == SongCollectionChart{}) kept = label;
        }
    }
} // namespace

const SongFavoriteFolder *SongFavoritesValues::Find(std::uint64_t id) const noexcept
{
    const auto found = std::ranges::find(folders, id, &SongFavoriteFolder::id);
    return found == folders.end() ? nullptr : &*found;
}
SongFavoriteFolder *SongFavoritesValues::Find(std::uint64_t id) noexcept
{
    const auto found = std::ranges::find(folders, id, &SongFavoriteFolder::id);
    return found == folders.end() ? nullptr : &*found;
}
const SongFavoriteFolder *SongFavoritesValues::FindName(std::wstring_view name) const noexcept
{
    const auto found = std::ranges::find_if(folders, [name](const auto &folder) { return SameName(folder.name, name); });
    return found == folders.end() ? nullptr : &*found;
}
bool SongFavoritesValues::SameName(std::wstring_view first, std::wstring_view second) noexcept
{
    const bool fits = first.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) &&
                      second.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max());
    return fits && CompareStringOrdinal(first.data(), static_cast<int>(first.size()), second.data(),
                                        static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
}
std::wstring SongFavoritesValues::FolderName(std::wstring_view name)
{
    while (!name.empty() && std::iswspace(name.front())) name.remove_prefix(1);
    while (!name.empty() && std::iswspace(name.back())) name.remove_suffix(1);
    if (name.empty() || std::ranges::any_of(name, [](wchar_t ch) { return ch < 32; }))
        throw std::invalid_argument("Enter a collection name without control characters.");
    return std::wstring{name};
}
void SongFavoritesValues::RequireNameAvailable(std::wstring_view name, std::uint64_t except) const
{
    const auto *found = FindName(name);
    if (found && found->id != except) throw std::invalid_argument("Another collection already has this name.");
}
std::uint64_t SongFavoritesValues::Create(std::wstring name)
{
    name = FolderName(name);
    RequireNameAvailable(name);
    if (nextId == 0 || nextId == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Collection ID capacity reached.");
    const auto id = nextId;
    folders.push_back({id, std::move(name), {}});
    ++nextId;
    return id;
}
void SongFavoritesValues::Rename(std::uint64_t id, std::wstring name)
{
    auto *folder = Find(id);
    if (!folder) throw std::invalid_argument("The collection does not exist.");
    name = FolderName(name);
    RequireNameAvailable(name, id);
    folder->name = std::move(name);
}
void SongFavoritesValues::Remove(std::uint64_t id)
{
    std::erase_if(folders, [id](const auto &folder) { return folder.id == id; });
}
void SongFavoritesValues::SetMembership(std::uint64_t id, const std::string &chartHash, bool member, SongCollectionChart label)
{
    auto *folder = Find(id);
    if (!folder || !IsChartHash(chartHash)) throw std::invalid_argument("Select a pattern and a collection.");
    if (member) folder->charts.insert_or_assign(chartHash, std::move(label));
    else folder->charts.erase(chartHash);
}
std::uint64_t SongFavoritesValues::Merge(SongFavoriteFolder collection)
{
    collection.name = FolderName(collection.name);
    if (const auto *same = FindName(collection.name))
    {
        auto *into = Find(same->id);
        JoinCharts(*into, collection);
        return into->id;
    }
    const auto id = Create(collection.name);
    Find(id)->charts = std::move(collection.charts);
    return id;
}

std::wstring SongFavorites::CollectionFileStem(std::wstring_view name)
{
    std::wstring stem;
    for (const wchar_t ch : name)
        stem.push_back(ch < 32 || std::wstring_view(L"<>:\"/\\|?*").find(ch) != std::wstring_view::npos ? L'_' : ch);
    constexpr std::size_t MaximumLength = 100;
    if (stem.size() > MaximumLength)
    {
        stem.resize(MaximumLength);
        // Do not leave half of a surrogate pair at the end.
        if (IS_HIGH_SURROGATE(stem.back())) stem.pop_back();
    }
    while (!stem.empty() && (stem.back() == L' ' || stem.back() == L'.')) stem.pop_back();
    while (!stem.empty() && stem.front() == L' ') stem.erase(stem.begin());
    if (stem.empty()) return L"Collection";
    return IsDeviceName(stem) ? L"_" + stem : stem;
}
SongFavoriteFolder SongFavorites::ReadCollectionFile(const std::filesystem::path &path)
{
    return ReadSongCollectionJson(ReadWholeFile(path));
}
void SongFavorites::WriteCollectionFile(const std::filesystem::path &path, const SongFavoriteFolder &collection)
{
    ReplaceFile(path, WriteSongCollectionJson(collection));
}

struct SongFavorites::Scan
{
    SongFavoritesValues values;
    std::map<std::uint64_t, std::vector<std::filesystem::path>> files;
    std::set<std::filesystem::path> unreadable;
    std::wstring skipped;
};

SongFavorites::Scan SongFavorites::ScanDirectory() const
{
    Scan scan;
    scan.values.nextId = values_.nextId;
    if (!std::filesystem::exists(directory_)) return scan;
    std::vector<std::filesystem::path> paths;
    for (const auto &entry : std::filesystem::directory_iterator(directory_))
        if (IsJsonFile(entry)) paths.push_back(entry.path());
    std::ranges::sort(paths, {}, FileKey);
    for (const auto &path : paths)
    {
        SongFavoriteFolder collection;
        try
        {
            collection = ReadCollectionFile(path);
        }
        catch (const std::exception &)
        {
            scan.unreadable.insert(path);
            scan.skipped += (scan.skipped.empty() ? L"" : L", ") + path.filename().wstring();
            continue;
        }
        if (const auto *same = scan.values.FindName(collection.name))
        {
            // Two files with one name (a shared copy dropped in, say) are one collection. The file
            // named after its collection leads: saving keeps it and folds the others into it.
            auto &joined = *scan.values.Find(same->id);
            JoinCharts(joined, collection);
            auto &joinedFiles = scan.files[same->id];
            const bool ownFile = FileKey(path) == FileKey(CollectionFileStem(collection.name) + L".json");
            if (!ownFile)
            {
                joinedFiles.push_back(path);
                continue;
            }
            joinedFiles.insert(joinedFiles.begin(), path);
            joined.name = collection.name;
            continue;
        }
        // A collection keeps its id across reloads so the chosen tab stays chosen.
        const auto *known = values_.FindName(collection.name);
        collection.id = known ? known->id : scan.values.nextId++;
        scan.files[collection.id].push_back(path);
        scan.values.folders.push_back(std::move(collection));
    }
    std::ranges::sort(scan.values.folders, NameBefore);
    return scan;
}

void SongFavorites::WriteChanges(SongFavoritesValues &candidate)
{
    std::filesystem::create_directories(directory_);
    // Never write over a file that someone still owns or that could not be read.
    std::set<std::wstring> taken;
    for (const auto &path : unreadable_) taken.insert(FileKey(path));
    for (const auto &[id, paths] : files_)
        for (const auto &path : paths) taken.insert(FileKey(path));
    std::map<std::uint64_t, std::vector<std::filesystem::path>> next;
    std::vector<const SongFavoriteFolder *> changed;
    for (const auto &folder : candidate.folders)
    {
        const auto *old = values_.Find(folder.id);
        const auto owned = files_.find(folder.id);
        const bool hasFile = owned != files_.end() && !owned->second.empty();
        const bool sameName = old && old->name == folder.name;
        if (sameName && hasFile)
        {
            next[folder.id] = {owned->second.front()};
            const bool sameCharts = old->charts == folder.charts;
            if (!sameCharts || owned->second.size() > 1) changed.push_back(&folder);
            continue;
        }
        // A renamed collection may reuse its own file (a change of letter case, say).
        std::set<std::wstring> own;
        if (owned != files_.end())
            for (const auto &path : owned->second) own.insert(FileKey(path));
        const auto occupied = [&](const std::filesystem::path &path)
        {
            const auto key = FileKey(path);
            return !own.contains(key) && (taken.contains(key) || std::filesystem::exists(path));
        };
        const auto stem = CollectionFileStem(folder.name);
        auto path = directory_ / (stem + L".json");
        for (int copy = 2; occupied(path); ++copy)
            path = directory_ / (stem + L" (" + std::to_wstring(copy) + L").json");
        taken.insert(FileKey(path));
        next[folder.id] = {path};
        changed.push_back(&folder);
    }
    for (const auto *folder : changed) WriteCollectionFile(next[folder->id].front(), *folder);
    // Every collection is on disk now; only then do the files it no longer uses go.
    std::wstring stale;
    for (const auto &[id, paths] : files_)
        for (const auto &path : paths)
        {
            const auto kept = next.find(id);
            if (kept != next.end() && FileKey(kept->second.front()) == FileKey(path)) continue;
            if (!DeleteFileW(path.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
                stale += (stale.empty() ? L"" : L", ") + path.filename().wstring();
        }
    std::ranges::sort(candidate.folders, NameBefore);
    values_ = candidate;
    files_ = std::move(next);
    if (!stale.empty()) throw std::runtime_error("Cannot remove the old collection files: " + Utf8Text(stale));
}

void SongFavorites::MoveLegacyFile()
{
    const auto folders = ReadLegacyFavoritesJson(ReadWholeFile(legacyFile_));
    auto candidate = values_;
    for (const auto &legacy : folders)
    {
        SongFavoriteFolder collection{0, legacy.name, {}};
        for (const auto &key : legacy.patternPaths)
        {
            // A chart that is gone has no hash; favorites.v1.json still lists it.
            try
            {
                collection.charts.emplace(ChartFileHash(WidePath(key)), SongCollectionChart{});
            }
            catch (const std::exception &)
            {
            }
        }
        candidate.Merge(std::move(collection));
    }
    WriteChanges(candidate);
    const auto folder = legacyFile_.parent_path();
    auto kept = folder / L"favorites.v1.json";
    for (int copy = 2; std::filesystem::exists(kept); ++copy)
        kept = folder / (L"favorites.v1 (" + std::to_wstring(copy) + L").json");
    if (!MoveFileExW(legacyFile_.c_str(), kept.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("The collections were moved, but favorites.json could not be renamed.");
}

bool SongFavorites::Load(std::string &error)
{
    error.clear();
    if (directory_.empty())
    {
        loadWarning_.clear();
        blocked_ = false;
        ++revision_;
        return true;
    }
    try
    {
        auto scan = ScanDirectory();
        values_ = std::move(scan.values);
        files_ = std::move(scan.files);
        unreadable_ = std::move(scan.unreadable);
        blocked_ = false;
        loadWarning_ = scan.skipped.empty() ? std::string{}
                                            : "Skipped unreadable collection files: " + Utf8Text(scan.skipped);
    }
    catch (const std::exception &exception)
    {
        error = loadWarning_ = exception.what();
        blocked_ = true;
        ++revision_;
        return false;
    }
    if (!legacyFile_.empty() && std::filesystem::exists(legacyFile_))
    {
        try
        {
            MoveLegacyFile();
        }
        catch (const std::exception &exception)
        {
            // favorites.json stays where it is and is tried again next time.
            loadWarning_ += (loadWarning_.empty() ? "" : " ") + std::string("favorites.json was not moved: ") + exception.what();
        }
    }
    ++revision_;
    error = loadWarning_;
    return loadWarning_.empty();
}

bool SongFavorites::Save(SongFavoritesValues candidate, std::string &error)
{
    error.clear();
    try
    {
        if (blocked_) throw std::runtime_error(loadWarning_);
        ValidateSongFavorites(candidate);
        if (directory_.empty())
        {
            std::ranges::sort(candidate.folders, NameBefore);
            values_ = std::move(candidate);
        }
        else WriteChanges(candidate);
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        if (!directory_.empty() && !blocked_)
        {
            // Some files may already be written: show what is on disk now.
            std::string ignored;
            static_cast<void>(Load(ignored));
        }
        return false;
    }
}
