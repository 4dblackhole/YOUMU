#pragma once
#include <Windows.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

// A fixture folder that is new for every run and removed when the test ends, whether it passed or threw.
// The name combines the process id, a clock reading and a counter, so a leftover folder from an aborted run
// or a reused process id can never make the next run fail. A run that is killed outright still leaves its
// folder behind; the unique name keeps that harmless.
class ScopedTestFolder final
{
  public:
    ScopedTestFolder(const std::filesystem::path &parent, std::wstring_view name)
    {
        std::filesystem::create_directories(parent);
        static std::atomic<unsigned> counter{};
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
            auto candidate = parent / (std::wstring(name) + L"-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                       std::to_wstring(ticks) + L"-" + std::to_wstring(counter++));
            // create_directory reports false instead of failing when the folder already exists.
            if (std::filesystem::create_directory(candidate))
            {
                path_ = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("Could not create a fresh test fixture folder.");
    }
    ScopedTestFolder(const ScopedTestFolder &) = delete;
    ScopedTestFolder &operator=(const ScopedTestFolder &) = delete;
    ~ScopedTestFolder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    [[nodiscard]] const std::filesystem::path &Path() const noexcept { return path_; }

  private:
    std::filesystem::path path_;
};
