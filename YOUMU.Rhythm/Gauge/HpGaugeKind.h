#pragma once
#include <algorithm>
#include <array>
#include <string_view>

// HP gauges a player can choose in Play options. Each gauge owns its clear criterion, which the play HP gauge
// and the result HP graph draw. Only the default gauge exists; further gauges, their rules and their order are
// undecided (HP-02), so adding one means a new kind, a description here and its IHpGauge implementation.
enum class HpGaugeKind : unsigned char
{
    Default
};

struct HpGaugeDescription
{
    HpGaugeKind kind{};
    std::string_view name; // English caption shown in Play options and on result option chips.
    double clearPercent{};
};

inline constexpr std::array HpGauges{HpGaugeDescription{HpGaugeKind::Default, "Default", 60.0}};

[[nodiscard]] constexpr const HpGaugeDescription *FindHpGauge(const HpGaugeKind kind) noexcept
{
    const auto found = std::ranges::find(HpGauges, kind, &HpGaugeDescription::kind);
    return found == HpGauges.end() ? nullptr : &*found;
}
