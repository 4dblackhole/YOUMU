#pragma once
#include "Editing/IEditorDocument.h"
#include "EditorValueParsing.h"
#include "PhantomCore.h"
#include "Presentation/UiColors.h"
#include "Texts/EditorScene/EditorTexts.h"
#include <Windows.h>
#include <cmath>
#include <stdexcept>

namespace editor_ui
{

    constexpr auto Background = ui_colors::editor::Background, Paper = ui_colors::editor::Paper;
    constexpr auto Ink = ui_colors::editor::Ink, Blue = ui_colors::editor::Accent, Pale = ui_colors::editor::Pale;
    constexpr auto White = ui_colors::White;
    inline std::wstring Wide(const std::string &s)
    {
        const int n =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<std::size_t>(n), L'\0');
        if (n)
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    }
    inline std::string Utf8(const std::wstring &w)
    {
        const int n =
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<std::size_t>(n), '\0');
        if (n)
            WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
        return s;
    }

    // The engine lays text out on one line without reporting its width before
    // rendering; inline captions are placed with this per-glyph estimate.
    [[nodiscard]] inline float EstimateTextWidth(std::wstring_view text, float size) noexcept
    {
        float width = 0;
        for (const wchar_t character : text)
            width += character >= 0x1100 ? size : size * (character == L' ' ? .3F : .6F);
        return width;
    }

    // Breaks a caption into lines no wider than `width` (estimated), after spaces;
    // CJK text may break after any character.
    [[nodiscard]] inline std::wstring WrapCaption(std::wstring_view text, float size, float width)
    {
        std::wstring result, line;
        for (const wchar_t character : text)
        {
            line += character;
            if (EstimateTextWidth(line, size) <= width)
                continue;
            // Break at the last space for words (Hangul too); before the overflowing character
            // for kana/Han or one long word.
            const auto space = line.find_last_of(L' ');
            const bool kana = character >= 0x3040 && character <= 0x9FFF;
            const bool kanaOrHan = kana || (character >= 0xF900 && character <= 0xFAFF);
            const bool spaceInside = space != std::wstring::npos && space > 0;
            const bool atSpace = !kanaOrHan && spaceInside;
            const auto split = atSpace ? space : line.size() - 1;
            result += line.substr(0, split) + L'\n';
            line = line.substr(atSpace ? split + 1 : split);
        }
        return result + line;
    }

} // namespace editor_ui

namespace editor_ui
{
    std::optional<std::string> EditText(const std::wstring &, const std::string &,
                                        const EditorTextSet &);
}
