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
    // The shipped faces. Views ask for a face on every Update, and resolving one checks the file on disk, so each
    // face is resolved the first time it is asked for and kept: the built-in pack does not change during a run.
    namespace faces
    {
        [[nodiscard]] inline const PhantomCore::TextFont &Latin()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\Nunito.ttf");
            return font;
        }
        [[nodiscard]] inline const PhantomCore::TextFont &Cjk()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\NotoSansKR.ttf");
            return font;
        }
        [[nodiscard]] inline const PhantomCore::TextFont &NumberExtraBoldItalic()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-ExtraBoldItalic.ttf");
            return font;
        }
        [[nodiscard]] inline const PhantomCore::TextFont &NumberBoldItalic()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-BoldItalic.ttf");
            return font;
        }
        [[nodiscard]] inline const PhantomCore::TextFont &NumberExtraBold()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-ExtraBold.ttf");
            return font;
        }
        [[nodiscard]] inline const PhantomCore::TextFont &NumberBold()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-Bold.ttf");
            return font;
        }
        [[nodiscard]] inline const PhantomCore::TextFont &NumberMedium()
        {
            static const PhantomCore::TextFont font = BuiltIn(L"assets\\fonts\\BarlowSemiCondensed-Medium.ttf");
            return font;
        }
    } // namespace faces
    // Nunito has no CJK glyphs. Choose one face per string so a mixed title
    // never switches typefaces in the middle of a word.
    [[nodiscard]] inline bool NeedsCjkFace(std::wstring_view text) noexcept
    {
        return std::ranges::any_of(text, [](wchar_t c) { return c >= 0x1100; });
    }
    [[nodiscard]] inline PhantomCore::TextFont Text(std::wstring_view text = {})
    {
        return NeedsCjkFace(text) ? faces::Cjk() : faces::Latin();
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
            return weight >= 800 ? NumberFace{faces::NumberExtraBoldItalic(), 800}
                                 : NumberFace{faces::NumberBoldItalic(), 700};
        if (weight >= 800)
            return {faces::NumberExtraBold(), 800};
        if (weight >= 700)
            return {faces::NumberBold(), 700};
        return {faces::NumberMedium(), 500};
    }
} // namespace ui_fonts
