#include "TextCatalog.h"
#include "EnglishTexts.generated.h"
#include "YOUMUAssets.h"
#include "Submodules/ExplanationKeys.h"
#include <algorithm>
#include <stdexcept>

using namespace PhantomCore;

namespace
{
    const TranslationEntries &EnglishDefaults()
    {
        static const auto english = []
        {
            auto parsed = ParseLanguageFile(EnglishDefaultIni, "built-in English");
            if (!parsed.diagnostics.empty() || parsed.entries.empty())
                throw std::logic_error("Built-in English language data is invalid.");
            return parsed.entries;
        }();
        return english;
    }
    std::size_t Index(Language language) noexcept
    {
        switch (language)
        {
        case Language::Korean: return 0;
        case Language::Japanese: return 2;
        default: return 1;
        }
    }
}
TextCatalog::TextCatalog() : TextCatalog(UserAssetRoot() / L"Languages")
{
}
TextCatalog::TextCatalog(const std::filesystem::path &languageDirectory)
{
    const auto &english = EnglishDefaults();
    files_[0] = ReadLanguageFile(languageDirectory / L"ko.ini");
    files_[1] = ReadLanguageFile(languageDirectory / L"en.ini");
    files_[2] = ReadLanguageFile(languageDirectory / L"ja.ini");
    constexpr std::array names{"ko.ini: ", "en.ini: ", "ja.ini: "};
    for (std::size_t i = 0; i < files_.size(); ++i)
    {
        auto &file = files_[i];
        ValidateTranslations(file, english);
        for (const auto &diagnostic : file.diagnostics)
            diagnostics_.push_back(std::string(names[i]) + diagnostic);
    }
    profiles_ = {{{Language::Korean,
                   Get(Language::English, "Languages.Korean"),
                   {TextFontSource::System, L"Segoe UI"}},
                  {Language::English,
                   Get(Language::English, "Languages.English"),
                   {TextFontSource::System, L"Segoe UI"}},
                  {Language::Japanese,
                   Get(Language::English, "Languages.Japanese"),
                   {TextFontSource::System, L"Segoe UI"}}}};
}
std::wstring_view TextCatalog::Get(std::string_view key) const noexcept
{
    return Get(std::ranges::find(ExplanationTextKeys, key) != ExplanationTextKeys.end() ? language_ : Language::English, key);
}
std::wstring_view TextCatalog::Get(Language language, std::string_view key) const noexcept
{
    const auto &selected = files_[Index(language)].entries;
    if (const auto found = selected.find(key); found != selected.end())
        return found->second;
    // Partial translation packs fall back to English.
    if (const auto found = files_[1].entries.find(key); found != files_[1].entries.end())
        return found->second;
    const auto &english = EnglishDefaults();
    const auto found = english.find(key);
    return found == english.end() ? std::wstring_view{} : std::wstring_view{found->second};
}
std::span<const std::string> TextCatalog::Diagnostics() const noexcept
{
    return diagnostics_;
}
Language TextCatalog::CurrentLanguage() const noexcept
{
    return language_;
}
std::uint64_t TextCatalog::Revision() const noexcept
{
    return revision_;
}
const LanguageProfile &TextCatalog::CurrentProfile() const noexcept
{
    return profiles_[Index(language_)];
}
std::span<const LanguageProfile> TextCatalog::Languages() const noexcept
{
    return profiles_;
}
void TextCatalog::SetLanguage(Language language) noexcept
{
    if (language_ != language)
    {
        language_ = language;
        ++revision_;
    }
}
void TextCatalog::ApplyFont(TextVisualComponent &text) const
{
    text.SetFont(CurrentProfile().font);
}
void TextCatalog::ApplyFont(ComboBoxBehaviorComponent &combo) const
{
    combo.SetFont(CurrentProfile().font);
}
