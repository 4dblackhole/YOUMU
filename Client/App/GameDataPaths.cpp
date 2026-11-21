#include "App/GameDataPaths.h"
#include "Records/PlayRecordStore.h"
#include "YOUMUAssets.h"

std::filesystem::path GameDataDirectory()
{
    return AssetExecutableDirectory() / L"Data";
}

std::filesystem::path PlayRecordDatabasePath()
{
    // Resolved once per process so the copy runs before any record is read or written.
    static const std::filesystem::path path = PlayRecordStore::AdoptLegacyDatabase(
        GameDataDirectory() / L"Result" / L"plays.sqlite3", PlayRecordStore::LegacyPath());
    return path;
}
