#include "stalkermp/common/Time.h"

#include <cstdio>
#include <ctime>

namespace stalkermp::common
{
    namespace
    {
        // localtime/gmtime portability: MSVC provides *_s, POSIX provides *_r.
        [[nodiscard]] std::tm ToLocalTime(const std::time_t time) noexcept
        {
            std::tm result{};
#if defined(_MSC_VER)
            localtime_s(&result, &time);
#else
            localtime_r(&time, &result);
#endif
            return result;
        }

        [[nodiscard]] std::tm ToUtcTime(const std::time_t time) noexcept
        {
            std::tm result{};
#if defined(_MSC_VER)
            gmtime_s(&result, &time);
#else
            gmtime_r(&time, &result);
#endif
            return result;
        }
    } // namespace

    std::string FormatTimestamp(const std::chrono::system_clock::time_point timePoint)
    {
        const std::time_t seconds = std::chrono::system_clock::to_time_t(timePoint);
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      timePoint.time_since_epoch()) %
                                  1000;
        const std::tm local = ToLocalTime(seconds);

        // Sized for the theoretical value ranges of std::tm fields so the
        // compiler can prove no truncation is possible.
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                      local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                      local.tm_hour, local.tm_min, local.tm_sec,
                      static_cast<int>(milliseconds.count()));
        return buffer;
    }

    std::string FormatTimestampIso8601Utc(const std::chrono::system_clock::time_point timePoint)
    {
        const std::time_t seconds = std::chrono::system_clock::to_time_t(timePoint);
        const std::tm utc = ToUtcTime(seconds);

        // See FormatTimestamp for the buffer sizing rationale.
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                      utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                      utc.tm_hour, utc.tm_min, utc.tm_sec);
        return buffer;
    }
} // namespace stalkermp::common
