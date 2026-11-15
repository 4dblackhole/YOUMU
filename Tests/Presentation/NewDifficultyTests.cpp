#include "Editing/ChartEditor.h"
#include "EditorScene/Submodules/EditorPatternCreation.h"
#include "Parsing/ChartParser.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <string_view>

namespace
{
    namespace fs = std::filesystem;

    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Write(const fs::path &path, std::string_view text)
    {
        fs::create_directories(path.parent_path());
        std::ofstream stream(path, std::ios::binary);
        stream << text;
        Check(static_cast<bool>(stream), "Fixture write failed.");
    }
    std::string Read(const fs::path &path)
    {
        std::ifstream stream(path, std::ios::binary);
        return {std::istreambuf_iterator<char>{stream}, {}};
    }
} // namespace
int main()
{
    try
    {
        const auto root = fs::absolute(fs::path("build/new-difficulty-tests") / std::to_string(GetCurrentProcessId()));
        Check(!fs::exists(root), "Use a fresh fixture root.");
        const auto music = root / L"Music/음악.ymm";
        Write(music, "Version: 1\nFile: sample.wav\nMusic Name 1: Test\nArtist 1: Test\n");
        // Catalog discovery needs a file; this test does not decode audio.
        const auto sample = root / L"Music/sample.wav";
        Write(sample, "fixture");
        auto parsedMusic = ChartParser{}.ParseMusicFile(music);
        Check(parsedMusic.Succeeded(), "YMM fixture must parse.");
        SongCatalogEntry song{music, sample, parsedMusic.document, {}};
        PatternDocument draft;
        draft.name = "Beginner";
        draft.makers = {"Tester"};
        draft.tags = {"new", "chart"};
        const auto created = Create(root, song, draft);
        Check(created.patternPath.parent_path() == root / L"Patterns/음악/Beginner", "Difficulty directory layout.");
        Check(created.pattern.musicMetadataFile == fs::path(L"Music/음악.ymm") &&
                  created.pattern.effectFile == L"음악 [Beginner].yme",
              "YMM root and sibling YME paths.");
        const auto parsed = ChartParser{}.ParsePatternFile(created.patternPath);
        const auto effects = ChartParser{}.ParseEffectFile(*created.effectPath);
        Check(parsed.Succeeded() && effects.Succeeded(), "New pair must parse.");
        ChartEditor editor(parsed.document, effects.document);
        Check(editor.Notes().empty() && parsed.document.timing.empty() && effects.document.commands.empty() &&
                  parsed.document.baseBpm == 120 && parsed.document.patternOffsetMilliseconds == 0 &&
                  parsed.document.mode == "Taiko" && editor.Timeline().MeasureLength(0) == Rational{1},
              "New chart defaults and empty score.");
        const auto before = Read(created.patternPath);
        bool refused{};
        try
        {
            static_cast<void>(Create(root, song, draft));
        }
        catch (const Error &e)
        {
            refused = e.reason == Failure::ExistingFolder;
        }
        Check(refused && Read(created.patternPath) == before, "Never overwrite existing difficulty.");
        for (const auto &name : {"../escape", "", "x/y", "name.", " name", "x\nheader"})
        {
            draft.name = name;
            refused = false;
            try
            {
                static_cast<void>(Create(root, song, draft));
            }
            catch (const Error &e)
            {
                refused = e.reason == Failure::InvalidName;
            }
            Check(refused, "Unsafe difficulty names must not create files.");
        }
        draft.name = "Invalid";
        draft.baseBpm = -1;
        refused = false;
        try
        {
            static_cast<void>(Create(root, song, draft));
        }
        catch (const std::exception &)
        {
            refused = true;
        }
        Check(refused && !fs::exists(root / L"Patterns/음악/Invalid"), "Invalid metadata leaves no folder.");
        draft.baseBpm = 120;
        draft.name = "Another";
        song.patterns.push_back(created);
        auto another = Create(root, song, draft);
        Check(another.patternPath.parent_path().parent_path() == created.patternPath.parent_path().parent_path(),
              "Reuse the song's actual pattern parent.");
        // Metadata typed before Create, including fields the former dialog did not ask for.
        draft.name = "Metadata";
        draft.makers = {"Tester", "테스터"};
        draft.judgementLevel = 80.25;
        draft.hpLevel = 7.5;
        draft.backgroundFile = "bg.png";
        draft.backgroundOffsetX = 12;
        draft.backgroundOffsetY = -4;
        const auto described = Create(root, song, draft);
        const auto reread = ChartParser{}.ParsePatternFile(described.patternPath);
        Check(reread.Succeeded() && reread.document.judgementLevel == 80.25 && reread.document.hpLevel.Value() == 7.5,
              "Create keeps the judgement and HP levels typed before it.");
        Check(reread.document.backgroundFile == fs::path("bg.png") && reread.document.backgroundOffsetX == 12 &&
                  reread.document.backgroundOffsetY == -4 && reread.document.makers.size() == 2,
              "Create keeps the BG file, its offset and every maker.");
        const auto catalog = SongCatalog{}.Load(root);
        Check(catalog.songs.size() == 1 && catalog.PatternCount() == 3, "New difficulties reload into the catalog.");
        Check(Read(music) == "Version: 1\nFile: sample.wav\nMusic Name 1: Test\nArtist 1: Test\n",
              "The shared YMM must remain untouched.");
        std::cout << "Difficulty creation tests passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
