#include "EditorPatternCreation.h"
#include "Catalog/SongLayout.h"
#include "Editing/ChartEditor.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <algorithm>
#include <limits>
#include <system_error>

namespace fs = std::filesystem;

namespace
{
    bool Within(const fs::path &path, const fs::path &root)
    {
        const auto relative = path.lexically_relative(root);
        return !relative.empty() && !relative.has_root_path() &&
               std::ranges::none_of(relative, [](const auto &part) { return part == L".."; });
    }
    bool IsFilenameComponent(std::string_view name)
    {
        if (name.empty() || name.find_first_of("<>:\"/\\|?*\r\n\t") != name.npos)
            return false;
        if (name.front() == ' ' || name.back() == ' ' || name.back() == '.')
            return false;
        return std::ranges::none_of(name, [](unsigned char c) { return c < 32; });
    }
    fs::path DifficultyName(std::string_view name)
    {
        if (!IsFilenameComponent(name))
            throw Error(Failure::InvalidName, "Difficulty name must be one valid filename component.");
        const auto path = fs::path(std::u8string(reinterpret_cast<const char8_t *>(name.data()), name.size()));
        const bool dotPath = path == L"." || path == L"..";
        if (path.has_root_path() || path != path.filename() || dotPath)
            throw Error(Failure::InvalidName, "Difficulty name must not be a path.");
        return path;
    }
    // Chart/<first maker - difficulty name>/ of the song folder that holds the YMM.
    fs::path ChartSetFolder(const fs::path &music, const PatternDocument &metadata)
    {
        const auto song = SongLayout::SongFolderOfMusic(music);
        if (song.empty())
            throw Error(Failure::InvalidMetadata, "The selected song's YMM is not in a song folder's Music folder.");
        const std::string_view maker = metadata.makers.empty() ? std::string_view{} : metadata.makers.front();
        return song / SongLayout::ChartFolder / SongLayout::FolderName(maker, metadata.name);
    }
    void WriteNewFile(const fs::path &path, const std::string &bytes)
    {
        if (bytes.size() > (std::numeric_limits<DWORD>::max)())
            throw std::length_error("The new chart exceeds the file writer's size range.");
        const HANDLE file =
            CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
        DWORD count{};
        const bool writeSucceeded =
            WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr);
        const bool written = writeSucceeded && count == bytes.size();
        const DWORD error = writeSucceeded ? ERROR_WRITE_FAULT : GetLastError();
        const bool flushed = written && FlushFileBuffers(file);
        const DWORD flushError = flushed ? ERROR_SUCCESS : GetLastError();
        CloseHandle(file);
        if (!written || !flushed)
        {
            std::error_code ignored;
            fs::remove(path, ignored); // Only the file created by this call.
            throw std::system_error(static_cast<int>(written ? flushError : error), std::system_category());
        }
    }
} // namespace
SongCatalogPattern Create(const fs::path &songsRoot, const SongCatalogEntry &song,
                                 const PatternDocument &metadata)
{
    const auto root = fs::absolute(songsRoot).lexically_normal();
    const auto music = fs::absolute(song.metadataPath).lexically_normal();
    const auto name = DifficultyName(metadata.name);
    if (!Within(music, root) || !ChartParser{}.ParseMusicFile(music).Succeeded())
        throw Error(Failure::InvalidMetadata, "The selected song's YMM is unavailable.");
    const auto folder = ChartSetFolder(music, metadata);
    const auto filename = music.stem().wstring() + L" [" + name.wstring() + L"]";
    PatternDocument pattern;
    pattern.sourcePath = folder / (filename + L".ymp");
    pattern.effectFile = filename + L".yme";
    pattern.name = metadata.name;
    pattern.makers = metadata.makers;
    pattern.tags = metadata.tags;
    pattern.mode = "Taiko";
    pattern.baseBpm = metadata.baseBpm;
    pattern.patternOffsetMilliseconds = metadata.patternOffsetMilliseconds;
    pattern.judgementLevel = metadata.judgementLevel;
    pattern.hpLevel = metadata.hpLevel;
    pattern.backgroundFile = metadata.backgroundFile;
    pattern.backgroundOffsetX = metadata.backgroundOffsetX;
    pattern.backgroundOffsetY = metadata.backgroundOffsetY;
    EffectDocument effects;
    effects.sourcePath = folder / pattern.effectFile;
    const auto patternText = ChartEditor::WritePattern(pattern);
    const auto effectText = ChartEditor::WriteEffects(effects);
    const auto parsed = ChartParser{}.ParsePattern(patternText, pattern.sourcePath);
    if (!parsed.Succeeded() || ChartEditor::WritePattern(parsed.document) != patternText)
        throw Error(Failure::InvalidMetadata, "Metadata must round-trip through the YMP format.");
    // Validate the same timing/automation invariants used when opening the editor.
    static_cast<void>(ChartEditor(pattern, effects));
    SongCatalogPattern result{pattern.sourcePath, effects.sourcePath, pattern};
    bool directoryCreated = false, patternWritten = false;
    try
    {
        fs::create_directories(folder.parent_path());
        directoryCreated = fs::create_directory(folder);
        if (!directoryCreated)
            throw Error(Failure::ExistingFolder, "The chart set folder already exists.");
        WriteNewFile(pattern.sourcePath, patternText);
        patternWritten = true;
        WriteNewFile(effects.sourcePath, effectText);
    }
    catch (...)
    {
        if (directoryCreated)
        {
            std::error_code ignored;
            if (patternWritten)
                fs::remove(pattern.sourcePath, ignored);
            fs::remove(folder, ignored); // Non-recursive: unrelated files are never removed.
        }
        throw;
    }
    return result;
}
