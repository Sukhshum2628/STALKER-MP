// STALKER-MP — Player configuration parsing (Sprint-007, Step 2)
//
// See PlayerConfiguration.h. Engine-free / OS-free; no exceptions, Expected<T>.
// All durations are tick counts (no ms/wall-clock in control flow). Mirrors the
// Sprint-006 TransportConfig::FromConfig parse/validate style.

#include "stalkermp/player/PlayerConfiguration.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::player
{
    namespace
    {
        constexpr const char* kSection = "player";

        // Read an optional [player] integer key into a std::uint32_t, validating
        // it lies in [min, max]; leaves the default in place when the key is
        // absent; returns InvalidArgument (value outcome) when out of range.
        [[nodiscard]] core::Expected<void>
        ReadU32(const core::ConfigStore& config, const char* key, std::uint32_t& target,
                std::int64_t min, std::int64_t max)
        {
            if (const auto raw = config.GetInt(kSection, key))
            {
                if (*raw < min || *raw > max)
                {
                    return core::MakeError(
                        core::ErrorCode::InvalidArgument,
                        common::Format("[player] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<PlayerConfiguration> PlayerConfiguration::FromConfig(const core::ConfigStore& config)
    {
        PlayerConfiguration result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        // maxPlayers: at least 1 (a session must allow at least one player).
        if (auto r = ReadU32(config, "max_players", result.maxPlayers, 1, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        // respawn_delay_ticks: full uint32 range (0 = immediate respawn).
        if (auto r = ReadU32(config, "respawn_delay_ticks", result.respawnDelayTicks, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }
        // reconnect_retention_ticks: full uint32 range (0 = no retention).
        if (auto r = ReadU32(config, "reconnect_retention_ticks", result.reconnectRetentionTicks, 0,
                             0xFFFFFFFFll);
            !r)
        {
            return r.GetError();
        }
        // spawn_policy_id: type/range validation only (0 = default). Unknown
        // non-zero ids are accepted and stored; policy resolution is a later step
        // (Step 8/10) — Step 2 does not couple to the policy set.
        if (auto r = ReadU32(config, "spawn_policy_id", result.spawnPolicyId, 0, 0xFFFFFFFFll); !r)
        {
            return r.GetError();
        }

        // Cross-field rule: maxPlayers >= 1 (guaranteed by the per-key min above;
        // re-asserted here as the frozen cross-field invariant, §11).
        if (result.maxPlayers < 1)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "[player] max_players must be >= 1");
        }

        return result;
    }
} // namespace stalkermp::player
