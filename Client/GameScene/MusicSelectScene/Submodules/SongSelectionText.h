#pragma once
#include "Catalog/SongCatalog.h"
#include "Model/Submodules/NotationText.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <ranges>
#include <span>
#include <string_view>

namespace song_select
{
    [[nodiscard]] inline std::wstring DecodeDisplayText(const std::string_view value)
    {
        if (value.empty())
        {
            return {};
        }
        UINT codePage = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        int length = MultiByteToWideChar(codePage, flags, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0)
        {
            codePage = CP_ACP;
            flags = 0;
            length = MultiByteToWideChar(codePage, flags, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
        }
        if (length <= 0)
        {
            return L"?";
        }
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(codePage, flags, value.data(), static_cast<int>(value.size()),
                            result.data(), length);
        return result;
    }

    [[nodiscard]] inline std::wstring DisplayLines(const std::vector<std::string> &values,
                                                  std::wstring_view fallback = L"—")
    {
        std::wstring text;
        for (const auto &value : values)
        {
            if (value.empty()) continue;
            if (!text.empty()) text += L'\n';
            text += DecodeDisplayText(value);
        }
        return text.empty() ? std::wstring{fallback} : text;
    }

    // Song and chart texts are shown in the notation chosen in the options (NotationText).
    // A song without any name shows its YMM file name.
    [[nodiscard]] inline std::wstring SongTitle(const SongCatalogEntry &song, const TextNotation notation)
    {
        const auto title = NotationText(song.music.names, song.music.romanizedName, notation);
        return title.empty() ? song.metadataPath.stem().wstring() : DecodeDisplayText(title);
    }

    // Empty when the song names no artist.
    [[nodiscard]] inline std::wstring ArtistName(const SongCatalogEntry &song, const TextNotation notation)
    {
        return DecodeDisplayText(NotationText(song.music.artists, song.music.romanizedArtist, notation));
    }

    [[nodiscard]] inline std::wstring SongArtist(const SongCatalogEntry &song, const TextNotation notation)
    {
        auto artist = ArtistName(song, notation);
        return artist.empty() ? std::wstring{L"—"} : artist;
    }

    // Every notation of a block, the romanized line first, for the tooltip of a line that shows one.
    [[nodiscard]] inline std::wstring AllNames(const std::vector<std::string> &values, const std::string_view romanized)
    {
        std::wstring names = DecodeDisplayText(romanized);
        for (const auto &value : values)
        {
            if (value.empty() || value == romanized)
                continue;
            if (!names.empty())
                names += L" · ";
            names += DecodeDisplayText(value);
        }
        return names;
    }

    [[nodiscard]] inline std::size_t NameCount(const std::vector<std::string> &values, const std::string_view romanized)
    {
        const auto listed = std::ranges::count_if(
            values, [romanized](const auto &value) { return !value.empty() && value != romanized; });
        return static_cast<std::size_t>(listed) + (romanized.empty() ? 0 : 1);
    }

    // A chart without a name shows its YMP file name.
    [[nodiscard]] inline std::wstring PatternName(const SongCatalogPattern &pattern, const TextNotation notation)
    {
        const auto name = NotationText(pattern.pattern.name, pattern.pattern.romanizedName, notation);
        return name.empty() ? pattern.patternPath.stem().wstring() : DecodeDisplayText(name);
    }

    // The representative (first) maker; empty when the chart names none. Makers are different
    // people, so only the romanized line replaces the first one, never another maker.
    [[nodiscard]] inline std::wstring MakerName(const SongCatalogPattern &pattern, const TextNotation notation)
    {
        const auto &makers = pattern.pattern.makers;
        const std::string_view first = makers.empty() ? std::string_view{} : std::string_view{makers.front()};
        return DecodeDisplayText(NotationText(first, pattern.pattern.romanizedMaker, notation));
    }

    [[nodiscard]] inline std::wstring Lowercase(std::wstring value)
    {
        std::ranges::transform(value, value.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }

    [[nodiscard]] inline std::wstring FoldSearchText(std::wstring value)
    {
        if (value.empty()) return value;
        const int length = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                                        value.data(), static_cast<int>(value.size()), nullptr, 0,
                                        nullptr, nullptr, 0);
        if (length <= 0) return Lowercase(std::move(value));
        std::wstring folded(static_cast<std::size_t>(length), L'\0');
        if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, value.data(),
                           static_cast<int>(value.size()), folded.data(), length, nullptr, nullptr, 0))
            return Lowercase(std::move(value));
        return folded;
    }

    [[nodiscard]] inline std::vector<std::wstring> SplitSearchTerms(std::wstring_view text)
    {
        constexpr std::wstring_view whitespace = L" \t\r\n\f\v\u3000";
        std::vector<std::wstring> terms;
        auto begin = text.find_first_not_of(whitespace);
        while (begin != std::wstring_view::npos)
        {
            const auto end = text.find_first_of(whitespace, begin);
            terms.push_back(FoldSearchText(std::wstring{text.substr(begin, end - begin)}));
            begin = end == std::wstring_view::npos ? end : text.find_first_not_of(whitespace, end);
        }
        return terms;
    }

    [[nodiscard]] inline bool MatchesSearch(const SongCatalogEntry &song, std::span<const std::wstring> terms)
    {
        if (terms.empty())
            return true;

        std::vector<std::wstring> fields;
        const auto append = [&fields](const std::vector<std::string> &values)
        {
            for (const auto &value : values)
                fields.push_back(FoldSearchText(DecodeDisplayText(value)));
        };
        if (song.music.names.empty())
            fields.push_back(FoldSearchText(song.metadataPath.stem().wstring()));
        append(song.music.names);
        append(song.music.artists);
        // The romanized lines are searched whichever notation is shown.
        fields.push_back(FoldSearchText(DecodeDisplayText(song.music.romanizedName)));
        fields.push_back(FoldSearchText(DecodeDisplayText(song.music.romanizedArtist)));
        append(song.music.tags);
        for (const auto &pattern : song.patterns)
            append(pattern.pattern.tags);

        // Every word must match a real metadata field. Keep fields separate so
        // a term cannot accidentally span the end of one field and the next.
        return std::ranges::all_of(terms,
                                   [&fields](const auto &term)
                                   {
                                       return std::ranges::any_of(fields, [&term](const auto &field)
                                                                  { return field.find(term) != std::wstring::npos; });
                                   });
    }
} // namespace song_select
