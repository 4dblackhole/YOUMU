#include "SongFavoritesJson.h"
#include <Windows.h>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace
{
    [[noreturn]] void Invalid() { throw std::invalid_argument("Invalid or unsupported favorites.json."); }
    std::wstring Wide(std::string_view text)
    {
        if (text.empty()) return {};
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) Invalid();
        const auto size = static_cast<int>(text.size());
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
        if (count == 0) Invalid();
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, result.data(), count)) Invalid();
        return result;
    }
    std::string Utf8(std::wstring_view text)
    {
        if (text.empty()) return {};
        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) Invalid();
        const auto size = static_cast<int>(text.size());
        const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), size, nullptr, 0, nullptr, nullptr);
        if (count == 0) Invalid();
        std::string result(static_cast<std::size_t>(count), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), size, result.data(), count, nullptr, nullptr)) Invalid();
        return result;
    }
    // Fixed schema reader: objects are order-independent, with duplicate/unknown
    // members rejected. Invalid files are preserved rather than partially loaded.
    class Reader
    {
      public:
        explicit Reader(std::wstring source) : source_(std::move(source)) {}
        SongFavoritesValues Read()
        {
            SongFavoritesValues result;
            unsigned fields{};
            Object([&](const std::wstring &key)
            {
                if (key == L"version") { Field(fields, 1); if (Number() != 1) Invalid(); }
                else if (key == L"nextId") { Field(fields, 2); result.nextId = Number(); }
                else if (key == L"folders")
                {
                    Field(fields, 4);
                    Array([&] { result.folders.push_back(Folder()); });
                }
                else Invalid();
            });
            Space();
            if (fields != 7 || cursor_ != source_.size()) Invalid();
            ValidateSongFavorites(result);
            return result;
        }
      private:
        static void Field(unsigned &fields, unsigned bit)
        {
            if (fields & bit) Invalid();
            fields |= bit;
        }
        void Space()
        {
            while (cursor_ < source_.size() && std::wstring_view{L" \t\r\n"}.find(source_[cursor_]) != std::wstring_view::npos) ++cursor_;
        }
        bool Take(wchar_t character)
        {
            Space();
            if (cursor_ < source_.size() && source_[cursor_] == character) { ++cursor_; return true; }
            return false;
        }
        void Expect(wchar_t character) { if (!Take(character)) Invalid(); }
        template<class Function> void Object(Function member)
        {
            Expect(L'{');
            if (Take(L'}')) return;
            do { const auto key = String(); Expect(L':'); member(key); } while (Take(L','));
            Expect(L'}');
        }
        template<class Function> void Array(Function element)
        {
            Expect(L'[');
            if (Take(L']')) return;
            do { element(); } while (Take(L','));
            Expect(L']');
        }
        std::uint64_t Number()
        {
            Space();
            std::string digits;
            while (cursor_ < source_.size() && source_[cursor_] >= L'0' && source_[cursor_] <= L'9')
                digits.push_back(static_cast<char>(source_[cursor_++]));
            if (digits.empty() || (digits.size() > 1 && digits.front() == '0')) Invalid();
            std::uint64_t value{};
            const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) Invalid();
            return value;
        }
        wchar_t Hex()
        {
            unsigned value{};
            for (unsigned i = 0; i < 4; ++i)
            {
                if (cursor_ == source_.size()) Invalid();
                const auto ch = source_[cursor_++];
                const unsigned digit = ch >= L'0' && ch <= L'9' ? ch - L'0' :
                    ch >= L'a' && ch <= L'f' ? ch - L'a' + 10 : ch >= L'A' && ch <= L'F' ? ch - L'A' + 10 : 16;
                if (digit == 16) Invalid();
                value = value * 16 + digit;
            }
            return static_cast<wchar_t>(value);
        }
        std::wstring String()
        {
            Expect(L'"');
            std::wstring result;
            while (cursor_ < source_.size())
            {
                auto ch = source_[cursor_++];
                if (ch == L'"') { static_cast<void>(Utf8(result)); return result; }
                if (ch < 32) Invalid();
                if (ch == L'\\')
                {
                    if (cursor_ == source_.size()) Invalid();
                    ch = source_[cursor_++];
                    switch (ch)
                    {
                    case L'"': case L'\\': case L'/': break;
                    case L'b': ch = L'\b'; break; case L'f': ch = L'\f'; break;
                    case L'n': ch = L'\n'; break; case L'r': ch = L'\r'; break; case L't': ch = L'\t'; break;
                    case L'u': ch = Hex(); break;
                    default: Invalid();
                    }
                }
                result.push_back(ch);
            }
            Invalid();
        }
        SongFavoriteFolder Folder()
        {
            SongFavoriteFolder folder;
            unsigned fields{};
            Object([&](const std::wstring &key)
            {
                if (key == L"id") { Field(fields, 1); folder.id = Number(); }
                else if (key == L"name") { Field(fields, 2); folder.name = String(); }
                else if (key == L"patterns")
                {
                    Field(fields, 4);
                    Array([&] { folder.patternKeys.insert(Utf8(String())); });
                }
                else Invalid();
            });
            if (fields != 7) Invalid();
            return folder;
        }
        std::wstring source_;
        std::size_t cursor_{};
    };
    std::string Quote(std::wstring_view text)
    {
        std::string result{"\""};
        constexpr char hex[] = "0123456789abcdef";
        for (const unsigned char ch : Utf8(text))
        {
            if (ch == '"' || ch == '\\') { result.push_back('\\'); result.push_back(static_cast<char>(ch)); }
            else if (ch < 32) { result += "\\u00"; result.push_back(hex[ch >> 4]); result.push_back(hex[ch & 15]); }
            else result.push_back(static_cast<char>(ch));
        }
        return result + '"';
    }
}

void ValidateSongFavorites(const SongFavoritesValues &values)
{
    std::set<std::uint64_t> ids;
    for (const auto &folder : values.folders)
    {
        if (folder.id == 0 || folder.id >= values.nextId || !ids.insert(folder.id).second ||
            SongFavoritesValues::FolderName(folder.name) != folder.name) Invalid();
        static_cast<void>(Utf8(folder.name));
        for (const auto &key : folder.patternKeys)
        {
            const auto wide = Wide(key);
            const std::filesystem::path path{wide};
            if (wide.empty() || wide.find(L'\0') != wide.npos || !path.is_absolute() || SongFavorites::PatternKey(path) != key) Invalid();
        }
    }
    if (values.nextId == 0) Invalid();
}
SongFavoritesValues ReadSongFavoritesJson(std::string_view source)
{
    if (source.starts_with("\xEF\xBB\xBF")) source.remove_prefix(3);
    return Reader{Wide(source)}.Read();
}
std::string WriteSongFavoritesJson(const SongFavoritesValues &values)
{
    ValidateSongFavorites(values);
    std::string result = "{\n  \"version\": 1,\n  \"nextId\": " + std::to_string(values.nextId) + ",\n  \"folders\": [";
    for (std::size_t i = 0; i < values.folders.size(); ++i)
    {
        const auto &folder = values.folders[i];
        result += (i ? ",\n" : "\n");
        result += "    {\"id\": " + std::to_string(folder.id) + ", \"name\": " + Quote(folder.name) + ", \"patterns\": [";
        bool first = true;
        for (const auto &key : folder.patternKeys)
        {
            if (!first) result += ", ";
            result += Quote(Wide(key));
            first = false;
        }
        result += "]}";
    }
    return result + "\n  ]\n}\n";
}
