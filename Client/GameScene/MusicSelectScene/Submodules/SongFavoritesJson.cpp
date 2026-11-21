#include "SongFavoritesJson.h"
#include "Catalog/ChartHash.h"
#include <Windows.h>
#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace
{
    constexpr std::wstring_view CollectionFormat = L"YOUMU collection";

    [[noreturn]] void Invalid() { throw std::invalid_argument("Invalid or unsupported collection file."); }
    // The value of one hexadecimal digit, or 16 when the character is not one.
    unsigned HexDigitValue(const wchar_t ch)
    {
        if (ch >= L'0' && ch <= L'9') return ch - L'0';
        if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
        if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
        return 16;
    }
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
    std::string LowerHash(std::wstring_view text)
    {
        std::string hash;
        for (const wchar_t ch : text)
        {
            if (ch > 127) Invalid();
            hash.push_back(static_cast<char>(ch >= L'A' && ch <= L'F' ? ch - L'A' + L'a' : ch));
        }
        if (!IsChartHash(hash)) Invalid();
        return hash;
    }
    // Fixed schema reader: objects are order-independent, with duplicate/unknown
    // members rejected. Invalid files are preserved rather than partially loaded.
    class Reader
    {
      public:
        explicit Reader(std::string_view source)
        {
            if (source.starts_with("\xEF\xBB\xBF")) source.remove_prefix(3);
            source_ = Wide(source);
        }
        static void Field(unsigned &fields, unsigned bit)
        {
            if (fields & bit) Invalid();
            fields |= bit;
        }
        void End()
        {
            Space();
            if (cursor_ != source_.size()) Invalid();
        }
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

      private:
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
        wchar_t Hex()
        {
            unsigned value{};
            for (unsigned i = 0; i < 4; ++i)
            {
                if (cursor_ == source_.size()) Invalid();
                const unsigned digit = HexDigitValue(source_[cursor_++]);
                if (digit == 16) Invalid();
                value = value * 16 + digit;
            }
            return static_cast<wchar_t>(value);
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
    std::pair<std::string, SongCollectionChart> ReadChart(Reader &in)
    {
        std::string hash;
        SongCollectionChart label;
        unsigned fields{};
        in.Object([&](const std::wstring &key)
        {
            if (key == L"sha256") { Reader::Field(fields, 1); hash = LowerHash(in.String()); }
            else if (key == L"title") { Reader::Field(fields, 2); label.title = in.String(); }
            else if (key == L"artist") { Reader::Field(fields, 4); label.artist = in.String(); }
            else if (key == L"pattern") { Reader::Field(fields, 8); label.pattern = in.String(); }
            else Invalid();
        });
        if ((fields & 1) == 0) Invalid();
        return {std::move(hash), std::move(label)};
    }
} // namespace

void ValidateSongCollection(const SongFavoriteFolder &collection)
{
    if (SongFavoritesValues::FolderName(collection.name) != collection.name) Invalid();
    static_cast<void>(Utf8(collection.name));
    for (const auto &[hash, label] : collection.charts)
    {
        if (!IsChartHash(hash) || LowerHash(Wide(hash)) != hash) Invalid();
        for (const auto *text : {&label.title, &label.artist, &label.pattern})
            static_cast<void>(Utf8(*text));
    }
}

void ValidateSongFavorites(const SongFavoritesValues &values)
{
    if (values.nextId == 0) Invalid();
    std::set<std::uint64_t> ids;
    for (std::size_t i = 0; i < values.folders.size(); ++i)
    {
        const auto &folder = values.folders[i];
        if (folder.id == 0 || folder.id >= values.nextId) Invalid();
        if (!ids.insert(folder.id).second) Invalid();
        ValidateSongCollection(folder);
        for (std::size_t j = 0; j < i; ++j)
            if (SongFavoritesValues::SameName(values.folders[j].name, folder.name)) Invalid();
    }
}

SongFavoriteFolder ReadSongCollectionJson(std::string_view source)
{
    Reader in{source};
    SongFavoriteFolder collection;
    unsigned fields{};
    in.Object([&](const std::wstring &key)
    {
        if (key == L"format") { Reader::Field(fields, 1); if (in.String() != CollectionFormat) Invalid(); }
        else if (key == L"version") { Reader::Field(fields, 2); if (in.Number() != 1) Invalid(); }
        else if (key == L"name") { Reader::Field(fields, 4); collection.name = SongFavoritesValues::FolderName(in.String()); }
        else if (key == L"charts")
        {
            Reader::Field(fields, 8);
            in.Array([&]
            {
                auto [hash, label] = ReadChart(in);
                // A chart listed twice keeps the first label that says something.
                auto &kept = collection.charts[hash];
                if (kept == SongCollectionChart{}) kept = std::move(label);
            });
        }
        else Invalid();
    });
    in.End();
    if (fields != 15) Invalid();
    ValidateSongCollection(collection);
    return collection;
}

std::string WriteSongCollectionJson(const SongFavoriteFolder &collection)
{
    ValidateSongCollection(collection);
    std::string result = "{\n  \"format\": " + Quote(CollectionFormat) + ",\n  \"version\": 1,\n  \"name\": " +
                         Quote(collection.name) + ",\n  \"charts\": [";
    bool first = true;
    for (const auto &[hash, label] : collection.charts)
    {
        result += first ? "\n" : ",\n";
        first = false;
        result += "    {\"sha256\": \"" + hash + "\"";
        for (const auto &[key, text] : {std::pair{"title", &label.title}, std::pair{"artist", &label.artist},
                                        std::pair{"pattern", &label.pattern}})
            if (!text->empty()) result += std::string(", \"") + key + "\": " + Quote(*text);
        result += "}";
    }
    return result + (first ? "]\n}\n" : "\n  ]\n}\n");
}

std::vector<LegacyFavoriteFolder> ReadLegacyFavoritesJson(std::string_view source)
{
    Reader in{source};
    std::vector<LegacyFavoriteFolder> folders;
    unsigned fields{};
    in.Object([&](const std::wstring &key)
    {
        if (key == L"version") { Reader::Field(fields, 1); if (in.Number() != 1) Invalid(); }
        else if (key == L"nextId") { Reader::Field(fields, 2); static_cast<void>(in.Number()); }
        else if (key == L"folders")
        {
            Reader::Field(fields, 4);
            in.Array([&]
            {
                LegacyFavoriteFolder folder;
                unsigned folderFields{};
                in.Object([&](const std::wstring &member)
                {
                    if (member == L"id") { Reader::Field(folderFields, 1); static_cast<void>(in.Number()); }
                    else if (member == L"name") { Reader::Field(folderFields, 2); folder.name = SongFavoritesValues::FolderName(in.String()); }
                    else if (member == L"patterns")
                    {
                        Reader::Field(folderFields, 4);
                        in.Array([&] { folder.patternPaths.push_back(Utf8(in.String())); });
                    }
                    else Invalid();
                });
                if (folderFields != 7) Invalid();
                folders.push_back(std::move(folder));
            });
        }
        else Invalid();
    });
    in.End();
    if (fields != 7) Invalid();
    return folders;
}
