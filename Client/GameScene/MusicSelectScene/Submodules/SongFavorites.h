#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

// What a collection file says about a chart besides its hash, so a player who receives the file can
// tell which charts they are missing. Every field may be empty.
struct SongCollectionChart
{
    std::wstring title;
    std::wstring artist;
    std::wstring pattern;
    bool operator==(const SongCollectionChart &) const = default;
};

// One collection (shown as a favorites tab). The id only lives while the game runs; files keep the name.
struct SongFavoriteFolder
{
    std::uint64_t id{};
    std::wstring name;
    // Chart hash (Catalog/ChartHash.h, lowercase) to its label.
    std::map<std::string, SongCollectionChart> charts;
};

struct SongFavoritesValues
{
    std::uint64_t nextId{1};
    std::vector<SongFavoriteFolder> folders;
    [[nodiscard]] const SongFavoriteFolder *Find(std::uint64_t id) const noexcept;
    [[nodiscard]] SongFavoriteFolder *Find(std::uint64_t id) noexcept;
    // Letter case does not tell two names apart: each name is also a file name.
    [[nodiscard]] const SongFavoriteFolder *FindName(std::wstring_view name) const noexcept;
    std::uint64_t Create(std::wstring name);
    void Rename(std::uint64_t id, std::wstring name);
    void Remove(std::uint64_t id);
    void SetMembership(std::uint64_t id, const std::string &chartHash, bool member, SongCollectionChart label = {});
    // A collection read from a file joins the one with the same name (its charts are added, labels kept
    // where this side had none); otherwise it becomes a new collection. Returns the id it ends up in.
    std::uint64_t Merge(SongFavoriteFolder collection);
    // Throws when another collection already has the name.
    void RequireNameAvailable(std::wstring_view name, std::uint64_t except = 0) const;
    static std::wstring FolderName(std::wstring_view name);
    [[nodiscard]] static bool SameName(std::wstring_view first, std::wstring_view second) noexcept;
};

// Collections are one JSON file each in a folder (Asset/Collections) so players can copy, send and
// import them. An empty folder keeps everything in memory. The favorites.json of earlier versions
// (paths, all folders in one file) is moved in once by Load and then kept as favorites.v1.json.
class SongFavorites final
{
  public:
    explicit SongFavorites(std::filesystem::path directory = {}, std::filesystem::path legacyFile = {})
        : directory_(std::move(directory)), legacyFile_(std::move(legacyFile))
    {
    }
    [[nodiscard]] const SongFavoritesValues &Values() const noexcept { return values_; }
    [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
    [[nodiscard]] const std::string &LoadWarning() const noexcept { return loadWarning_; }
    [[nodiscard]] const std::filesystem::path &Directory() const noexcept { return directory_; }
    // Reads every collection file. Unreadable files are skipped, reported and never overwritten.
    // Returns false when something was skipped or the folder could not be read.
    [[nodiscard]] bool Load(std::string &error);
    // Writes the collections that changed, then removes the files of deleted or renamed ones.
    [[nodiscard]] bool Save(SongFavoritesValues candidate, std::string &error);

    [[nodiscard]] static SongFavoriteFolder ReadCollectionFile(const std::filesystem::path &path);
    static void WriteCollectionFile(const std::filesystem::path &path, const SongFavoriteFolder &collection);
    // The file name for a collection: characters Windows does not allow become '_'.
    [[nodiscard]] static std::wstring CollectionFileStem(std::wstring_view name);

  private:
    struct Scan;
    [[nodiscard]] Scan ScanDirectory() const;
    void WriteChanges(SongFavoritesValues &candidate);
    void MoveLegacyFile();
    std::filesystem::path directory_;
    std::filesystem::path legacyFile_;
    SongFavoritesValues values_;
    // Each collection's files; more than one when several files had the same name.
    std::map<std::uint64_t, std::vector<std::filesystem::path>> files_;
    std::set<std::filesystem::path> unreadable_;
    std::string loadWarning_;
    bool blocked_{};
    std::uint64_t revision_{};
};
