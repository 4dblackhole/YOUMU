#pragma once

#include <string>
#include <string_view>

// The engine lays text out on one line, so notices are broken to a width here. Widths are
// estimates per glyph (Latin, space, CJK). Lines break after a space; Japanese kana and Han
// also break after any character, while Hangul words stay whole like Latin ones.
namespace ui_text
{
    [[nodiscard]] inline float EstimateGlyphWidth(const wchar_t character, const float size) noexcept
    {
        if (character >= 0x1100)
            return size;
        return size * (character == L' ' ? 0.3F : 0.6F);
    }
    [[nodiscard]] inline float EstimateWidth(const std::wstring_view text, const float size) noexcept
    {
        float width = 0;
        for (const wchar_t character : text)
            width += EstimateGlyphWidth(character, size);
        return width;
    }
    [[nodiscard]] inline bool BreaksAnywhere(const wchar_t character) noexcept
    {
        const bool kanaOrHan = character >= 0x3040 && character <= 0x9FFF;
        return kanaOrHan || (character >= 0xF900 && character <= 0xFAFF);
    }
    [[nodiscard]] inline std::wstring WrapText(const std::wstring_view text, const float size, const float width)
    {
        std::wstring result, line;
        float lineWidth = 0;
        std::size_t breakAfter = std::wstring::npos;
        for (const wchar_t character : text)
        {
            if (character == L'\n')
            {
                result += line + L'\n';
                line.clear();
                lineWidth = 0;
                breakAfter = std::wstring::npos;
                continue;
            }
            line += character;
            lineWidth += EstimateGlyphWidth(character, size);
            const bool breakable = character == L' ' || BreaksAnywhere(character);
            if (breakable)
                breakAfter = line.size();
            if (lineWidth <= width || line.size() == 1)
                continue;
            // Break at the last opportunity before this glyph, or else just before it.
            const bool earlierBreak = breakAfter != std::wstring::npos && breakAfter < line.size();
            const std::size_t cut = earlierBreak ? breakAfter : line.size() - 1;
            auto head = line.substr(0, cut);
            while (!head.empty() && head.back() == L' ')
                head.pop_back();
            result += head + L'\n';
            line.erase(0, cut);
            while (!line.empty() && line.front() == L' ')
                line.erase(0, 1);
            lineWidth = EstimateWidth(line, size);
            breakAfter = std::wstring::npos;
        }
        return result + line;
    }
} // namespace ui_text
