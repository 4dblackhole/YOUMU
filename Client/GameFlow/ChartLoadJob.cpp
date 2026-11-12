#include "GameFlow/ChartLoadJob.h"

#include <array>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace
{
    class ChartLoadWorker final
    {
    public:
        static ChartLoadWorker& Instance()
        {
            static ChartLoadWorker worker;
            return worker;
        }
        void Enqueue(std::stop_token cancellation,
                     std::function<void(std::stop_token)> operation)
        {
            if (!operation)
                throw std::invalid_argument("A chart load operation is required.");
            {
                std::scoped_lock lock(mutex_);
                operations_.push_back({std::move(cancellation), std::move(operation)});
            }
            wake_.notify_one();
        }

    private:
        struct Operation
        {
            std::stop_token cancellation;
            std::function<void(std::stop_token)> execute;
        };

        ChartLoadWorker()
            : workers_{
                std::jthread([this](std::stop_token stop) { Run(stop); }),
                std::jthread([this](std::stop_token stop) { Run(stop); })}
        {
        }
        ~ChartLoadWorker()
        {
            for (auto& worker : workers_)
                static_cast<void>(worker.request_stop());
            wake_.notify_all();
        }
        bool WaitAndPop(std::stop_token stop, Operation& operation)
        {
            std::unique_lock lock(mutex_);
            static_cast<void>(wake_.wait(lock, stop, [this] { return !operations_.empty(); }));
            if (operations_.empty())
                return false;
            operation = std::move(operations_.front());
            operations_.pop_front();
            return true;
        }
        void Run(std::stop_token stop)
        {
            for (;;)
            {
                Operation operation;
                if (!WaitAndPop(stop, operation))
                    return;
                // Canceled captures are released on a worker, outside the queue
                // lock. Shutdown drains queued ownership without starting work.
                if (stop.stop_requested() || operation.cancellation.stop_requested())
                    continue;
                try { operation.execute(stop); }
                catch (...) { /* packaged_task normally stores errors in its future. */ }
            }
        }

        std::mutex mutex_;
        std::condition_variable_any wake_;
        std::deque<Operation> operations_;
        std::array<std::jthread, 2> workers_;
    };
}

void EnqueueChartLoad(std::stop_token cancellation,
                      std::function<void(std::stop_token)> operation)
{
    ChartLoadWorker::Instance().Enqueue(std::move(cancellation), std::move(operation));
}
