// STALKER-MP — Replication configuration (Sprint-009, Step 2)
//
// Parsed from the [replication] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument (value outcome). All
// budgets are fixed integers; durations are tick counts (no wall-clock in
// control flow). Engine-free and OS-free. ADR-007: no exceptions, no RTTI, no
// iostream; Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no client
// registry, interest, delta, classifier, queue, packet, worker, manager,
// diagnostics, service/tick, message id, or Bootstrap wiring. Consumes only the
// Sprint-009 Step-01 vocabulary conceptually; depends on core config/expected.
//
// Cross-field: maxClients >= 1; maxEntitiesPerUpdate >= 1; maxPlayersPerUpdate
// >= 1; reliableQueueDepth >= 1; unreliableQueueDepth >= 1; retryLimit >= 1;
// version >= 1.

#pragma once

#include <cstdint>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::replication
{
    struct ReplicationConfiguration
    {
        // Maximum concurrently replicated clients (bounds the client registry).
        // Must be >= 1.
        std::uint32_t maxClients = 64;

        // Maximum entities emitted in a single per-client replication update
        // (bounded memory; Overflow value outcome when exceeded). Must be >= 1.
        std::uint32_t maxEntitiesPerUpdate = 1024;

        // Maximum players emitted in a single per-client update. Must be >= 1.
        std::uint32_t maxPlayersPerUpdate = 64;

        // Depth of the reliable outgoing queue (bounded memory). Must be >= 1.
        std::uint32_t reliableQueueDepth = 256;

        // Depth of the unreliable outgoing queue (bounded memory). Must be >= 1.
        std::uint32_t unreliableQueueDepth = 512;

        // Maximum retry attempts for a reliable record before it is dropped.
        // Must be >= 1.
        std::uint32_t retryLimit = 5;

        // Per-tick outgoing byte budget (bandwidth shaping; 0 = advisory/unbounded).
        std::uint32_t bandwidthBudgetBytesPerTick = 65536;

        // Interest radius in meters for distance-based relevance (0 = rely on
        // bubble membership only).
        std::uint32_t interestRadiusMeters = 150;

        // Per-tick replication build budget in ticks (diagnostic only; 0 =
        // unbounded/advisory).
        std::uint32_t updateBudgetTicks = 0;

        // Replication format version stamped into update metadata (forward-
        // compatible consumers). Must be >= 1.
        std::uint32_t version = 1;

        // Parses [replication]. Missing section => defaults; present-but-invalid
        // => InvalidArgument. Enforces the cross-field minimums documented above.
        [[nodiscard]] static core::Expected<ReplicationConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::replication
