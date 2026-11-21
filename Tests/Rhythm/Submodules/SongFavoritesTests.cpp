#include "TestSupport.h"
#include "TestCases.h"
#include "Catalog/ChartHash.h"
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
        std::vector<std::wstring> JsonFiles(const std::filesystem::path &directory)
        {
            std::vector<std::wstring> names;
            for (const auto &entry : std::filesystem::directory_iterator(directory))
                names.push_back(entry.path().filename().wstring());
            std::ranges::sort(names);
            return names;
        }
        std::string CollectionSource(std::string_view name, std::string_view hash)
        {
            return "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"" + std::string(name) +
                   "\",\"charts\":[{\"sha256\":\"" + std::string(hash) + "\"}]}";
        }
    }
    void TestSongFavoritesPersistence()
    {
        const TemporaryDirectory fixture("collections-logic");
        const auto &root = fixture.Path();
        const auto directory = root / L"Collections";
        const auto pattern = root / L"天使の夢 한글.ymp";
        WriteTextFile(pattern, "chart bytes must remain untouched");
        const auto hash = ChartFileHash(pattern), other = ChartBytesHash("other chart");
        SongFavorites store(directory);
        std::string error;
        Require(store.Load(error) && !std::filesystem::exists(directory), "A missing folder loads as empty without creating it.");
        auto values = store.Values();
        const auto first = values.Create(L" 練習 한글 \"A\" ");
        const auto second = values.Create(L"Favorites 🎵");
        bool rejected = false;
        try { static_cast<void>(values.Create(L"FAVORITES 🎵")); } catch (const std::invalid_argument &) { rejected = true; }
        Require(rejected, "Two collections cannot share a name, whatever the letter case.");
        values.SetMembership(first, hash, true, {L"Angel Dream", L"Takahashi", L"Oni"});
        values.SetMembership(second, hash, true);
        values.SetMembership(second, other, true);
        Require(store.Save(values, error), "Save Unicode collections.");
        Require(JsonFiles(directory) == std::vector<std::wstring>{L"Favorites 🎵.json", L"練習 한글 _A_.json"},
                "Each collection is its own file named after it; quotation marks become '_'.");
        const auto serialized = Read(directory / L"練習 한글 _A_.json");
        Require(serialized.find("\"format\": \"YOUMU collection\"") != std::string::npos &&
                serialized.find("\"sha256\": \"" + hash + "\"") != std::string::npos,
                "A collection file names its format and lists charts by SHA-256.");
        Require(serialized.find("\\\"A\\\"") != std::string::npos && serialized.find("\"pattern\": \"Oni\"") != std::string::npos,
                "The writer escapes quotation marks and keeps chart labels.");
        SongFavorites reload(directory);
        Require(reload.Load(error) && reload.Values().folders.size() == 2, "Collections load from their files.");
        const auto *named = reload.Values().FindName(L"練習 한글 \"A\"");
        Require(named && named->charts.at(hash) == SongCollectionChart{L"Angel Dream", L"Takahashi", L"Oni"},
                "Names, membership and labels round-trip across instances.");
        Require(reload.Values().FindName(L"favorites 🎵")->charts.size() == 2, "A chart can be in several collections.");

        values = store.Values();
        values.Rename(first, L"Renamed");
        Require(store.Save(values, error) && JsonFiles(directory) == std::vector<std::wstring>{L"Favorites 🎵.json", L"Renamed.json"},
                "Renaming writes the new file and removes the old one.");
        values.Rename(first, L"RENAMED");
        Require(store.Save(values, error) && JsonFiles(directory).size() == 2, "A change of letter case keeps one file.");
        values.Remove(first);
        Require(values.Create(L"New") > second && store.Save(values, error), "Deletion keeps other collections and never reuses ids.");
        Require(JsonFiles(directory) == std::vector<std::wstring>{L"Favorites 🎵.json", L"New.json"} &&
                Read(pattern) == "chart bytes must remain untouched", "Deleting removes the file; charts are never rewritten.");

        values = store.Values();
        const auto valid = Read(directory / L"Favorites 🎵.json");
        values.nextId = 0;
        Require(!store.Save(values, error) && Read(directory / L"Favorites 🎵.json") == valid, "An invalid candidate writes nothing.");
        values = store.Values();
        values.SetMembership(second, other, false);
        HANDLE lock = CreateFileW((directory / L"Favorites 🎵.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        Require(lock != INVALID_HANDLE_VALUE, "Lock a collection file for the write-failure regression.");
        const bool saved = store.Save(values, error);
        CloseHandle(lock);
        Require(!saved && Read(directory / L"Favorites 🎵.json") == valid, "A failed replacement keeps the collection file.");
        Require(store.Values().FindName(L"Favorites 🎵")->charts.contains(other), "After a failed save the store shows the files on disk.");

        // A file that cannot be read is reported, left alone and never written over.
        WriteTextFile(directory / L"broken.json", "{");
        Require(!store.Load(error) && !store.LoadWarning().empty() && store.Values().folders.size() == 2,
                "An unreadable file is skipped and named; the other collections still load.");
        values = store.Values();
        static_cast<void>(values.Create(L"broken"));
        Require(store.Save(values, error) && Read(directory / L"broken.json") == "{" && std::filesystem::exists(directory / L"broken (2).json"),
                "A new collection takes another file name instead of the unreadable one.");

        // Two files with one name are one collection; the next save folds them into one file.
        WriteTextFile(directory / L"copy.json", CollectionSource("NEW", ChartBytesHash("shared chart")));
        Require(!store.Load(error) && store.Values().FindName(L"New")->charts.contains(ChartBytesHash("shared chart")),
                "A dropped-in copy with the same name joins the collection.");
        Require(store.Save(store.Values(), error) && !std::filesystem::exists(directory / L"copy.json") &&
                SongFavorites::ReadCollectionFile(directory / L"New.json").charts.size() == 1,
                "Saving keeps the joined charts in one file and removes the copy.");

        // Export and import go through the same file format; a same-named import merges.
        const auto exported = root / L"Shared.json";
        SongFavorites::WriteCollectionFile(exported, *store.Values().FindName(L"Favorites 🎵"));
        auto imported = SongFavorites::ReadCollectionFile(exported);
        Require(imported.name == L"Favorites 🎵" && imported.charts.size() == 2, "An exported collection reads back.");
        values = store.Values();
        imported.charts.emplace(ChartBytesHash("friend's chart"), SongCollectionChart{L"Song", L"", L"Hard"});
        const auto mergedId = values.Merge(imported);
        Require(mergedId == second && values.Find(second)->charts.size() == 3, "Importing a collection with the same name merges it.");
        imported.name = L"From a friend";
        Require(values.Merge(imported) != second && values.FindName(L"From a friend"), "Importing a new name adds a collection.");

        Require(SongFavorites::CollectionFileStem(L"a/b:c*?") == L"a_b_c__" && SongFavorites::CollectionFileStem(L"CON") == L"_CON",
                "File names avoid characters and device names Windows does not allow.");
        Require(SongFavorites::CollectionFileStem(L"Mix...") == L"Mix" && SongFavorites::CollectionFileStem(L" .") == L"Collection",
                "File names drop trailing dots and never end up empty.");

        const auto valid64 = std::string(64, 'a');
        const std::vector<std::string> invalid{
            "{", CollectionSource("A", "xyz"),
            "{\"format\":\"YOUMU collection\",\"version\":2,\"name\":\"A\",\"charts\":[]}",
            "{\"format\":\"other\",\"version\":1,\"name\":\"A\",\"charts\":[]}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"version\":1,\"name\":\"A\",\"charts\":[]}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"A\",\"charts\":[],\"extra\":0}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"\\uD800\",\"charts\":[]}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"\xFF\",\"charts\":[]}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"\",\"charts\":[]}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"A\",\"charts\":[{\"title\":\"no hash\"}]}",
            "{\"format\":\"YOUMU collection\",\"version\":1,\"name\":\"A\",\"charts\":[],}"};
        for (const auto &source : invalid)
        {
            bool refused = false;
            try { static_cast<void>(ReadSongCollectionJson(source)); } catch (const std::exception &) { refused = true; }
            Require(refused, "Malformed or future-version collection files are refused.");
        }
        const auto escaped = ReadSongCollectionJson("{\"charts\":[{\"sha256\":\"" + std::string(64, 'A') +
                                                    "\"}],\"name\":\"\\uD83C\\uDFB5\\uD55C\",\"version\":1,\"format\":\"YOUMU collection\"}");
        Require(escaped.name == L"🎵한" && escaped.charts.contains(valid64), "Member order, surrogate pairs and upper-case hashes are accepted.");
    }

    void TestSongFavoritesMigration()
    {
        const TemporaryDirectory fixture("collections-migration");
        const auto &root = fixture.Path();
        const auto directory = root / L"Collections", legacy = root / L"favorites.json";
        const auto chartA = root / L"Songs/A 한글.ymp", chartB = root / L"Songs/B.ymp";
        WriteTextFile(chartA, "chart A");
        WriteTextFile(chartB, "chart B");
        const auto key = [](const std::filesystem::path &path)
        {
            auto text = std::filesystem::absolute(path).lexically_normal().generic_u8string();
            std::string utf8(text.begin(), text.end());
            std::ranges::transform(utf8, utf8.begin(), [](char ch) { return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch; });
            return utf8;
        };
        const auto source = "{\"version\":1,\"nextId\":4,\"folders\":["
                            "{\"id\":1,\"name\":\"Practice\",\"patterns\":[\"" + key(chartA) + "\",\"" + key(root / L"Songs/gone.ymp") + "\"]},"
                            "{\"id\":3,\"name\":\"\\uD55C\\uAE00\",\"patterns\":[\"" + key(chartB) + "\"]}]}";
        WriteTextFile(legacy, source);
        // A collection already in the folder with the same name (another letter case) takes the old folder's charts.
        WriteTextFile(directory / L"mine.json", CollectionSource("practice", ChartBytesHash("chart C")));
        SongFavorites store(directory, legacy);
        std::string error;
        Require(store.Load(error), "favorites.json moves into the collections folder.");
        const auto *practice = store.Values().FindName(L"Practice");
        Require(practice && practice->charts.size() == 2 && practice->charts.contains(ChartFileHash(chartA)),
                "Old favorites find their charts by file hash and join a collection of the same name.");
        Require(store.Values().FindName(L"한글") && store.Values().FindName(L"한글")->charts.contains(ChartFileHash(chartB)),
                "Every old folder becomes a collection.");
        Require(!std::filesystem::exists(legacy) && Read(root / L"favorites.v1.json") == source,
                "favorites.json is kept unchanged as favorites.v1.json, missing charts included.");
        Require(std::filesystem::exists(directory / L"한글.json"), "Moved folders are written as collection files.");
        Require(store.Load(error) && !std::filesystem::exists(root / L"favorites.v1 (2).json") && store.Values().folders.size() == 2,
                "The move happens once.");

        WriteTextFile(legacy, "{ not json");
        Require(!store.Load(error) && std::filesystem::exists(legacy) && store.Values().folders.size() == 2,
                "An unreadable favorites.json stays where it is and is reported.");
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
            song.patterns[i].chartHash = ChartBytesHash(std::to_string(i));
            song.patterns[i].noteCount = i * 100;
        }
        other.music.names = {"Other"};
        other.patterns = {song.patterns[1]};
        other.patterns[0].patternPath = "build/other.ymp";
        other.patterns[0].chartHash = ChartBytesHash("other");
        state.catalog_.songs = {song, other, empty};
        SongFavoritesValues values;
        const auto first = values.Create(L"Practice"), second = values.Create(L"Empty");
        values.SetMembership(first, song.patterns[0].chartHash, true);
        values.SetMembership(first, song.patterns[2].chartHash, true);
        values.SetMembership(first, other.patterns[0].chartHash, true);
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
