#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stop_token>
#include <type_traits>
#include <utility>

// Process-owned workers execute copied paths/domain data. Callers poll and
// cancel on their owner thread; work must not capture Scene/engine observers.
void EnqueueChartLoad(std::stop_token cancellation,
                      std::function<void(std::stop_token)> operation);

template<class Result> class ChartLoadJob;
template<class Work>
auto StartChartLoad(Work work) -> ChartLoadJob<std::invoke_result_t<Work&, std::stop_token>>;

template<class Result> class ChartLoadJob final
{
public:
    ChartLoadJob() noexcept = default;
    ~ChartLoadJob() { Cancel(); }
    ChartLoadJob(const ChartLoadJob&) = delete;
    ChartLoadJob& operator=(const ChartLoadJob&) = delete;
    ChartLoadJob(ChartLoadJob&& other) noexcept
        : cancellation_(std::move(other.cancellation_)), result_(std::move(other.result_)) {}
    ChartLoadJob& operator=(ChartLoadJob&& other) noexcept
    {
        if (this != &other)
        {
            Cancel();
            cancellation_ = std::move(other.cancellation_);
            result_ = std::move(other.result_);
        }
        return *this;
    }

    bool Busy() const noexcept { return result_.valid(); }
    void Cancel() noexcept
    {
        static_cast<void>(cancellation_.request_stop());
        // These futures come from packaged_task, so releasing them does not
        // join a worker. Empty stop state also avoids allocation during cleanup.
        result_ = {};
        cancellation_ = std::stop_source{std::nostopstate};
    }
    std::optional<Result> Poll()
    {
        if (!Busy() || result_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
            return {};
        auto result = result_.get();
        cancellation_ = std::stop_source{std::nostopstate};
        return result;
    }

private:
    template<class Work>
    friend auto StartChartLoad(Work work)
        -> ChartLoadJob<std::invoke_result_t<Work&, std::stop_token>>;

    ChartLoadJob(std::stop_source cancellation, std::future<Result> result) noexcept
        : cancellation_(std::move(cancellation)), result_(std::move(result)) {}

    std::stop_source cancellation_{std::nostopstate};
    std::future<Result> result_;
};

template<class Work>
auto StartChartLoad(Work work) -> ChartLoadJob<std::invoke_result_t<Work&, std::stop_token>>
{
    using Result = std::invoke_result_t<Work&, std::stop_token>;
    static_assert(!std::is_void_v<Result> && !std::is_reference_v<Result>,
                  "Chart load jobs return owned data.");
    auto task = std::make_shared<std::packaged_task<Result(std::stop_token)>>(std::move(work));
    auto result = task->get_future();
    std::stop_source cancellation;
    const auto token = cancellation.get_token();
    EnqueueChartLoad(token, [task, cancellation](std::stop_token workerStop) mutable
    {
        std::stop_callback stopWithWorker(workerStop, [cancellation]() mutable noexcept
        {
            static_cast<void>(cancellation.request_stop());
        });
        (*task)(cancellation.get_token());
    });
    return {std::move(cancellation), std::move(result)};
}
