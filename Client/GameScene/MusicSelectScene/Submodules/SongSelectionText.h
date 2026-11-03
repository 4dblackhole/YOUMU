#pragma once
#include "Catalog/SongCatalog.h"
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

    [[nodiscard]] inline std::wstring SongTitle(const SongCatalogEntry &song)
    {
        return song.music.names.empty() ? song.metadataPath.stem().wstring()
                                        : DecodeDisplayText(song.music.names.front());
    }

    [[nodiscard]] inline std::wstring SongArtist(const SongCatalogEntry &song)
    {
        return song.music.artists.empty() ? L"—" : DecodeDisplayText(song.music.artists.front());
    }

    [[nodiscard]] inline std::wstring PatternName(
        const SongCatalogPattern &pattern)
    {
        return pattern.pattern.name.empty() ? pattern.patternPath.stem().wstring()
                                            : DecodeDisplayText(pattern.pattern.name);
    }

    [[nodiscard]] inline std::wstring Lowercase(std::wstring value)
    {
        std::ranges::transform(value, value.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }

    [[nodiscard]] inline std::vector<std::wstring> SplitSearchTerms(std::wstring_view text)
    {
        constexpr std::wstring_view whitespace = L" \t\r\n\f\v";
        std::vector<std::wstring> terms;
        auto begin = text.find_first_not_of(whitespace);
        while (begin != std::wstring_view::npos)
        {
            const auto end = text.find_first_of(whitespace, begin);
            terms.push_back(Lowercase(std::wstring{text.substr(begin, end - begin)}));
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
                fields.push_back(Lowercase(DecodeDisplayText(value)));
        };
        if (song.music.names.empty())
            fields.push_back(Lowercase(song.metadataPath.stem().wstring()));
        append(song.music.names);
        append(song.music.artists);
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
