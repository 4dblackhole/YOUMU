#pragma once

#include <cctype>
#include <string_view>

[[nodiscard]] inline std::string_view Trim(std::string_view value) noexcept
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0)
    {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0)
    {
        value.remove_suffix(1);
    }
    return value;
}
