// STALKER-MP — Player subsystem configuration (Sprint-007, Step 2)
//
// Parsed from the [player] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument (value outcome). All
// durations are stored as TICK COUNTS (never ms/wall-clock in control flow).
// Engine-free and OS-free. ADR-007: no exceptions, no RTTI, no iostream;
// Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no consumer,
// no registry, no lifecycle, no service (those are later steps).

#pragma once

#include <cstdint>

#include "stalkermp/core/Config.h"     // core::ConfigStore
#include "stalkermp/core/Expected.h"   // core::Expected

namespace stalkermp::player
{
    struct PlayerConfiguration
    {
        // Capacity of the player registry (Step 3 consumes this). >= 1.
        std::uint32_t maxPlayers = 32;

        // Respawn delay in ticks (Step 5 respawn scheduling). 0 = immediate.
        std::uint32_t respawnDelayTicks = 300;

        // How long a Suspended record is retained for reclaim, in ticks
        // (Step 7 reconnect window). 0 = no retention.
        std::uint32_t reconnectRetentionTicks = 3600;

        // Identifier selecting the deterministic spawn-location policy
        // (Step 8/10 consume this). 0 = default policy.
        std::uint32_t spawnPolicyId = 0;

        // Parses [player]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Cross-field rule: maxPlayers >= 1. Durations are ticks.
        [[nodiscard]] static core::Expected<PlayerConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::player
