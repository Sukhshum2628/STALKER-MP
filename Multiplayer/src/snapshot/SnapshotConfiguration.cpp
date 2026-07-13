// STALKER-MP — Snapshot configuration parsing (Sprint-008, Step 2)
//
// See SnapshotConfiguration.h. Engine-free / OS-free; no exceptions, Expected<T>.
// Mirrors the Sprint-006/007 *Config::FromConfig parse/validate style.

#include "stalkermp/snapshot/SnapshotConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::snapshot
{
    namespace
    {
        constexpr const char* kSection = "snapshot";

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
                        common::Format("[snapshot] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<SnapshotConfiguration> SnapshotConfiguration::FromConfig(const core::ConfigStore& config)
    {
        SnapshotConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (auto r = ReadU32(config, "pool_capacity", result.poolCapacity, 2, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "max_entities", result.maxEntities, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "max_players", result.maxPlayers, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "version", result.version, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "queue_depth", result.queueDepth, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "build_budget_ticks", result.buildBudgetTicks, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rules.
        if (result.poolCapacity < 2)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "[snapshot] pool_capacity must be >= 2");
        }
        if (result.queueDepth > result.poolCapacity)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("[snapshot] queue_depth ({}) must be <= pool_capacity ({})", result.queueDepth,
                               result.poolCapacity));
        }

        return result;
    }
} // namespace stalkermp::snapshot
