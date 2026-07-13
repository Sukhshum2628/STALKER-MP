// STALKER-MP — Networking configuration parsing (Sprint-006, Step 1)
//
// See NetworkingConfig.h. Engine-free / OS-free; no exceptions, Expected<T>.
// Mirrors the BubbleConfiguration parsing convention.

#include "stalkermp/net/NetworkingConfig.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h" // common::Format (ADR-007-safe)

namespace stalkermp::net
{
    namespace
    {
        constexpr const char* kSection = "networking";

        // Reads a present integer key into a u32 target within [min, max].
        // Missing key leaves target unchanged. Present-but-invalid => error.
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
                        common::Format("[networking] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<NetworkingConfig> NetworkingConfig::FromConfig(const core::ConfigStore& config)
    {
        NetworkingConfig result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (const auto enabled = config.GetBool(kSection, "enabled"))
        {
            result.enabled = *enabled;
        }

        if (auto r = ReadU32(config, "max_connections", result.maxConnections, 1, 4096); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "connection_timeout_ms", result.connectionTimeoutMs, 1000, 0xFFFFFFFFll); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "handshake_timeout_ms", result.handshakeTimeoutMs, 500, 0xFFFFFFFFll); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "heartbeat_interval_ms", result.heartbeatIntervalMs, 100, 0xFFFFFFFFll); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "debug_flags", result.debugFlags, 0, 0xFFFFFFFFll); !r) { return r.GetError(); }

        // Cross-field rules.
        if (result.handshakeTimeoutMs > result.connectionTimeoutMs)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[networking] handshake_timeout_ms must be <= connection_timeout_ms");
        }
        if (result.heartbeatIntervalMs >= result.connectionTimeoutMs)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[networking] heartbeat_interval_ms must be < connection_timeout_ms");
        }

        return result;
    }
} // namespace stalkermp::net
