#include "Records/RecordFilter.h"
#include "Records/PlayRecordStore.h"
#include "TestSupport.h"
#include <algorithm>
#include <limits>

namespace tests
{
    void TestRecordFilters()
    {
        Require(ParseRecordFilter({}).Query().empty(), "Unspecified record conditions impose no limits.");
        RecordFilterDraft draft;
        draft.choices[0] = 2;
        draft.choices[1] = 1;
        draft.choices[8] = 1; // Cleared.
        draft.choices[RecordFilterFlagCount] = 2;
        draft.values = {L"60", L"70", L"1.5", L"1.5", L"240", L"240", L"35", L"35", L"18446744073709551615"};
        auto filter = ParseRecordFilter(draft);
        Require(filter.flags[0] == false && filter.flags[1] == true && !filter.flags[2],
                "False, true and unlimited are distinct choices; BPM uses the stored unit.");
        Require(filter.scrollUnit == 1, "False, true and unlimited are distinct choices; BPM uses the stored unit.");
        const auto restored = FormatRecordFilter(filter);
        Require(restored.choices == draft.choices && restored.values == draft.values,
                "Reopening preserves numeric values and the full uint64 seed.");
        TemporaryDirectory directory("youmu-record-filter");
        const auto database = directory.Path() / "plays.sqlite3", pattern = directory.Path() / "fixture.ymp";
        std::ofstream(pattern) << "fixture";
        PlayRecord record;
        record.patternPath = pattern; record.mode = "Taiko";
        record.completed = record.cleared = true;
        record.options.random = true; record.options.accelerate = 1.5;
        record.options.scrollUnit = PlayOptions::ScrollUnit::Bpm; record.options.scrollSpeed = 240;
        record.options.randomAttemptProbability = .35; record.options.randomSeed = UINT64_MAX;
        record.totalNotes = record.processedNotes = 1;
        for (const double level : {59., 60., 70., 71.})
        { record.judgeLevel = level; PlayRecordStore(database).Save(record); }
        record.judgeLevel = 65;
        record.options.mirror = true; PlayRecordStore(database).Save(record);
        record.options.mirror = false; record.options.random = false; PlayRecordStore(database).Save(record);
        record.options.random = true; record.cleared = false; PlayRecordStore(database).Save(record);
        record.completed = false; record.options.accelerate = 2; PlayRecordStore(database).Save(record);
        const auto rows = PlayRecordStore(database).Query(pattern, "Taiko", filter.Query(), 0, 20);
        Require(rows.size() == 2 && rows[0].judgeLevel == JudgeLevel{70} && rows[1].judgeLevel == JudgeLevel{60},
                "Real history uses inclusive bounds and ANDs the structured fields.");
        // Play results are Any/On/Off rows too: Random on with Cleared off finds the two plays that were not cleared.
        auto results = draft;
        results.values = {};
        results.choices = {};
        results.choices[1] = 1;
        results.choices[8] = 2;
        Require(PlayRecordStore(database).Query(pattern, "Taiko", ParseRecordFilter(results).Query(), 0, 20).size() == 2,
                "Cleared and Completed choices filter real history.");
        results.choices[7] = 1;
        Require(PlayRecordStore(database).Query(pattern, "Taiko", ParseRecordFilter(results).Query(), 0, 20).size() == 1,
                "Completed narrows the not-cleared random plays to the finished one.");
        results.choices = {};
        results.choices[9] = 1;
        Require(PlayRecordStore(database).Query(pattern, "Taiko", ParseRecordFilter(results).Query(), 0, 20).empty(),
                "No stored play used the pause menu.");
        // A switched-off value row keeps its value but stops limiting the records.
        auto switched = draft;
        Require(switched.ToggleRow(0) && !switched.IsRowOn(0), "A row with values can be switched off.");
        switched.SetValue(1, L"71");
        Require(!switched.IsRowOn(0), "Editing a switched-off row keeps it off.");
        filter = ParseRecordFilter(switched);
        Require(PlayRecordStore(database).Query(pattern, "Taiko", filter.Query(), 0, 20).size() == 4 &&
                    !FormatRecordFilter(filter).IsRowOn(0) && FormatRecordFilter(filter).values[1] == L"71",
                "Switched-off JudgeLevel bounds no longer limit records and survive reopening.");
        auto fresh = RecordFilterDraft{};
        fresh.SetValue(8, L"7");
        Require(fresh.IsRowOn(4), "The seed row turns on with its first value.");
        for (const auto &[index, value] : std::array<std::pair<std::size_t, const wchar_t *>, 15>{{
            {0,L"71"}, {0,L"0"}, {1,L"101"}, {2,L"0.49"}, {3,L"2.01"}, {4,L"0"}, {5,L"-1"},
            {6,L"-1"}, {7,L"101"}, {0,L"nan"}, {2,L"inf"}, {8,L"18446744073709551616"},
            {8,L"1.5"}, {0,L"６０"}, {1,L"60x"}}})
        {
            auto invalid = draft; invalid.values[index] = value;
            bool rejected{};
            try { static_cast<void>(ParseRecordFilter(invalid)); }
            catch (const std::invalid_argument &) { rejected = true; }
            Require(rejected, "Invalid numeric drafts cannot become committed record conditions.");
        }
        Require(PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 20).size() == 8,
                "Filter queries preserve stored records.");
        // Max strokes: Any keeps every count; a count shows only the records played with it.
        record.options.maxStrokes = 3;
        PlayRecordStore(database).Save(record);
        RecordFilterDraft strokes;
        Require(PlayRecordStore(database).Query(pattern, "Taiko", ParseRecordFilter(strokes).Query(), 0, 20).size() == 9,
                "Any stroke count keeps every record.");
        strokes.maxStrokes = 3;
        const auto byStrokes = ParseRecordFilter(strokes);
        const auto three = PlayRecordStore(database).Query(pattern, "Taiko", byStrokes.Query(), 0, 20);
        Require(byStrokes.maxStrokes == 3 && three.size() == 1 && three[0].options.maxStrokes == 3,
                "A stroke count shows only the records played with it.");
        strokes.maxStrokes = 2;
        Require(PlayRecordStore(database).Query(pattern, "Taiko", ParseRecordFilter(strokes).Query(), 0, 20).size() == 8,
                "Two strokes finds the default-count records.");
        Require(FormatRecordFilter(byStrokes).maxStrokes == 3, "Reopening keeps the chosen stroke count.");
        strokes.maxStrokes = 5;
        bool rejected{};
        try { static_cast<void>(ParseRecordFilter(strokes)); }
        catch (const std::invalid_argument &) { rejected = true; }
        Require(rejected, "A stroke count outside 1-4 is rejected.");

        // Deleting one record removes that row only; a second delete finds nothing, and a missing database
        // is left missing.
        const auto before = PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 20);
        Require(before.size() == 9 && PlayRecordStore(database).Delete(before[3].id),
                "A stored record can be deleted.");
        const auto after = PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 20);
        const bool gone = std::ranges::none_of(after, [&](const PlayRecord &r) { return r.id == before[3].id; });
        Require(after.size() == 8 && gone && after[3].id == before[4].id, "Only the deleted record is gone.");
        Require(!PlayRecordStore(database).Delete(before[3].id), "Deleting a record that is gone finds nothing.");
        const auto missing = directory.Path() / "missing.sqlite3";
        Require(!PlayRecordStore(missing).Delete(1) && !std::filesystem::exists(missing),
                "Deleting from a missing database creates nothing.");
    }
}
