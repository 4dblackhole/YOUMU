#pragma once

#include <exception>
#include <stop_token>

class ChartLoadCancelled final : public std::exception
{
public:
    const char* what() const noexcept override
    {
        return "Chart loading was cancelled.";
    }
};

inline void CheckChartLoadCancellation(const std::stop_token& stop)
{
    if (stop.stop_requested())
        throw ChartLoadCancelled{};
}
