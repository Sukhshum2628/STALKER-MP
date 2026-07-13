// STALKER-MP — Transport configuration (Sprint-006, Step 1)
//
// Parsed from the [transport] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument. bindAddress is only
// checked for non-emptiness and length here — syntactic/semantic address
// validation happens inside PlatformSockets.cpp at bind (a later step, ADR-009).
// Engine-free and OS-free. ADR-007: no exceptions, Expected<T>.

#pragma once

#include <cstdint>
#include <string>

#include "stalkermp/core/Config.h"
#include "stalkermp/core/Expected.h"

namespace stalkermp::net
{
    struct TransportConfig
    {
        std::uint16_t listenPort = 27015;
        std::string bindAddress = "0.0.0.0";
        std::uint32_t backlog = 32;
        std::uint32_t mtuPayloadBytes = 1200;
        std::uint32_t maxOutgoingQueue = 256;
        std::uint32_t maxIncomingQueue = 256;
        std::uint32_t maxBytesPerTick = 262144;
        std::uint16_t reliableWindow = 256;
        std::uint32_t reassemblyBudgetTicks = 120;

        // Parses [transport]. Cross-field rules: mtuPayloadBytes >
        // kPacketHeaderWireSize; maxBytesPerTick >= mtuPayloadBytes.
        [[nodiscard]] static core::Expected<TransportConfig> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::net
