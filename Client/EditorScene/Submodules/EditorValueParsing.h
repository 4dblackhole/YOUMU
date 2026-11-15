#pragma once
#include "Parsing/ChartParser.h"
#include <cmath>
#include <stdexcept>

inline std::string Fraction(Rational beat)
{
    return std::to_string(beat.Numerator()) + "/" + std::to_string(beat.Denominator());
}
// A measure length in quarter notes when it is whole (3/4, 4/4, 7/4), else in whole notes (7/8).
inline std::string MeasureLengthText(Rational length)
{
    const Rational quarters = length * 4;
    return quarters.Denominator() == 1 ? std::to_string(quarters.Numerator()) + "/4" : Fraction(length);
}
inline double Number(const std::string &value)
{
    std::size_t used{};
    const double result = std::stod(value, &used);
    if (used != value.size() || !std::isfinite(result))
        throw std::invalid_argument("Invalid number.");
    return result;
}
inline std::int64_t Integer(const std::string &value)
{
    std::size_t used{};
    const auto result = std::stoll(value, &used);
    if (used != value.size())
        throw std::invalid_argument("Invalid integer.");
    return result;
}
inline MusicalPosition Position(const std::string &measure, const std::string &fraction)
{
    MusicalPosition result;
    const auto oneBased = Integer(measure);
    if (oneBased < 1 || !TryParseMusicalPosition(fraction, oneBased - 1, result))
        throw std::invalid_argument("Use a one-based measure and N/D without internal spaces.");
    return result;
}
inline JudgeLevel ParseJudgeLevel(const std::string &value)
{
    return JudgeLevel{Number(value)};
}
