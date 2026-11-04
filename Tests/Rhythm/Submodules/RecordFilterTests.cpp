#include "Records/RecordFilter.h"
#include "Records/PlayRecordStore.h"
#include "TestSupport.h"
#include <limits>

namespace tests
{
    void TestRecordFilters()
    {
        Require(ParseRecordFilter({}).Query().empty(), "Unspecified record conditions impose no limits.");
        RecordFilterDraft draft;
        draft.choices[0] = 2;
        draft.choices[1] = 1;
        draft.choices[7] = 2;
        draft.values = {L"60", L"70", L"1.5", L"1.5", L"240", L"240", L"35", L"35", L"18446744073709551615"};
        draft.condition = L"Cleared = true OR NOT (Completed = true)";
        const auto filter = ParseRecordFilter(draft);
        Require(filter.flags[0] == false && filter.flags[1] == true && !filter.flags[2] && filter.scrollUnit == 1,
                "False, true and unlimited are distinct choices; BPM uses the stored unit.");
        const auto restored = FormatRecordFilter(filter);
        Require(restored.choices == draft.choices && restored.values == draft.values && restored.condition == draft.condition,
                "Reopening preserves numeric values, full uint64 seed and the direct expression.");
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
                "Real history uses inclusive bounds and ANDs structured fields with the complete OR/NOT expression.");
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
        RecordFilter manual;
        manual.condition = "NOT (Mirror = true) OR Random = true";
        Require(ParseRecordFilter(FormatRecordFilter(manual)).Query() == manual.condition,
                "A direct-only condition retains its existing syntax and parser limits.");
        manual.condition = " \t ";
        Require(manual.Query().empty(), "Whitespace-only direct conditions are unlimited.");
        draft.condition = L"Cleared=true; DELETE FROM Plays";
        bool rejected{};
        try { static_cast<void>(ParseRecordFilter(draft)); }
        catch (const std::invalid_argument &) { rejected = true; }
        Require(rejected && PlayRecordStore(database).Query(pattern, "Taiko", "", 0, 20).size() == 8,
                "Invalid direct conditions are rejected and filter queries preserve stored records.");
    }
}
