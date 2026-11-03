#include "SongFavorites.h"
#include "SongFavoritesJson.h"
#include <Windows.h>
#include <algorithm>
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
            if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create favorites.json temporary file.");
            created = true;
            std::size_t offset{};
            while (offset < source.size())
            {
                const auto count = static_cast<DWORD>(std::min(source.size() - offset, static_cast<std::size_t>(MAXDWORD)));
                DWORD written{};
                if (!WriteFile(file, source.data() + offset, count, &written, nullptr) || written == 0)
                    throw std::runtime_error("Cannot write favorites.json.");
                offset += written;
            }
            if (!FlushFileBuffers(file)) throw std::runtime_error("Cannot flush favorites.json.");
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
            if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot replace favorites.json.");
        }
        catch (...)
        {
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
            if (created) DeleteFileW(temporary.c_str());
            throw;
        }
    }
}

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
std::wstring SongFavoritesValues::FolderName(std::wstring_view name)
{
    while (!name.empty() && std::iswspace(name.front())) name.remove_prefix(1);
    while (!name.empty() && std::iswspace(name.back())) name.remove_suffix(1);
    if (name.empty() || std::ranges::any_of(name, [](wchar_t ch) { return ch < 32; }))
        throw std::invalid_argument("Enter a folder name without control characters.");
    return std::wstring{name};
}
std::uint64_t SongFavoritesValues::Create(std::wstring name)
{
    name = FolderName(name);
    if (nextId == 0 || nextId == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Favorite folder ID capacity reached.");
    const auto id = nextId;
    folders.push_back({id, std::move(name), {}});
    ++nextId;
    return id;
}
void SongFavoritesValues::Rename(std::uint64_t id, std::wstring name)
{
    auto *folder = Find(id);
    if (!folder) throw std::invalid_argument("Favorite folder does not exist.");
    folder->name = FolderName(name);
}
void SongFavoritesValues::Remove(std::uint64_t id)
{
    std::erase_if(folders, [id](const auto &folder) { return folder.id == id; });
}
void SongFavoritesValues::SetMembership(std::uint64_t id, const std::string &patternKey, bool member)
{
    auto *folder = Find(id);
    if (!folder || patternKey.empty()) throw std::invalid_argument("Select a pattern and a favorite folder.");
    if (member) folder->patternKeys.insert(patternKey);
    else folder->patternKeys.erase(patternKey);
}
std::string SongFavorites::PatternKey(const std::filesystem::path &path)
{
    if (path.empty()) return {};
    const auto wide = std::filesystem::absolute(path).lexically_normal().generic_wstring();
    if (wide.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) || wide.find(L'\0') != wide.npos)
        throw std::invalid_argument("Invalid favorite pattern path.");
    const auto size = static_cast<int>(wide.size());
    const int lowerSize = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), size, nullptr, 0, nullptr, nullptr, 0);
    if (lowerSize == 0) throw std::runtime_error("Cannot normalize favorite pattern path.");
    std::wstring lower(static_cast<std::size_t>(lowerSize), L'\0');
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), size, lower.data(), lowerSize, nullptr, nullptr, 0))
        throw std::runtime_error("Cannot normalize favorite pattern path.");
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, lower.data(), lowerSize, nullptr, 0, nullptr, nullptr);
    if (!bytes) throw std::invalid_argument("Invalid Unicode favorite pattern path.");
    std::string result(static_cast<std::size_t>(bytes), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, lower.data(), lowerSize, result.data(), bytes, nullptr, nullptr))
        throw std::runtime_error("Cannot encode favorite pattern path.");
    return result;
}
bool SongFavorites::Load(std::string &error)
{
    error.clear();
    try
    {
        SongFavoritesValues candidate;
        if (!path_.empty() && std::filesystem::exists(path_))
        {
            std::ifstream file(path_, std::ios::binary);
            if (!file) throw std::runtime_error("Cannot open favorites.json.");
            const std::string source{std::istreambuf_iterator<char>{file}, {}};
            if (file.bad()) throw std::runtime_error("Cannot read favorites.json.");
            candidate = ReadSongFavoritesJson(source);
        }
        if (!path_.empty()) values_ = std::move(candidate);
        loadWarning_.clear();
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = loadWarning_ = exception.what();
        return false;
    }
}
bool SongFavorites::Save(SongFavoritesValues candidate, std::string &error)
{
    error.clear();
    try
    {
        if (!loadWarning_.empty()) throw std::runtime_error(loadWarning_);
        const auto source = WriteSongFavoritesJson(candidate);
        if (!path_.empty()) ReplaceFile(path_, source);
        values_ = std::move(candidate);
        ++revision_;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}
