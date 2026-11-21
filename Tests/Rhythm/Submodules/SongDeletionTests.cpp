#include "TestSupport.h"
#include "TestCases.h"
#include "GameFlow/SongDeletion.h"
#include <Windows.h>

namespace tests
{
    void TestSongDeletionPlan()
    {
        const TemporaryDirectory fixture("song-deletion");
        const auto root = fixture.Path() / L"Songs";
        const auto folder = root / L"Artist - Title";
        WriteTextFile(folder / L"Music/title.ymm", "Version: 1\n");
        // Set1 holds two charts that share one YME; Set2 holds a single chart with its own YME.
        for (const auto *file : {L"Chart/Set1/hard.ymp", L"Chart/Set1/oni.ymp", L"Chart/Set1/shared.yme",
                                 L"Chart/Set2/easy.ymp", L"Chart/Set2/easy.yme"})
            WriteTextFile(folder / file, "chart");
        SongCatalogEntry song;
        song.metadataPath = folder / L"Music/title.ymm";
        const auto chart = [&](const wchar_t *pattern, const wchar_t *effect) {
            SongCatalogPattern entry;
            entry.patternPath = folder / pattern;
            entry.effectPath = folder / effect;
            return entry;
        };
        song.patterns = {chart(L"Chart/Set1/hard.ymp", L"Chart/Set1/shared.yme"),
                         chart(L"Chart/Set1/oni.ymp", L"Chart/Set1/shared.yme"),
                         chart(L"Chart/Set2/easy.ymp", L"Chart/Set2/easy.yme")};

        const auto hard = PlanDifficultyDeletion(song, 0, root);
        Require(!hard.wholeSong && hard.paths == std::vector<std::filesystem::path>{folder / L"Chart/Set1/hard.ymp"},
                "A YME another chart still uses stays when one difficulty goes.");
        const auto easy = PlanDifficultyDeletion(song, 2, root);
        Require(easy.paths == std::vector<std::filesystem::path>{folder / L"Chart/Set2"},
                "A chart set left empty goes as one folder.");
        Require(DeletionListing(easy, root) == L"Artist - Title/Chart/Set2/", "The listing shows paths under Songs.");
        WriteTextFile(folder / L"Chart/Set2/notes.txt", "user file");
        Require(PlanDifficultyDeletion(song, 2, root).paths.size() == 2,
                "Other files in the chart set keep the folder; only the chart's files go.");

        const auto whole = PlanSongDeletion(song, root);
        Require(whole.wholeSong && whole.paths == std::vector<std::filesystem::path>{folder}, "A song goes as its folder.");
        // Letter case does not decide what is inside Songs.
        std::wstring upper = root.wstring();
        CharUpperBuffW(upper.data(), static_cast<DWORD>(upper.size()));
        Require(PlanSongDeletion(song, upper).paths.size() == 1, "Songs is compared without letter case.");

        const auto refused = [&](auto plan) {
            try
            {
                static_cast<void>(plan());
            }
            catch (const std::invalid_argument &)
            {
                return true;
            }
            return false;
        };
        SongCatalogEntry outside = song;
        outside.metadataPath = fixture.Path() / L"Elsewhere/Music/title.ymm";
        Require(refused([&] { return PlanSongDeletion(outside, root); }), "A song outside Songs is never deleted.");
        SongCatalogEntry rootSong = song;
        rootSong.metadataPath = root / L"Music/title.ymm";
        Require(refused([&] { return PlanSongDeletion(rootSong, root); }), "The Songs folder itself is never deleted.");
        Require(refused([&] { return PlanDifficultyDeletion(song, 3, root); }), "A missing difficulty is refused.");
        Require(std::filesystem::exists(folder / L"Chart/Set1/hard.ymp"), "Planning deletes nothing.");
    }
}
