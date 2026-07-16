// STALKER-MP — Replication configuration parsing (Sprint-009, Step 2)
//
// See ReplicationConfiguration.h. Engine-free / OS-free; no exceptions,
// Expected<T>. Mirrors the Sprint-008 SnapshotConfiguration::FromConfig style.

#include "stalkermp/replication/ReplicationConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::replication
{
    namespace
    {
        constexpr const char* kSection = "replication";

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
                        common::Format("[replication] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<ReplicationConfiguration> ReplicationConfiguration::FromConfig(const core::ConfigStore& config)
    {
        ReplicationConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        // Per-field parse with inline minimum enforcement (present-but-invalid =>
        // value outcome). Minimums encode the cross-field invariants.
        if (auto r = ReadU32(config, "max_clients", result.maxClients, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "max_entities_per_update", result.maxEntitiesPerUpdate, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "max_players_per_update", result.maxPlayersPerUpdate, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "reliable_queue_depth", result.reliableQueueDepth, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "unreliable_queue_depth", result.unreliableQueueDepth, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "retry_limit", result.retryLimit, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "bandwidth_budget_bytes_per_tick", result.bandwidthBudgetBytesPerTick, 0,
                             0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "interest_radius_meters", result.interestRadiusMeters, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "update_budget_ticks", result.updateBudgetTicks, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        if (auto r = ReadU32(config, "version", result.version, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rules (defensive; the per-field minimums above already
        // enforce these, but the explicit checks document the invariants and
        // guard against a default being lowered in future).
        if (result.maxClients < 1 || result.maxEntitiesPerUpdate < 1 || result.maxPlayersPerUpdate < 1 ||
            result.reliableQueueDepth < 1 || result.unreliableQueueDepth < 1 || result.retryLimit < 1 ||
            result.version < 1)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[replication] maxClients/maxEntitiesPerUpdate/maxPlayersPerUpdate/"
                                   "reliableQueueDepth/unreliableQueueDepth/retryLimit/version must be >= 1");
        }

        return result;
    }
} // namespace stalkermp::replication
