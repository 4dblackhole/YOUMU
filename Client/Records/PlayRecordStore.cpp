#include "PlayRecordStore.h"
#include "RecordCondition.h"
#include "Mode/PlayGameModeFactory.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cwctype>
#include <format>
#include <limits>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <winsqlite/winsqlite3.h>
#pragma comment(lib, "winsqlite3.lib")

namespace
{
    using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>;
    using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
    void Check(sqlite3 *database, int result)
    {
        if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW)
            throw std::runtime_error(sqlite3_errmsg(database));
    }
    Statement Prepare(sqlite3 *db, const std::string &sql)
    {
        sqlite3_stmt *raw{};
        const int status = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);
        Statement statement(raw, sqlite3_finalize);
        Check(db, status);
        return statement;
    }
    void Execute(sqlite3 *db, const char *sql)
    {
        Check(db, sqlite3_exec(db, sql, nullptr, nullptr, nullptr));
    }
    Database Connect(const std::filesystem::path &path)
    {
        std::filesystem::create_directories(path.parent_path());
        sqlite3 *raw{};
        const int status = sqlite3_open16(path.c_str(), &raw);
        Database db(raw, sqlite3_close_v2);
        if (!raw)
            throw std::runtime_error("Could not allocate the record database.");
        Check(raw, status);
        Check(raw, sqlite3_busy_timeout(raw, 2000));
        return db;
    }
    int SchemaOf(sqlite3 *db)
    {
        auto version = Prepare(db, "PRAGMA user_version");
        Check(db, sqlite3_step(version.get()));
        return sqlite3_column_int(version.get(), 0);
    }
    // A database file and the SQLite files that can sit beside it.
    constexpr std::array<std::wstring_view, 4> DatabaseFileSuffixes{L"", L"-journal", L"-wal", L"-shm"};
    bool TakePrefix(std::wstring_view &text, std::wstring_view prefix)
    {
        if (!text.starts_with(prefix))
            return false;
        text.remove_prefix(prefix.size());
        return true;
    }
    bool TakeDigits(std::wstring_view &text)
    {
        const auto digits = std::ranges::find_if_not(text, [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; });
        const auto count = static_cast<std::size_t>(digits - text.begin());
        text.remove_prefix(count);
        return count != 0;
    }
    // The copy number ` (<digits>)` is optional.
    bool TakeCopyNumber(std::wstring_view &text)
    {
        if (!TakePrefix(text, L" ("))
            return true;
        return TakeDigits(text) && TakePrefix(text, L")");
    }
    // Earlier builds set old databases aside as `<stem>.v<digits>[ (<digits>)]<extension>` with their
    // `-journal`/`-wal`/`-shm` files. Only names of exactly this form match.
    [[nodiscard]] bool IsSetAsideName(std::wstring_view name, std::wstring_view stem, std::wstring_view extension)
    {
        const bool numbered = TakePrefix(name, stem) && TakePrefix(name, L".v") && TakeDigits(name);
        if (!numbered || !TakeCopyNumber(name) || !TakePrefix(name, extension))
            return false;
        return std::ranges::find(DatabaseFileSuffixes, name) != DatabaseFileSuffixes.end();
    }
    void RemoveFile(const std::filesystem::path &file)
    {
        std::error_code error;
        std::filesystem::remove(file, error);
        if (error)
            throw std::runtime_error("Could not delete an old record database. Existing records were preserved.");
    }
    // Deletes an older-schema database. The files beside it go first, so a failure on the database itself
    // leaves it in place and no journal stays behind for the new database.
    void DeleteOldDatabase(const std::filesystem::path &path)
    {
        for (const auto suffix : DatabaseFileSuffixes | std::views::drop(1))
        {
            auto beside = path;
            beside += suffix;
            RemoveFile(beside);
        }
        RemoveFile(path);
    }
    // Deletes the databases earlier builds set aside in the database's folder, and nothing else.
    void DeleteSetAsideDatabases(const std::filesystem::path &path)
    {
        const auto stem = path.stem().wstring(), extension = path.extension().wstring();
        std::vector<std::filesystem::path> found;
        for (const auto &entry : std::filesystem::directory_iterator(path.parent_path()))
        {
            if (entry.is_regular_file() && IsSetAsideName(entry.path().filename().wstring(), stem, extension))
                found.push_back(entry.path());
        }
        for (const auto &file : found)
            RemoveFile(file);
    }
    // Schemas 1..3 hold records judged by earlier rules; the user dropped them all (schema 1: no score,
    // schema 2/3: before BAD recovery). The file is deleted and a fresh one starts.
    [[nodiscard]] bool IsOldSchema(int schema) noexcept
    {
        return schema > 0 && schema < PlayRecordStore::SchemaVersion;
    }
    Database Open(const std::filesystem::path &path)
    {
        auto db = Connect(path);
        int schema = SchemaOf(db.get());
        if (IsOldSchema(schema))
        {
            db.reset();
            DeleteOldDatabase(path);
            db = Connect(path);
            schema = SchemaOf(db.get());
        }
        if (schema != 0 && schema != PlayRecordStore::SchemaVersion)
            throw std::runtime_error("Unsupported local record database version. Existing records were preserved.");
        DeleteSetAsideDatabases(path);
        if (schema == 0)
        {
            Execute(db.get(), "BEGIN IMMEDIATE");
            Execute(db.get(),
                    "CREATE TABLE IF NOT EXISTS Plays (Id INTEGER PRIMARY KEY, PlayedAt INTEGER NOT NULL, Pattern TEXT "
                    "NOT NULL, Mode TEXT NOT NULL, Accuracy INTEGER NOT NULL, TotalNotes INTEGER NOT NULL, "
                    "ProcessedNotes INTEGER NOT NULL, Completed INTEGER NOT NULL, Cleared INTEGER NOT NULL, "
                    "PauseMenuUsed INTEGER NOT NULL, JudgeLevel REAL NOT NULL, ScrollSpeed REAL NOT NULL, ScrollUnit "
                    "INTEGER NOT NULL, IgnoreScrollEffects INTEGER NOT NULL, Metronome INTEGER NOT NULL, Mirror "
                    "INTEGER NOT NULL, Random INTEGER NOT NULL, RandomSeed TEXT NOT NULL, RandomProbability REAL NOT "
                    "NULL, Accelerate REAL NOT NULL, PreserveMusicPitch INTEGER NOT NULL, PreserveHitSoundPitch "
                    "INTEGER NOT NULL, JudgeOverride INTEGER NOT NULL, GlobalOffset INTEGER NOT NULL, Score INTEGER "
                    "NOT NULL, MaximumCombo INTEGER NOT NULL, HpGauge INTEGER NOT NULL, Detail TEXT NOT NULL, "
                    "MaxStrokes INTEGER NOT NULL DEFAULT 2)");
            Execute(db.get(), "CREATE INDEX IF NOT EXISTS PatternHistory ON Plays(Pattern, Mode, Id DESC)");
            Execute(db.get(), std::format("PRAGMA user_version={}", PlayRecordStore::SchemaVersion).c_str());
            Execute(db.get(), "COMMIT");
        }
        return db;
    }
    std::string PathKey(const std::filesystem::path &path)
    {
        auto normalized = std::filesystem::weakly_canonical(path).lexically_normal().generic_wstring();
        // Windows chart identity is case-insensitive; Unicode path is stored as UTF-8.
        for (auto &ch : normalized)
            ch = static_cast<wchar_t>(std::towlower(ch));
        const auto utf8 = std::filesystem::path(normalized).generic_u8string();
        return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
    }
    void Text(sqlite3 *db, sqlite3_stmt *statement, int column, std::string_view value)
    {
        Check(db, sqlite3_bind_text(statement, column, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT));
    }
    std::string ReadText(sqlite3_stmt *s, int column)
    {
        const auto *text = sqlite3_column_text(s, column);
        return text ? std::string(reinterpret_cast<const char *>(text),
                                  static_cast<std::size_t>(sqlite3_column_bytes(s, column)))
                    : std::string{};
    }
} // namespace
PlayRecord PlayRecord::From(const IPlayResult &result)
{
    const auto &s = result.Summary();
    PlayRecord r;
    r.playedAtMilliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    r.patternPath = s.patternPath;
    r.mode = result.ModeId();
    r.accuracy = s.accuracy;
    r.score = s.score;
    r.totalNotes = s.totalNotes;
    r.processedNotes = s.processedNotes;
    r.maximumCombo = s.maximumCombo;
    r.completed = s.endReason == PlayEndReason::Completed;
    r.cleared = s.IsCleared();
    r.pauseMenuUsed = s.pauseMenuUsed;
    r.judgeLevel = s.judgementLevel;
    r.options = s.playOptions;
    r.globalOffset = s.globalOffset;
    if (IsSupportedPlayGameMode(r.mode))
        r.detail = CreatePlayGameMode(r.mode)->SaveResultDetail(result);
    return r;
}
std::unique_ptr<IPlayResult> PlayRecord::LoadResult(std::string patternName) const
{
    if (detail.empty() || !IsSupportedPlayGameMode(mode))
        throw std::runtime_error("This record keeps no result detail.");
    PlayResultSummary s;
    s.patternPath = patternPath;
    s.patternName = std::move(patternName);
    s.accuracy = accuracy;
    s.score = score;
    s.totalNotes = totalNotes;
    s.processedNotes = processedNotes;
    s.maximumCombo = maximumCombo;
    s.judgementLevel = judgeLevel;
    s.playOptions = options;
    s.globalOffset = globalOffset;
    s.pauseMenuUsed = pauseMenuUsed;
    return CreatePlayGameMode(mode)->LoadResult(std::move(s), detail);
}
void PlayRecordStore::Save(const PlayRecord &r) const
{
    r.options.Validate();
    if (r.patternPath.empty() || r.mode.empty())
        throw std::invalid_argument("A record requires a real pattern and mode.");
    const bool judgeOverrideMismatch = r.options.judgeLevel && *r.options.judgeLevel != r.judgeLevel;
    constexpr auto largest = static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
    const bool countsFit = r.totalNotes <= largest && r.maximumCombo <= largest;
    if (r.processedNotes > r.totalNotes || !countsFit || judgeOverrideMismatch)
        throw std::invalid_argument("Inconsistent play record summary.");
    auto db = Open(path_);
    auto s = Prepare(db.get(), "INSERT INTO Plays VALUES(NULL,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    const auto integer = [&](int index, std::int64_t value)
    { Check(db.get(), sqlite3_bind_int64(s.get(), index, value)); };
    const auto number = [&](int index, double value) { Check(db.get(), sqlite3_bind_double(s.get(), index, value)); };
    integer(1, r.playedAtMilliseconds);
    Text(db.get(), s.get(), 2, PathKey(r.patternPath));
    Text(db.get(), s.get(), 3, r.mode);
    integer(4, r.accuracy.Units());
    integer(5, static_cast<std::int64_t>(r.totalNotes));
    integer(6, static_cast<std::int64_t>(r.processedNotes));
    integer(7, r.completed);
    integer(8, r.cleared);
    integer(9, r.pauseMenuUsed);
    number(10, r.judgeLevel.Value());
    const auto &o = r.options;
    number(11, o.scrollSpeed);
    integer(12, o.scrollUnit == PlayOptions::ScrollUnit::Bpm);
    integer(13, o.ignoreScrollEffects);
    integer(14, o.metronome);
    integer(15, o.mirror);
    integer(16, o.random);
    Text(db.get(), s.get(), 17, std::to_string(o.randomSeed));
    number(18, o.randomAttemptProbability * 100);
    number(19, o.accelerate);
    integer(20, o.preserveMusicPitch);
    integer(21, o.preserveHitSoundPitch);
    integer(22, o.judgeLevel.has_value());
    integer(23, r.globalOffset.count());
    integer(24, r.score);
    integer(25, static_cast<std::int64_t>(r.maximumCombo));
    integer(26, static_cast<std::int64_t>(o.hpGauge));
    Text(db.get(), s.get(), 27, r.detail);
    integer(28, o.maxStrokes);
    Check(db.get(), sqlite3_step(s.get()));
}
void PlayRecordStore::ValidateQuery(std::string_view condition)
{
    static_cast<void>(RecordCondition::Parse(condition));
}
std::vector<PlayRecord> PlayRecordStore::Query(const std::filesystem::path &pattern, std::string_view mode,
                                               std::string_view condition, std::size_t offset, std::size_t limit) const
{
    if (!limit || limit > 100 || offset > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Invalid record page.");
    const auto expression = RecordCondition::Parse(condition);
    if (!std::filesystem::exists(path_))
        return {};
    auto db = Open(path_);
    // Every column but the detail, which LoadDetail reads for one record.
    auto s = Prepare(db.get(),
                     "SELECT Id, PlayedAt, Pattern, Mode, Accuracy, TotalNotes, ProcessedNotes, Completed, Cleared, "
                     "PauseMenuUsed, JudgeLevel, ScrollSpeed, ScrollUnit, IgnoreScrollEffects, Metronome, Mirror, Random, "
                     "RandomSeed, RandomProbability, Accelerate, PreserveMusicPitch, PreserveHitSoundPitch, JudgeOverride, "
                     "GlobalOffset, Score, MaximumCombo, HpGauge, MaxStrokes FROM Plays WHERE Pattern=? AND Mode=? AND (" +
                         expression.sql + ") ORDER BY Id DESC LIMIT ? OFFSET ?");
    Text(db.get(), s.get(), 1, PathKey(pattern));
    Text(db.get(), s.get(), 2, mode);
    int index = 3;
    for (const auto &v : expression.values)
        Text(db.get(), s.get(), index++, v);
    Check(db.get(), sqlite3_bind_int64(s.get(), index++, static_cast<sqlite3_int64>(limit)));
    Check(db.get(), sqlite3_bind_int64(s.get(), index, static_cast<sqlite3_int64>(offset)));
    std::vector<PlayRecord> records;
    int status{};
    while ((status = sqlite3_step(s.get())) == SQLITE_ROW)
    {
        PlayRecord r;
        const auto integer = [&](int c) { return sqlite3_column_int64(s.get(), c); };
        const auto number = [&](int c) { return sqlite3_column_double(s.get(), c); };
        r.id = integer(0);
        r.playedAtMilliseconds = integer(1);
        r.patternPath = pattern;
        r.mode = mode;
        const bool accuracyInRange = integer(4) >= 0 && integer(4) <= AccuracyScore::MaximumUnits;
        const bool noteCountsValid = integer(5) >= 0 && integer(6) >= 0 && integer(6) <= integer(5);
        const bool scoreValid = integer(24) >= 0 && integer(24) <= std::numeric_limits<std::uint32_t>::max();
        const bool comboValid = integer(25) >= 0 && integer(26) >= 0;
        if (!accuracyInRange || !noteCountsValid)
            throw std::runtime_error("Invalid stored accuracy or note count. Records were preserved.");
        if (!scoreValid || !comboValid)
            throw std::runtime_error("Invalid stored score, combo or HP gauge. Records were preserved.");
        for (const int column : {7, 8, 9, 12, 13, 14, 15, 16, 20, 21, 22})
            if (integer(column) != 0 && integer(column) != 1)
                throw std::runtime_error("Invalid stored option flag. Records were preserved.");
        r.accuracy = AccuracyScore::FromUnits(static_cast<std::uint32_t>(integer(4)));
        r.totalNotes = static_cast<std::size_t>(integer(5));
        r.processedNotes = static_cast<std::size_t>(integer(6));
        r.completed = integer(7) != 0;
        r.cleared = integer(8) != 0;
        r.pauseMenuUsed = integer(9) != 0;
        r.judgeLevel = number(10);
        auto &o = r.options;
        o.scrollSpeed = number(11);
        o.scrollUnit = integer(12) ? PlayOptions::ScrollUnit::Bpm : PlayOptions::ScrollUnit::Multiplier;
        o.ignoreScrollEffects = integer(13) != 0;
        o.metronome = integer(14) != 0;
        o.mirror = integer(15) != 0;
        o.random = integer(16) != 0;
        const auto seed = ReadText(s.get(), 17);
        const auto parsed = std::from_chars(seed.data(), seed.data() + seed.size(), o.randomSeed);
        if (parsed.ec != std::errc{} || parsed.ptr != seed.data() + seed.size())
            throw std::runtime_error("Invalid record seed.");
        o.randomAttemptProbability = number(18) / 100;
        o.accelerate = number(19);
        o.preserveMusicPitch = integer(20) != 0;
        o.preserveHitSoundPitch = integer(21) != 0;
        if (integer(22))
            o.judgeLevel = r.judgeLevel;
        r.globalOffset = RhythmDuration{integer(23)};
        r.score = static_cast<std::uint32_t>(integer(24));
        r.maximumCombo = static_cast<std::size_t>(integer(25));
        o.hpGauge = static_cast<HpGaugeKind>(std::min<std::int64_t>(integer(26), 255));
        if (integer(27) < PlayOptions::MinimumStrokes || integer(27) > PlayOptions::MaximumStrokes)
            throw std::runtime_error("Invalid stored max strokes. Records were preserved.");
        o.maxStrokes = static_cast<int>(integer(27));
        o.Validate();
        records.push_back(std::move(r));
    }
    Check(db.get(), status);
    return records;
}
bool PlayRecordStore::Delete(const std::int64_t id) const
{
    if (!std::filesystem::exists(path_))
        return false;
    auto db = Open(path_);
    auto s = Prepare(db.get(), "DELETE FROM Plays WHERE Id=?");
    Check(db.get(), sqlite3_bind_int64(s.get(), 1, id));
    Check(db.get(), sqlite3_step(s.get()));
    return sqlite3_changes(db.get()) > 0;
}

std::string PlayRecordStore::LoadDetail(std::int64_t id) const
{
    if (!std::filesystem::exists(path_))
        return {};
    auto db = Open(path_);
    auto s = Prepare(db.get(), "SELECT Detail FROM Plays WHERE Id=?");
    Check(db.get(), sqlite3_bind_int64(s.get(), 1, id));
    const int status = sqlite3_step(s.get());
    Check(db.get(), status);
    return status == SQLITE_ROW ? ReadText(s.get(), 0) : std::string{};
}
