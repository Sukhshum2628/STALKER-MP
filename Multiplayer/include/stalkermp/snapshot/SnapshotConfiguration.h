// STALKER-MP — Snapshot configuration (Sprint-008, Step 2)
//
// Parsed from the [snapshot] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument (value outcome). All
// budgets are fixed integers; durations are tick counts (no wall-clock in
// control flow). Engine-free and OS-free. ADR-007: no exceptions, no RTTI, no
// iostream; Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no builder,
// pool, queue, manager, diagnostics, engine seam, or Bootstrap wiring.

#pragma once

#include <cstdint>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::snapshot
{
    struct SnapshotConfiguration
    {
        // Number of pooled snapshot buffers (Step 4 consumes this). Must be >= 2
        // (one publishing + at least one in flight for consumers).
        std::uint32_t poolCapacity = 4;

        // Maximum entities captured per snapshot (bounded memory).
        std::uint32_t maxEntities = 4096;

        // Maximum players captured per snapshot (bounded memory).
        std::uint32_t maxPlayers = 64;

        // Snapshot format version stamped into metadata (forward-compatible
        // consumers). Must be >= 1.
        std::uint32_t version = 1;

        // Maximum published snapshots retained in the queue at once. Must be >= 1
        // and <= poolCapacity (every queued snapshot occupies a pooled buffer).
        std::uint32_t queueDepth = 2;

        // Per-build budget in ticks (diagnostic only; 0 = unbounded/advisory).
        std::uint32_t buildBudgetTicks = 0;

        // Parses [snapshot]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Cross-field: poolCapacity >= 2; 1 <= queueDepth <=
        // poolCapacity; version >= 1. Durations are ticks.
        [[nodiscard]] static core::Expected<SnapshotConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::snapshot
