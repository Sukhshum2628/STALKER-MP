// STALKER-MP — Networking configuration (Sprint-006, Step 1)
//
// Parsed from the [networking] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument. Timeouts are stored in
// milliseconds; conversion to ticks happens once at module Initialize (a later
// step). Engine-free and OS-free. ADR-007: no exceptions, Expected<T>.

#pragma once

#include <cstdint>

#include "stalkermp/core/Config.h"
#include "stalkermp/core/Expected.h"

namespace stalkermp::net
{
    struct NetworkingConfig
    {
        bool enabled = false;
        std::uint32_t maxConnections = 16;
        std::uint32_t connectionTimeoutMs = 15000;
        std::uint32_t handshakeTimeoutMs = 5000;
        std::uint32_t heartbeatIntervalMs = 2000;
        std::uint32_t debugFlags = 0;

        // Parses [networking]. Missing keys keep defaults; invalid values fail
        // with InvalidArgument. Cross-field rules: handshakeTimeoutMs <=
        // connectionTimeoutMs; heartbeatIntervalMs < connectionTimeoutMs.
        [[nodiscard]] static core::Expected<NetworkingConfig> FromConfig(const core::ConfigStore& config);
    };

    // Opaque debug flag bits (diagnostics consume these later). No behavior here.
    inline constexpr std::uint32_t kNetDebugPacketTrace = 0x00000001;
    inline constexpr std::uint32_t kNetDebugVerboseConn = 0x00000002;
    inline constexpr std::uint32_t kNetDebugHealthLog = 0x00000004;
} // namespace stalkermp::net
