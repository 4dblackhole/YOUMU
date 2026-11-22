#include "Records/RecordOptionText.h"
#include <algorithm>
#include <format>

namespace
{
    // Two decimals at most, without trailing zeros: 1.5, 1.25, 180.
    [[nodiscard]] std::wstring Decimal(const double value)
    {
        auto text = std::format(L"{:.2f}", value);
        while (text.ends_with(L'0'))
            text.pop_back();
        if (text.ends_with(L'.'))
            text.pop_back();
        return text;
    }
} // namespace

std::vector<std::wstring> RecordOptionTokens(const PlayOptions &options)
{
    std::vector<std::wstring> tokens;
    if (options.accelerate != 1.0)
        tokens.push_back(L"AC" + Decimal(options.accelerate) + L"x");
    if (options.judgeLevel)
        tokens.emplace_back(L"JL");
    if (options.mirror)
        tokens.emplace_back(L"MR");
    if (options.random)
        tokens.emplace_back(L"RD");
    if (options.ignoreScrollEffects)
        tokens.emplace_back(L"CS");
    const bool bpmScroll = options.scrollUnit == PlayOptions::ScrollUnit::Bpm;
    if (bpmScroll || options.scrollSpeed != 1.0)
        tokens.push_back(L"SS" + Decimal(options.scrollSpeed) + (bpmScroll ? L"" : L"x"));
    return tokens;
}

std::wstring JoinRecordOptions(const std::vector<std::wstring> &tokens, const std::size_t shown)
{
    const auto count = std::min(shown, tokens.size());
    std::wstring text;
    for (std::size_t i = 0; i < count; ++i)
        text += (i == 0 ? L"" : L",") + tokens[i];
    if (count < tokens.size())
        text += count == 0 ? L"…" : L",…";
    return text;
}
