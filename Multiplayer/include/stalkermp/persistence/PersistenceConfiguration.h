// STALKER-MP — Persistence configuration (Sprint-011, Step 2)
//
// Parsed from the [persistence] section of server.cfg. Missing section => all
// defaults; present-but-invalid value => InvalidArgument (value outcome). Depths,
// intervals, and backoffs are counts of ticks / entries; no wall-clock appears in
// control flow. Engine-free and OS-free. ADR-007: no exceptions, no RTTI, no
// iostream; Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no queue,
// worker, scheduler, manager, storage, diagnostics, or Bootstrap wiring.
//
// Cross-field: queueDepth >= 1; version >= 1; backpressureLowWatermark <=
// backpressureHighWatermark <= queueDepth. Autosave interval, retries, backoff,
// and the watermarks may be 0.

#pragma once

#include <cstdint>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::persistence
{
    struct PersistenceConfiguration
    {
        // Maximum number of pending save jobs the persistence queue retains. Bounds
        // memory (no unbounded growth); a full queue is a value outcome. Must be >= 1.
        std::uint32_t queueDepth = 16;

        // Periodic autosave interval in simulation ticks. 0 = autosave disabled
        // (only manual/administrative/shutdown/deferred saves occur).
        std::uint32_t autosaveIntervalTicks = 0;

        // Maximum times a failed save is retried before it is abandoned (the previous
        // save is retained). 0 = no retry.
        std::uint32_t maxRetries = 3;

        // Delay in ticks between a failed save and its retry. 0 = retry next pass.
        std::uint32_t retryBackoffTicks = 30;

        // Back-pressure watermarks (queue depth, hysteresis): back-pressure engages at
        // the high watermark and releases at the low watermark. Must satisfy
        // low <= high <= queueDepth. Both may be 0 (back-pressure effectively off).
        std::uint32_t backpressureHighWatermark = 12;
        std::uint32_t backpressureLowWatermark = 4;

        // Persistence format version stamped into metadata/diagnostics
        // (forward-compatible). Must be >= 1.
        std::uint32_t version = 1;

        // Parses [persistence]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Enforces the cross-field rules documented above.
        [[nodiscard]] static core::Expected<PersistenceConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::persistence
