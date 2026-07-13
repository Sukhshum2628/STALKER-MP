// STALKER-MP — Timing utilities (Sprint-001, §7.11)

#pragma once

#include <chrono>
#include <string>

namespace stalkermp::common
{
    // Monotonic stopwatch for measuring durations.
    class Stopwatch
    {
    public:
        Stopwatch() noexcept
            : m_start(std::chrono::steady_clock::now())
        {
        }

        void Restart() noexcept { m_start = std::chrono::steady_clock::now(); }

        [[nodiscard]] std::chrono::nanoseconds Elapsed() const noexcept
        {
            return std::chrono::steady_clock::now() - m_start;
        }

        [[nodiscard]] double ElapsedSeconds() const noexcept
        {
            return std::chrono::duration<double>(Elapsed()).count();
        }

        [[nodiscard]] double ElapsedMilliseconds() const noexcept
        {
            return std::chrono::duration<double, std::milli>(Elapsed()).count();
        }

    private:
        std::chrono::steady_clock::time_point m_start;
    };

    // "2026-07-03 14:02:11.123" (local time). Used by the log formatter.
    [[nodiscard]] std::string FormatTimestamp(std::chrono::system_clock::time_point timePoint);

    // "2026-07-03T14:02:11Z" (UTC). For file names and machine-readable output.
    [[nodiscard]] std::string FormatTimestampIso8601Utc(std::chrono::system_clock::time_point timePoint);
} // namespace stalkermp::common
