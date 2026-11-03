#pragma once
#include <cmath>
#include <compare>
#include <cstdint>
#include <stdexcept>
#include <string>

// HP drain multiplier; independent of judgement windows, with exact hundredths.
class HpLevel final
{
  public:
    static constexpr double Minimum = 1.0, Maximum = 100.0, Default = 9.0;
    HpLevel(double value = Default)
    {
        if (!IsValid(value))
            throw std::invalid_argument("HP must be 1.00..100.00 in steps of 0.01.");
        hundredths_ = static_cast<std::uint16_t>(std::lround(value * 100));
    }
    static bool IsValid(double value) noexcept
    {
        return std::isfinite(value) && value >= Minimum && value <= Maximum &&
               std::abs(value * 100 - std::round(value * 100)) < 1e-8;
    }
    double Value() const noexcept
    {
        return hundredths_ / 100.0;
    }
    std::string Format() const
    {
        return std::to_string(hundredths_ / 100) + "." + (hundredths_ % 100 < 10 ? "0" : "") +
               std::to_string(hundredths_ % 100);
    }
    auto operator<=>(const HpLevel &) const = default;

  private:
    std::uint16_t hundredths_{900};
};
