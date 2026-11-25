#include "App/GameDataPaths.h"
#include "YOUMUAssets.h"

std::filesystem::path GameDataDirectory()
{
    return AssetExecutableDirectory() / L"Data";
}

std::filesystem::path PlayRecordDatabasePath()
{
    return GameDataDirectory() / L"Result" / L"plays.sqlite3";
}

std::filesystem::path LocalOffsetFilePath()
{
    return GameDataDirectory() / L"LocalOffsets.txt";
}

std::filesystem::path GameLogDirectory()
{
    return GameDataDirectory() / L"Logs";
}
