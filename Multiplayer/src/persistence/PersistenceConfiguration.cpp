// STALKER-MP — Persistence configuration parsing (Sprint-011, Step 2)
//
// See PersistenceConfiguration.h. Engine-free / OS-free; no exceptions,
// Expected<T>. Mirrors the Sprint-008/009/010 *Config::FromConfig parse/validate
// style.

#include "stalkermp/persistence/PersistenceConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::persistence
{
    namespace
    {
        constexpr const char* kSection = "persistence";

        [[nodiscard]] core::Expected<void>
        ReadU32(const core::ConfigStore& config, const char* key, std::uint32_t& target, std::int64_t min,
                std::int64_t max)
        {
            if (const auto raw = config.GetInt(kSection, key))
            {
                if (*raw < min || *raw > max)
                {
                    return core::MakeError(
                        core::ErrorCode::InvalidArgument,
                        common::Format("[persistence] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<PersistenceConfiguration> PersistenceConfiguration::FromConfig(const core::ConfigStore& config)
    {
        PersistenceConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (auto r = ReadU32(config, "queue_depth", result.queueDepth, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "autosave_interval_ticks", result.autosaveIntervalTicks, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "max_retries", result.maxRetries, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "retry_backoff_ticks", result.retryBackoffTicks, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "backpressure_high_watermark", result.backpressureHighWatermark, 0,
                             0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "backpressure_low_watermark", result.backpressureLowWatermark, 0, 0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "version", result.version, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rules. queueDepth/version minimums are enforced per-field above;
        // the watermark ordering is validated here (low <= high <= queueDepth).
        if (result.queueDepth < 1 || result.version < 1)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[persistence] queueDepth/version must be >= 1");
        }
        if (result.backpressureLowWatermark > result.backpressureHighWatermark)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("[persistence] backpressure_low_watermark '{}' must be <= "
                               "backpressure_high_watermark '{}'",
                               result.backpressureLowWatermark, result.backpressureHighWatermark));
        }
        if (result.backpressureHighWatermark > result.queueDepth)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("[persistence] backpressure_high_watermark '{}' must be <= queue_depth '{}'",
                               result.backpressureHighWatermark, result.queueDepth));
        }

        return result;
    }
} // namespace stalkermp::persistence
