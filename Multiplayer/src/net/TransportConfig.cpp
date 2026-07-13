// STALKER-MP — Transport configuration parsing (Sprint-006, Step 1)
//
// See TransportConfig.h. Engine-free / OS-free; no exceptions, Expected<T>.
// No OS address validation (One Platform Boundary) — only non-emptiness/length.

#include "stalkermp/net/TransportConfig.h"

#include <cstdint>

#include "stalkermp/common/StringUtil.h"  // common::Format
#include "stalkermp/net/PacketHeader.h"   // kPacketHeaderWireSize (cross-field bound)

namespace stalkermp::net
{
    namespace
    {
        constexpr const char* kSection = "transport";
        constexpr std::size_t kMaxAddressLength = 45; // IPv4/IPv6 textual max

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
                        common::Format("[transport] {} '{}' must be in [{}, {}]", key, *raw, min, max));
                }
                target = static_cast<std::uint32_t>(*raw);
            }
            return core::Success();
        }

        [[nodiscard]] core::Expected<void>
        ReadU16(const core::ConfigStore& config, const char* key, std::uint16_t& target,
                std::int64_t min, std::int64_t max)
        {
            std::uint32_t wide = target;
            if (auto r = ReadU32(config, key, wide, min, max); !r)
            {
                return r;
            }
            target = static_cast<std::uint16_t>(wide);
            return core::Success();
        }
    } // namespace

    core::Expected<TransportConfig> TransportConfig::FromConfig(const core::ConfigStore& config)
    {
        TransportConfig result;

        if (!config.HasSection(kSection))
        {
            return result; // all defaults
        }

        if (auto r = ReadU16(config, "listen_port", result.listenPort, 1, 65535); !r) { return r.GetError(); }

        if (const auto addr = config.GetString(kSection, "bind_address"))
        {
            if (addr->empty() || addr->size() > kMaxAddressLength)
            {
                return core::MakeError(core::ErrorCode::InvalidArgument,
                                       "[transport] bind_address must be non-empty and <= 45 chars");
            }
            result.bindAddress = *addr;
        }

        if (auto r = ReadU32(config, "backlog", result.backlog, 1, 1024); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "mtu_payload_bytes", result.mtuPayloadBytes, 256, 9000); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "max_outgoing_queue", result.maxOutgoingQueue, 1, 65536); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "max_incoming_queue", result.maxIncomingQueue, 1, 65536); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "max_bytes_per_tick", result.maxBytesPerTick, 1, 0xFFFFFFFFll); !r) { return r.GetError(); }
        if (auto r = ReadU16(config, "reliable_window", result.reliableWindow, 1, 65535); !r) { return r.GetError(); }
        if (auto r = ReadU32(config, "reassembly_budget_ticks", result.reassemblyBudgetTicks, 1, 100000); !r) { return r.GetError(); }

        // Cross-field rules.
        if (result.mtuPayloadBytes <= kPacketHeaderWireSize)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("[transport] mtu_payload_bytes must be > {}", kPacketHeaderWireSize));
        }
        if (result.maxBytesPerTick < result.mtuPayloadBytes)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "[transport] max_bytes_per_tick must be >= mtu_payload_bytes");
        }

        return result;
    }
} // namespace stalkermp::net
