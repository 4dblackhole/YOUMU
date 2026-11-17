#include "EditorScene/Submodules/Modes/Taiko/TaikoEditorTool.h"
#include "GameFlow/GameplayLaunchStore.h"
#include "GameScene/MusicSelectScene/Submodules/MusicPlayerState.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionState.h"
#include "GameScene/MusicSelectScene/Submodules/SongSelectionText.h"
#include "TestSupport.h"
#include <limits>

namespace tests
{
    // HP, chart length and tag rows, and the row switches (UI-02 song filters).
    void TestSongFilterRows()
    {
        SongCatalogEntry song;
        song.music.tags = {"Cloud"};
        song.patterns.resize(1);
        auto &entry = song.patterns.front();
        entry.pattern.hpLevel = HpLevel{12.5};
        entry.pattern.tags = {"\xE6\x98\x9F"}; // A non-Latin pattern tag (U+661F).
        entry.chartLengthSeconds = 95.0;
        SongFilter filter;
        filter.minimumHp = 12.5;
        filter.maximumLength = 95.0;
        filter.tag = L"cloud";
        Require(filter.IsActive() && filter.Matches(song, entry),
                "HP and length bounds are inclusive, and a tag matches the song's tags ignoring case.");
        filter.tag = L"\u661F";
        Require(filter.Matches(song, entry), "A tag also matches the pattern's own tags.");
        filter.tag = L"clou";
        Require(!filter.Matches(song, entry), "A tag must equal one tag; part of a tag is not enough.");
        filter.switchedOff[static_cast<std::size_t>(SongFilterRow::Tag)] = true;
        Require(filter.Matches(song, entry), "A switched-off row keeps its value and does not filter.");
        filter.maximumLength = 94.0;
        Require(!filter.Matches(song, entry), "Other rows keep filtering.");
        entry.chartLengthSeconds.reset();
        filter.maximumLength = 1000.0;
        Require(!filter.Matches(song, entry), "A chart without a known length is excluded by a length bound.");

        SongFilterDraft draft;
        constexpr auto notes = SongFilterRow::Notes;
        draft.SetValue(0, L"1000");
        Require(draft.IsRowOn(notes), "A first value turns the row's switch on by itself.");
        Require(draft.ToggleRow(notes) && !draft.IsRowOn(notes), "The switch turns the row off.");
        draft.SetValue(1, L"1500");
        draft.SetValue(0, L"1100");
        Require(!draft.IsRowOn(notes), "Editing values of a switched-off row leaves it off.");
        draft.SetValue(0, L" ");
        draft.SetValue(1, L"");
        draft.SetValue(0, L"900");
        Require(draft.IsRowOn(notes), "A row emptied and given a new first value turns on again.");
        Require(!draft.ToggleRow(SongFilterRow::Hp), "An empty row has no switch to turn.");
        draft.SetTag(L"Cloud");
        draft.ToggleRow(SongFilterRow::Tag);
        SongFilter parsed;
        Require(ParseSongFilter(draft, parsed) == SongFilterError::None && parsed.minimumNotes == 900 &&
                    parsed.tag == L"Cloud" && parsed.switchedOff[static_cast<std::size_t>(SongFilterRow::Tag)] &&
                    !parsed.IsRowActive(SongFilterRow::Tag) && parsed.IsActive(),
                "Applying keeps switched-off values for reopening while only switched-on rows filter.");
        const auto reopened = FormatSongFilter(parsed);
        Require(reopened.tag == L"Cloud" && !reopened.IsRowOn(SongFilterRow::Tag) && reopened.IsRowOn(notes),
                "Reopening restores values and switch states.");
    }

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
        auto unknown = song.patterns.front();
        unknown.noteCount.reset();
        SongFilter countFilter;
        countFilter.minimumNotes = 0;
        Require(!countFilter.Matches(song, unknown) && SongFilter{}.Matches(song, unknown),
            "Unknown logical counts are excluded only when a note-count bound is active.");
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
        draft.values = {L" 0 ", L"100", L"120.25", L"150", L"1", L"100", L"", L"", L"", L"", L"", L""};
        Require(ParseSongFilter(draft, parsed) == SongFilterError::None &&
                    parsed.minimumNotes == 0 && parsed.minimumBpm == 120.25,
                "Integer count and fractional range bounds accept whitespace and inclusive endpoints.");
        Require(FormatSongFilter(parsed).values == std::array<std::wstring, 12>{L"0", L"100", L"120.25", L"150", L"1", L"100", L"", L"", L"", L"", L"", L""},
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
        draft.values[8] = L"0.99";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::Hp, "HP bounds use the 1-100 HP range.");
        draft = {};
        draft.values[10] = L"-1";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::Length, "Length is a nonnegative number of seconds.");
        draft = {};
        draft.values[11] = L"60";
        draft.values[10] = L"61";
        Require(ParseSongFilter(draft, parsed) == SongFilterError::ReversedRange, "Every range row rejects reversed bounds.");
        TestSongFilterRows();
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

    void TestMusicPlayerTransport()
    {
        MusicPlayerState player;
        Require(player.Transport() == MusicTransport::Stopped && player.PositionMicroseconds() == 0 &&
                    !player.DurationMicroseconds(),
                "A player without a song is stopped at the start with no length.");
        player.BeginTrack();
        player.Advance(1.5);
        Require(player.IsPlaying() && player.PositionMicroseconds() == 1'500'000,
                "A new song plays from its start and counts elapsed playback time.");
        player.SetDuration(104'000'000);
        player.Pause();
        player.Advance(10.0);
        Require(player.Transport() == MusicTransport::Paused && player.PositionMicroseconds() == 1'500'000,
                "Pause keeps the position and stops counting.");
        player.TogglePlay();
        Require(player.IsPlaying(), "Play/pause resumes a paused song.");
        player.TogglePlay();
        Require(player.Transport() == MusicTransport::Paused, "Play/pause pauses a playing song.");
        player.Stop();
        Require(player.Transport() == MusicTransport::Stopped && player.PositionMicroseconds() == 0 &&
                    player.DurationMicroseconds() == 104'000'000,
                "Stop rewinds to the start, unlike pause, and keeps the known length.");
        player.Seek(40'000'000);
        Require(player.Transport() == MusicTransport::Paused && player.PositionMicroseconds() == 40'000'000,
                "Seeking a stopped song leaves it paused at the new position.");
        player.Seek(500'000'000);
        Require(player.PositionMicroseconds() < 104'000'000 && player.PositionMicroseconds() >= 103'000'000,
                "A seek past the end lands just before it, where the audio can still start.");
        player.Play();
        player.Advance(60.0);
        Require(player.PositionMicroseconds() == 104'000'000, "Elapsed time never runs past the length.");

        player.BeginTrack();
        Require(player.IsPlaying() && player.PositionMicroseconds() == 0 && !player.DurationMicroseconds(),
                "Another song starts playing from its start with an unknown length.");
        player.Advance(3.0);
        Require(!player.PreviousRestartsTrack(), "Within three seconds the previous key goes to the previous song.");
        player.Advance(0.001);
        Require(player.PreviousRestartsTrack(), "After three seconds the previous key restarts the current song.");
        player.Advance(-1.0);
        Require(player.PositionMicroseconds() == 3'001'000, "A negative frame time never rewinds the song.");
    }

    void TestMusicPlaylistSteps()
    {
        Require(!StepPlaylist(0, 0, 1) && !StepPlaylist(0, 0, -1), "An empty playlist has no next or previous song.");
        Require(StepPlaylist(3, 0, 1) == 1U && StepPlaylist(3, 2, 1) == 0U, "Next wraps from the last song to the first.");
        Require(StepPlaylist(3, 0, -1) == 2U && StepPlaylist(3, 1, -1) == 0U, "Previous wraps from the first song to the last.");
        Require(StepPlaylist(1, 0, 1) == 0U && StepPlaylist(1, 0, -1) == 0U, "A one-song playlist steps onto itself.");
        Require(StepPlaylist(3, 9, 1) == 0U, "A stale position steps from the last song.");
        Require(StepPlaylist(4, 1, -9) == 0U, "Steps larger than the playlist wrap around.");
        Require(FormatPlayerTime(0) == L"0:00" && FormatPlayerTime(104'999'999) == L"1:44" &&
                    FormatPlayerTime(3'600'000'000) == L"60:00",
                "Player times show whole minutes and two-digit seconds.");

        // The title player and song select follow the focused song; a rescan of the same folder
        // must keep it focused so its audio path, and therefore the sound, stays the same.
        SongSelectionState state;
        SongCatalogEntry first, second, third;
        first.metadataPath = "a/song.ymm";
        first.audioPath = "a/song.ogg";
        second.metadataPath = "b/song.ymm";
        second.audioPath = "b/song.ogg";
        third.metadataPath = "c/song.ymm";
        third.audioPath = "c/song.ogg";
        for (auto *song : {&first, &second, &third})
            song->patterns.resize(1);
        state.catalog_.songs = {first, second};
        state.RebuildVisibleSongs();
        state.SelectVisibleSong(1);
        std::vector<SongCatalogEntry> rescanned{third, second};
        state.MergeCatalogProgress(rescanned);
        const auto focusedAudio = [&state] {
            return state.catalog_.songs[state.visibleSongIndices_[state.focusedSongPosition_]].audioPath;
        };
        Require(focusedAudio() == "b/song.ogg", "Rescanned entries keep the focused song while they arrive.");
        SongCatalogLoadResult finished;
        finished.songs = {third, second, first};
        state.FinishCatalog(std::move(finished));
        Require(focusedAudio() == "b/song.ogg" && state.visibleSongIndices_.size() == 3,
                "A finished rescan in another order keeps the focused song.");
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
        Require(state.focusedSongPosition_ == 0 && state.selectedPatternIndex_ == 1,
                "Up at the first difficulty selects the previous song's last difficulty.");
        state.MoveDifficultyFocus(-1);
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
    // The shown notation decides the name and artist order; search covers every notation.
    void TestSongSelectionNotation()
    {
        using namespace song_select;
        SongCatalogEntry kana, latin;
        kana.metadataPath = "kana/Music/kana.ymm";
        kana.music.names = {"\xE3\x82\xAA\xE3\x83\xA2\xE3\x82\xA4"}; // Sorts after every Latin title.
        kana.music.romanizedName = "Aaa";
        kana.music.artists = {"\xE6\x9F\x8A"};
        kana.music.romanizedArtist = "Zed";
        kana.patterns.resize(1);
        kana.patterns[0].pattern.name = "\xE3\x82\xBD";
        kana.patterns[0].pattern.romanizedName = "Soflan";
        kana.patterns[0].pattern.makers = {"\xE9\xAD\x82", "Guest"};
        latin.metadataPath = "latin/Music/latin.ymm";
        latin.music.names = {"Middle"};
        latin.music.artists = {"Artist"};
        latin.patterns.resize(1);
        latin.patterns[0].pattern.name = "Oni";
        latin.patterns[0].pattern.makers = {"Maker"};

        using enum TextNotation;
        Require(SongTitle(kana, First) == L"\u30AA\u30E2\u30A4" && SongTitle(kana, Romanized) == L"Aaa" &&
                    SongArtist(kana, Romanized) == L"Zed" && SongTitle(latin, Romanized) == L"Middle",
                "Song texts follow the chosen notation; Roman text needs no romanized line.");
        Require(PatternName(kana.patterns[0], Romanized) == L"Soflan" && PatternName(kana.patterns[0], First) == L"\u30BD" &&
                    MakerName(latin.patterns[0], Romanized) == L"Maker",
                "Chart names follow the same rule.");
        // Makers are different people, so another maker never stands in for the first one's romanization.
        Require(MakerName(kana.patterns[0], Romanized) == L"\u9B42" && MakerName(kana.patterns[0], First) == L"\u9B42",
                "Without a romanized line the representative maker is shown as written.");
        kana.patterns[0].pattern.romanizedMaker = "Tamashii";
        Require(MakerName(kana.patterns[0], Romanized) == L"Tamashii" && MakerName(kana.patterns[0], First) == L"\u9B42",
                "The makers' romanized line replaces the representative maker.");
        SongCatalogEntry unnamed;
        unnamed.metadataPath = "x/Music/file name.ymm";
        Require(SongTitle(unnamed, Romanized) == L"file name" && SongArtist(unnamed, Romanized) == L"\u2014" &&
                    ArtistName(unnamed, First).empty(),
                "A song without names still shows its file name and a dash for the artist.");
        Require(AllNames(kana.music.names, kana.music.romanizedName) == L"Aaa \u00B7 \u30AA\u30E2\u30A4" &&
                    NameCount(kana.music.names, kana.music.romanizedName) == 2 &&
                    NameCount(latin.music.names, latin.music.romanizedName) == 1,
                "Tooltips list the romanized line before the notations.");

        SongSelectionState state;
        state.catalog_.songs = {latin, kana};
        state.sortMode_ = SongSelectionState::SortMode::Title;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0, 1}, "First notation: the kana title sorts last.");
        state.notation_ = Romanized;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1, 0}, "Romanized: the same song sorts by its romanized title.");
        state.sortMode_ = SongSelectionState::SortMode::Artist;
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{0, 1}, "Artist order follows the romanized artist.");
        state.notation_ = First;
        state.searchText_ = L"aaa zed";
        state.RebuildVisibleSongs();
        Require(state.visibleSongIndices_ == std::vector<std::size_t>{1},
                "Romanized lines are searched even while the first notation is shown.");
    }
} // namespace tests
