#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorTool.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include "TestSupport.h"
#include <limits>

namespace tests
{
    void TestSongFilters()
    {
        SongSelectionState state;
        SongCatalogEntry song, other, empty;
        song.music.names = {"Angel Dream"};
        song.patterns.resize(3);
        for (auto &entry : song.patterns)
        {
            entry.pattern.mode = "Taiko";
            entry.noteCount = 100;
            entry.pattern.baseBpm = 150;
            entry.bpmRange = std::pair{1.0, 999.0};
        }
        song.patterns[0].pattern.baseBpm = 90;
        song.patterns[1].pattern.mode = "Other";
        other.music.names = {"Other"};
        other.patterns = {song.patterns[2]};
        state.catalog_.songs = {song, other, empty};
        Require(!song.patterns[0].calculatedDifficulty, "Runtime difficulty starts uncalculated.");
        state.filter_.mode = "Taiko";
        state.filter_.minimumBpm = 140;
        state.filter_.maximumNotes = 100;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0, 1} &&
                    state.VisiblePatternIndices(0) == std::vector<std::size_t>{2},
                "All bounds must match the same pattern; BPM uses the base value, not its range.");
        Require(state.selectedPatternIndex_ == 2, "Selection keeps the real catalog pattern index.");
        state.SelectPattern(0);
        Require(state.selectedPatternIndex_ == 2, "Hidden patterns cannot be selected.");
        state.MoveDifficultyFocus(1);
        Require(state.focusedSongPosition_ == 1 && state.selectedPatternIndex_ == 0,
                "Keyboard navigation crosses filtered songs using actual pattern indices.");
        state.MoveDifficultyFocus(-1);
        Require(state.focusedSongPosition_ == 0 && state.selectedPatternIndex_ == 2, "Reverse filtered navigation.");
        state.searchText_ = L"angel dream";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0}, "Metadata AND search composes with filters.");
        state.filter_.minimumDifficulty = 0;
        state.filter_.maximumDifficulty = 0;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_.empty(), "An unknown calculated difficulty is not zero.");
        state.catalog_.songs[0].patterns[2].calculatedDifficulty = 0;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0}, "Known zero matches inclusive zero bounds.");
        state.catalog_.songs[0].patterns[2].calculatedDifficulty = std::numeric_limits<double>::quiet_NaN();
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_.empty(), "Nonfinite calculated values are unavailable.");
        state.filter_ = {};
        state.searchText_.clear();
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_.size() == 3 && state.VisiblePatternCount(0) == 3,
                "Clearing filters restores all original patterns and songs without patterns.");

        SongFilter parsed;
        SongFilterDraft draft;
        draft.values = {L" 0 ", L"100", L"120.25", L"150", L"1", L"100", L"", L""};
        Require(ParseSongFilter(draft, parsed) == SongFilterError::None &&
                    parsed.minimumNotes == 0 && parsed.minimumBpm == 120.25,
                "Integer count and fractional range bounds accept whitespace and inclusive endpoints.");
        Require(FormatSongFilter(parsed).values == std::array<std::wstring, 8>{L"0", L"100", L"120.25", L"150", L"1", L"100", L"", L""},
                "Committed filters round-trip into editable values.");
        for (const auto bad : {L"-1", L"1.5", L"184467440737095516160", L"日本語"})
        {
            draft.values[0] = bad;
            Require(ParseSongFilter(draft, parsed) == SongFilterError::Notes && parsed.minimumNotes == 0,
                    "Invalid count is rejected without mutating the committed filter.");
        }
        draft = {};
        draft.values[2] = L"nan";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::Bpm, "Reject nonfinite BPM.");
        draft.values[2] = L"-1";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::Bpm, "Reject negative BPM.");
        draft = {};
        draft.values[4] = L"0";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::JudgeLevel, "JudgeLevel lower bound is one.");
        draft.values[4] = L"100.01";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::JudgeLevel, "JudgeLevel upper bound is 100.");
        draft = {};
        draft.values[6] = L"inf";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::Difficulty, "Reject infinite difficulty.");
        draft = {};
        draft.values[0] = L"101";
        draft.values[1] = L"100";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::ReversedRange, "Reject reversed bounds.");
        draft = {};
        Require(ParseSongFilter(draft, parsed) == SongFilterError::None && !parsed.IsActive(), "Empty bounds remove restrictions.");
    }

    void TestSongBaseBpmSort()
    {
        SongSelectionState state;
        SongCatalogEntry fast, slow, multiple, tied, missing, invalid;
        fast.music.names = {"Fast match"};
        slow.music.names = {"Slow match"};
        fast.patterns.resize(1);
        fast.patterns[0].pattern.baseBpm = 240;
        slow.patterns.resize(1);
        slow.patterns[0].pattern.baseBpm = 90.5;
        multiple.patterns.resize(2);
        multiple.patterns[0].pattern.baseBpm = 150;
        multiple.patterns[0].bpmRange = std::pair{1.0, 999.0};
        multiple.patterns[1].pattern.baseBpm = 10;
        tied.patterns.resize(1);
        tied.patterns[0].pattern.baseBpm = 150;
        invalid.patterns.resize(1);
        invalid.patterns[0].pattern.baseBpm = std::numeric_limits<double>::quiet_NaN();
        state.catalog_.songs = {fast, slow, multiple, tied, missing, invalid};
        state.RebuildVisibleSongs();
        state.sortMode_ = SongSelectionState::SortMode::BaseBpm;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 2, 3, 0, 4, 5},
                "Base BPM sorts numerically by the first pattern, keeps ties stable and puts unavailable values last.");
        Require(state.focusedSongPosition_ == 3,
                "Changing sort preserves the focused song even when its visible position changes.");
        state.SelectVisibleSong(1);
        state.SelectPattern(1);
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 2, 3, 0, 4, 5} &&
                    state.focusedSongPosition_ == 1,
                "The selected difficulty and BPM range must not change the first-pattern sort key.");
        state.searchText_ = L"match";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 0},
                "Search filters compose with BPM sorting without restoring catalog order.");
        state.catalog_.songs.clear();
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_.empty(), "BPM sorting accepts an empty catalog.");
    }

    void TestSongSearchAndMetadata()
    {
        SongSelectionState state;
        SongCatalogEntry combined, angelOnly, splitFields, boundary, fallback;
        combined.music.names = {"Angel Dream", "天使の夢"};
        combined.music.artists = {"Primary artist", "Alternate creator", "아티스트"};
        combined.music.tags = {"Bright-sky", "音楽", "ÉTÉ"};
        combined.patterns.resize(2);
        combined.patterns[1].pattern.tags = {"practice", "fast"};
        angelOnly.music.names = {"Angel only"};
        splitFields.music.names = {"Dream only"};
        splitFields.music.artists = {"Angel painter"};
        boundary.music.names = {"an"};
        boundary.music.artists = {"gel"};
        fallback.metadataPath = "Fallback Track.ymm";
        state.catalog_.songs = {combined, angelOnly, splitFields, boundary, fallback};
        const auto search = [&](std::wstring query, const std::vector<std::size_t> &expected)
        {
            state.searchText_ = std::move(query);
            state.RebuildVisibleSongs();
            Require(state.visibleSongIndices_ == expected, "Search must AND every word across complete metadata.");
        };
        search(L"angel dream", {0, 2});
        search(L"dream ANGEL", {0, 2});
        search(L"  ANGEL\t dream\r\n", {0, 2});
        search(L"angel", {0, 1, 2}); // Separate fields must not create the word an+gel.
        search(L"dream", {0, 2});
        search(L"ang", {0, 1, 2});
        search(L"angel\u3000dream", {0, 2});
        search(L"angel absent", {});
        search(L"Alternate practice", {0});
        search(L"sky FAST", {0});
        search(L"天使 音楽", {0});
        search(L"아티스트 practice", {0});
        search(L"été", {0});
        Require(state.catalog_.songs.front().patterns.size() == 2,
                "A matching pattern tag filters songs without removing their other patterns.");
        search(L"Fallback track", {4});
        search(L" \t\r\n", {0, 1, 2, 3, 4});
        search(L"", {0, 1, 2, 3, 4});
        state.catalog_.songs.clear();
        search(L"angel dream", {});
    }

    void TestSceneStateBoundaries()
    {
        SongSelectionState state;
        state.RebuildVisibleSongs();
        state.MoveDifficultyFocus(1);
        state.MoveSongFocus(-1);
        Require(state.visibleSongIndices_.empty(), "Empty catalogs must remain navigable without a selection.");

        SongCatalogEntry first, second;
        first.music.names = {"Zulu"};
        first.music.artists = {"Artist A"};
        first.patterns.resize(2);
        second.music.names = {"Alpha"};
        second.music.artists = {"Artist B"};
        second.patterns.resize(1);
        state.catalog_.songs = {first, second};
        state.RebuildVisibleSongs();
        state.MoveDifficultyFocus(1);
        Require(state.focusedSongPosition_ == 0 && state.selectedPatternIndex_ == 1,
                "Down first moves inside the expanded song.");
        state.MoveDifficultyFocus(1);
        Require(state.focusedSongPosition_ == 1 && state.selectedPatternIndex_ == 0,
                "Down at the final difficulty selects the next song's first difficulty.");
        state.MoveDifficultyFocus(-1);
        Require(state.focusedSongPosition_ == 0 && state.selectedPatternIndex_ == 0,
                "Up at the first difficulty preserves the existing previous-song policy.");
        state.sortMode_ = SongSelectionState::SortMode::Title;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 0} && state.focusedSongPosition_ == 1,
                "Sorting changes display order but keeps focus on the same catalog entry.");
        state.searchText_ = L"artist b";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1} && state.selectedPatternIndex_ == 0,
                "Search is case-insensitive and resets difficulty safely.");
        state.MoveSongFocus(20);
        Require(state.focusedSongPosition_ == 0, "Song focus clamps to the filtered list.");

        GameplayLaunchStore store;
        auto prepared = std::make_unique<PlaySession>();
        auto *identity = prepared.get();
        store.SetValidated({"validated.ymp", {}, "music.mp3", "Taiko"}, std::move(prepared));
        auto transferred = store.TakeValidatedSession();
        Require(transferred.get() == identity && !store.TakeValidatedSession(),
                "A validated play session must transfer ownership once without reparsing.");
        GameplayLaunchRequest firstLaunch{"first.ymp", {}, "first.mp3", "Taiko", L"Played title", L"Played creator"};
        firstLaunch.songArtist = L"Played artist";
        store.Set(firstLaunch);
        auto playing = store.Snapshot();
        store.Set({"second.ymp", "second.yme", "second.mp3", "Taiko"});
        playing.musicPath = "local.mp3";
        Require(playing.patternPath == "first.ymp" && store.Snapshot().musicPath == "second.mp3",
                "An active session and a later selection must own independent launch values.");
        Require(playing.songTitle == L"Played title" && playing.patternCreator == L"Played creator" &&
                    playing.songArtist == L"Played artist" && store.Snapshot().songTitle.empty() &&
                    store.Snapshot().songArtist.empty(),
                "Result display metadata must belong to the played selection snapshot.");
        playing.Clear();
        Require(!playing.IsValid() && playing.songTitle.empty() && playing.patternCreator.empty() &&
                    playing.songArtist.empty(),
                "Clearing a launch must also clear result display metadata.");

        for (const auto &tool : Tools)
            Require(FindTaikoNote(static_cast<int>(tool.note)) != nullptr,
                    "Every editor note tool must refer to a known persisted note definition.");
        const auto *katBuzz = Find(18);
        Require(katBuzz && katBuzz->note == TaikoNoteType::Buzz &&
                    ExtraData(*katBuzz) == std::vector<std::string>{"Action=Kat", "TickDivision=16"},
                "Kat Buzz's editor ID must serialize as Buzz plus its action, not as a new YMP ID.");
        Require(TaikoVisual(TaikoNoteType::BigTickRoll) == NoteVisualKind::BigRoll &&
                    TaikoVisual(TaikoNoteType::Buzz, TaikoAction::Kat) ==
                        NoteVisualKind::KatBuzz,
                "Typed presentation must preserve the large roll and Kat Buzz appearances.");
    }
} // namespace tests
