#pragma once
#include "PhantomCore.h"
#include "YOUMUAssets.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

// Redesign typography (UI-02): Nunito for Latin captions, Noto Sans KR for
// text that contains Hangul/Kana/Han, and Barlow Semi Condensed for numbers.
// All three ship in the built-in asset pack with their OFL licences.
namespace ui_fonts
{
    [[nodiscard]] inline PhantomCore::TextFont BuiltIn(const wchar_t *file)
    {
        return {PhantomCore::TextFontSource::File, ResolveBuiltInAsset(file).wstring()};
    }
    // Nunito has no CJK glyphs. Choose one face per string so a mixed title
    // never switches typefaces in the middle of a word.
    [[nodiscard]] inline bool NeedsCjkFace(std::wstring_view text) noexcept
    {
        return std::ranges::any_of(text, [](wchar_t c) { return c >= 0x1100; });
    }
    [[nodiscard]] inline PhantomCore::TextFont Text(std::wstring_view text = {})
    {
        return BuiltIn(NeedsCjkFace(text) ? L"assets\\fonts\\NotoSansKR.ttf" : L"assets\\fonts\\Nunito.ttf");
    }
    struct NumberFace
    {
        PhantomCore::TextFont font;
        std::uint16_t weight;
    };
    // Barlow Semi Condensed ships as static faces; the returned weight is the
    // face's own weight so DirectWrite never synthesizes a bolder outline.
    [[nodiscard]] inline NumberFace Number(std::uint16_t weight, bool italic = true)
    {
        if (italic)
            return weight >= 800 ? NumberFace{BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-ExtraBoldItalic.ttf"), 800}
                                 : NumberFace{BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-BoldItalic.ttf"), 700};
        if (weight >= 800)
            return {BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-ExtraBold.ttf"), 800};
        if (weight >= 700)
            return {BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-Bold.ttf"), 700};
        return {BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-Medium.ttf"), 500};
    }
} // namespace ui_fonts
