#pragma once
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct SongFavoriteFolder
{
    std::uint64_t id{};
    std::wstring name;
    std::set<std::string> patternKeys;
};

struct SongFavoritesValues
{
    std::uint64_t nextId{1};
    std::vector<SongFavoriteFolder> folders;
    [[nodiscard]] const SongFavoriteFolder *Find(std::uint64_t id) const noexcept;
    [[nodiscard]] SongFavoriteFolder *Find(std::uint64_t id) noexcept;
    std::uint64_t Create(std::wstring name);
    void Rename(std::uint64_t id, std::wstring name);
    void Remove(std::uint64_t id);
    void SetMembership(std::uint64_t id, const std::string &patternKey, bool member);
    static std::wstring FolderName(std::wstring_view name);
};

// Owns user preferences separately from chart data. An empty path is in-memory.
class SongFavorites final
{
  public:
    explicit SongFavorites(std::filesystem::path path = {}) : path_(std::move(path)) {}
    [[nodiscard]] const SongFavoritesValues &Values() const noexcept { return values_; }
    [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
    [[nodiscard]] const std::string &LoadWarning() const noexcept { return loadWarning_; }
    [[nodiscard]] bool Load(std::string &error);
    [[nodiscard]] bool Save(SongFavoritesValues candidate, std::string &error);
    // Lexically normalized absolute Windows path, invariant lowercase, UTF-8.
    // Does not read the chart or resolve filesystem links.
    [[nodiscard]] static std::string PatternKey(const std::filesystem::path &path);

  private:
    std::filesystem::path path_;
    SongFavoritesValues values_;
    std::string loadWarning_;
    std::uint64_t revision_{};
};
