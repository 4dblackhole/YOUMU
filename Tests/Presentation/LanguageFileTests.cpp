#include "Texts/EditorScene/EditorTexts.h"
#include "Texts/GameScene/YOUMULogoScene/LogoTexts.h"
#include "Texts/GameScene/ResultScene/ResultTexts.h"
#include "Texts/GameScene/MusicSelectScene/MusicSelectTexts.h"
#include "Texts/TextCatalog.h"
#include "Texts/Submodules/ExplanationKeys.h"
#include <Windows.h>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace
{

    void Check(bool value, const char *message)
    {
        if (!value)
            throw std::runtime_error(message);
    }
    void Write(const std::filesystem::path &path, std::string_view bytes)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out)
            throw std::runtime_error("Could not write the isolated language fixture.");
    }
    void Parser()
    {
        const auto parsed = ParseLanguageFile("\xEF\xBB\xBF[Logo]\r\nStart=\"시작 😀\"\r\n"
                                                 R"ini(Options="  Options; # = \"quoted\"\nnext\tline\\  "
)ini"
                                                 "Exit=\"\"\nStart=duplicate\nBroken=\"unterminated\nEditor=Editor",
                                                 "fixture");
        Check(parsed.entries.at("Logo.Start") == L"시작 😀" && parsed.entries.at("Logo.Editor") == L"Editor" &&
                  parsed.entries.at("Logo.Exit").empty(),
              "BOM/UTF-8/CRLF/empty values must be supported.");
        Check(parsed.entries.at("Logo.Options") == L"  Options; # = \"quoted\"\nnext\tline\\  ",
              "Quoted escapes, delimiters and edge whitespace must round-trip.");
        Check(parsed.diagnostics.size() == 2, "Only duplicate and malformed entries must be diagnosed.");
        const auto invalid = ParseLanguageFile("[Logo]\nStart=\xFF", "invalid UTF-8");
        Check(invalid.entries.empty() && !invalid.diagnostics.empty(), "Invalid UTF-8 must not enter UI text.");
        auto formats =
            ParseLanguageFile("[Result]\npercent=\"{999999999999999999999999999999999999}%\"\n", "overflow");
        ValidateTranslations(formats, {{"Result.percent", L"{:.6f}%"}});
        Check(formats.entries.empty(), "An overflowing argument index must fall back safely.");
    }
    void Catalog(const std::filesystem::path &directory)
    {
        TextCatalog empty(directory);
        Check(Logo(empty).menu[0] == L"Game Start" && Editor(empty).save == L"SAVE" &&
                  MusicSelect(empty).sortModes[3] == L"BASE BPM",
              "Missing Korean and English files must use built-in English for every screen.");
        Check(empty.Get("MusicSelect.hpLevelLabel") == L"HP level",
              "Older language packs must retain an English fallback for the HP label.");
        Write(directory / "en.ini", "[Logo]\nOptions=Preferences\n");
        Write(directory / "ko.ini", "[Logo]\nStart=\"직접 시작\"\nExit=\"\"\n"
                                    "[Result]\npercent=\"{:d}%\"\nnoteCounts=\"Pending {2} / total {0} / done {1}\"\n"
                                    "[Unknown]\nkey=ignored\n");
        TextCatalog catalog(directory);
        const auto logo = Logo(catalog);
        const auto original = catalog.Get(Language::Korean, "Logo.Start");
        Check(logo.menu[0] == L"Game Start" && logo.menu[1] == L"Editor" && logo.menu[2] == L"Preferences" &&
                  logo.menu[3] == L"Exit" && original == L"직접 시작",
              "UI uses English while explicit pack reads preserve custom translated entries.");
        Check(Result(catalog).percent == L"{:.6f}%" && catalog.Diagnostics().size() == 2,
              "Incompatible format types and unknown keys must not replace defaults.");
        const int total = 12, done = 5, pending = 7;
        Check(std::vformat(catalog.Get(Language::Korean, "Result.noteCounts"), std::make_wformat_args(total, done, pending)) ==
                  L"Pending 7 / total 12 / done 5",
              "Translations may reorder existing arguments by index.");
        const auto revision = catalog.Revision();
        catalog.SetLanguage(Language::English);
        catalog.SetLanguage(Language::English);
        Check(catalog.Revision() == revision + 1 && Logo(catalog).menu[0] == L"Game Start" &&
                  original == L"직접 시작",
              "Switching languages must retain previous string-view lifetimes.");
        Write(directory / "ko.ini", "[Logo]\nStart=\"다음 실행\"\n");
        catalog.SetLanguage(Language::Korean);
        Check(catalog.Get(Language::Korean, "Logo.Start") == L"직접 시작" && Logo(catalog).menu[0] == L"Game Start",
              "File I/O must not occur during text lookup or language switching.");
        const TextCatalog restarted(directory);
        Check(restarted.Get(Language::Korean, "Logo.Start") == L"다음 실행", "A new catalog must read replaced language files.");
        Write(directory / "ko.ini", "[Logo]\nStart=\xFF");
        const TextCatalog corrupt(directory);
        Check(Logo(corrupt).menu[0] == L"Game Start", "A corrupt Korean pack must still fall back to English.");
    }
} // namespace
int main()
{
    try
    {
        static_assert(!std::is_copy_constructible_v<TextCatalog>);
        Parser();
        const auto directory =
            std::filesystem::absolute("build/obj/LanguageFileTests/Fixtures") /
            (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(directory);
        Catalog(directory);
        TextCatalog shipped("Client/Texts/Languages");
        Check(shipped.Diagnostics().empty() && shipped.Languages().size() == 3 && Logo(shipped).menu[0] == L"Game Start",
              "All three packs parse while UI captions use English.");
        const auto japanese = ReadLanguageFile("Client/Texts/Languages/ja.ini");
        for (const auto key : ExplanationTextKeys)
            Check(japanese.entries.contains(std::string(key)), "Japanese must cover every classified explanation key.");
        for (const auto language : {Language::Korean, Language::English, Language::Japanese})
        {
            shipped.SetLanguage(language);
            Check(shipped.Get("Options.title") == L"OPTIONS" && shipped.Get("Editor.menuSave") == L"Save" &&
                      shipped.Get("Result.errorAxis") == L"Error (ms)",
                  "Titles, action captions and numeric-axis labels must remain English.");
            Check(shipped.Get("Options.offsetHint") == shipped.Get(language, "Options.offsetHint") &&
                      shipped.Get("MusicSelect.patternErrorPrefix") == shipped.Get(language, "MusicSelect.patternErrorPrefix"),
                  "Explanations and error notices use the selected language.");
        }
        std::cout
            << "UTF-8 language files, English fallback, format validation and immutable catalog ownership passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
