#include "TestSupport.h"
#include "TestCases.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include "GameScene/MusicSelectScene/Submodules/SongFavoritesJson.h"
#include <Windows.h>
#include <iterator>

namespace tests
{
    namespace
    {
        std::string Read(const std::filesystem::path &path)
        {
            std::ifstream file(path, std::ios::binary);
            return {std::istreambuf_iterator<char>{file}, {}};
        }
    }
    void TestSongFavoritesPersistence()
    {
        const auto root = std::filesystem::path{"build"} / ("favorites-logic-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(root);
        const auto path = root / L"favorites.json";
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        const auto pattern = root / L"天使の夢 한글.ymp";
        WriteTextFile(pattern, "chart bytes must remain untouched");
        const auto key = SongFavorites::PatternKey(pattern);
        Require(key == SongFavorites::PatternKey(root / L".\\天使の夢 한글.YMP"), "Windows path case and lexical aliases have one identity.");
        SongFavorites store(path);
        std::string error;
        Require(store.Load(error) && !std::filesystem::exists(path), "Missing preferences load as empty without writing a file.");
        auto values = store.Values();
        const auto first = values.Create(L" 練習 한글 \"A\" ");
        const auto second = values.Create(L"Favorites 🎵");
        values.SetMembership(first, key, true);
        values.SetMembership(second, key, true);
        Require(store.Save(values, error), "Save Unicode favorite folders.");
        const auto serialized = Read(path);
        SongFavorites reload(path);
        Require(reload.Load(error) && reload.Values().Find(first)->name == L"練習 한글 \"A\"" &&
                    reload.Values().Find(first)->patternKeys.contains(key) && reload.Values().Find(second)->patternKeys.contains(key),
                "UTF-8 names, JSON escapes and multiple folder membership round-trip across instances.");
        values.Rename(first, L"Renamed");
        values.SetMembership(first, key, false);
        Require(values.Find(second)->patternKeys.contains(key), "Removing one membership leaves other folders unchanged.");
        values.Remove(first);
        Require(values.Create(L"New") > second && values.Find(second)->patternKeys.contains(key), "Deletion preserves other folders and IDs are never reused.");
        Require(store.Save(values, error) && Read(pattern) == "chart bytes must remain untouched", "Folder changes never rewrite chart data.");
        values = store.Values();
        const auto valid = Read(path);
        values.nextId = 0;
        const auto revision = store.Revision();
        Require(!store.Save(values, error) && Read(path) == valid && store.Revision() == revision,
                "Invalid candidate preserves both the file and published state.");
        HANDLE lock = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(lock != INVALID_HANDLE_VALUE, "Lock favorite destination for write-failure regression.");
        const bool saved = store.Save(store.Values(), error);
        CloseHandle(lock);
        Require(!saved && Read(path) == valid && store.Revision() == revision, "Atomic replacement failure preserves published preferences.");

        const std::vector<std::string> invalid{
            "{", "{\"version\":2,\"nextId\":1,\"folders\":[]}",
            "{\"version\":1,\"version\":1,\"nextId\":1,\"folders\":[]}",
            "{\"version\":1,\"nextId\":01,\"folders\":[]}",
            "{\"version\":1,\"nextId\":1,\"folders\":[],\"extra\":0}",
            "{\"version\":1,\"nextId\":2,\"folders\":[{\"id\":1,\"name\":\"\\uD800\",\"patterns\":[]}]}",
            "{\"version\":1,\"nextId\":2,\"folders\":[{\"id\":1,\"name\":\"\xFF\",\"patterns\":[]}]}",
            "{\"version\":1,\"nextId\":2,\"folders\":[{\"id\":1,\"name\":\"A\",\"patterns\":[]},{\"id\":1,\"name\":\"B\",\"patterns\":[]}]}",
            "{\"version\":1,\"nextId\":2,\"folders\":[{\"id\":1,\"name\":\"A\",\"patterns\":[\"relative.ymp\"]}]}",
            "{\"version\":1,\"nextId\":1,\"folders\":[],}",
            "{\"version\":1,\"nextId\":18446744073709551616,\"folders\":[]}"};
        for (const auto &source : invalid)
        {
            WriteTextFile(path, source);
            Require(!store.Load(error) && !error.empty() && store.Values().folders.size() == 2,
                    "Malformed or future-version data preserves the last valid model.");
            Require(!store.Save(store.Values(), error) && Read(path) == source,
                    "A failed load prevents overwriting the user's original preference file.");
        }
        WriteTextFile(path, valid);
        Require(store.Load(error) && store.LoadWarning().empty() && store.Save(store.Values(), error), "A corrected file can be loaded and saved again.");
        const auto escaped = ReadSongFavoritesJson("{\"folders\":[{\"patterns\":[],\"name\":\"\\uD83C\\uDFB5\\uD55C\",\"id\":1}],\"nextId\":2,\"version\":1}");
        Require(escaped.folders[0].name == L"🎵한", "JSON member order and Unicode surrogate pairs are supported.");
        Require(serialized.find("\\\"A\\\"") != std::string::npos, "Writer escapes quotation marks.");
        std::filesystem::remove(path, ignored);
        std::filesystem::remove(pattern, ignored);
        std::filesystem::remove(root, ignored);
    }

    void TestSongFavoriteSelection()
    {
        SongSelectionState state;
        SongCatalogEntry song, other, empty;
        song.music.names = {"Angel Dream"};
        song.patterns.resize(3);
        for (std::size_t i = 0; i < 3; ++i)
        {
            song.patterns[i].patternPath = std::filesystem::path{"build"} / (std::to_string(i) + ".ymp");
            song.patterns[i].noteCount = i * 100;
        }
        other.music.names = {"Other"};
        other.patterns = {song.patterns[1]};
        other.patterns[0].patternPath = "build/other.ymp";
        state.catalog_.songs = {song, other, empty};
        SongFavoritesValues values;
        const auto first = values.Create(L"Practice"), second = values.Create(L"Empty");
        values.SetMembership(first, SongFavorites::PatternKey(song.patterns[0].patternPath), true);
        values.SetMembership(first, SongFavorites::PatternKey(song.patterns[2].patternPath), true);
        values.SetMembership(first, SongFavorites::PatternKey(other.patterns[0].patternPath), true);
        std::string error;
        Require(state.favorites_.Save(values, error), "Seed favorite selection.");
        state.favoriteFolder_ = first;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0, 1} &&
                    state.VisiblePatternIndices(0) == std::vector<std::size_t>{0, 2}, "Folder shows only member patterns and their songs.");
        state.MoveDifficultyFocus(1);
        Require(state.selectedPatternIndex_ == 2, "Folder navigation skips hidden original indices.");
        state.SelectPattern(1);
        Require(state.selectedPatternIndex_ == 2, "Nonmember patterns cannot be selected.");
        state.filter_.minimumNotes = 150;
        state.searchText_ = L"angel dream";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0} &&
                    state.VisiblePatternIndices(0) == std::vector<std::size_t>{2}, "Folder, metadata search and numeric filter combine with AND.");
        state.favoriteFolder_ = second;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_.empty(), "Empty folder has no invented results.");
        values.Remove(second);
        Require(state.favorites_.Save(values, error), "Remove selected folder.");
        state.filter_ = {};
        state.searchText_.clear();
        state.RebuildVisibleSongs();
        Require(state.favoriteFolder_ == 0 && state.visibleSongIndices_.size() == 3 && state.VisiblePatternCount(0) == 3,
                "Deleting the active folder returns to All and restores songs without patterns.");
        Require(state.catalog_.songs[0].patterns.size() == 3, "Favorites never destructively trim the source catalog.");
    }
}
