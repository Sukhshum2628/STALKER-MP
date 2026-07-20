// STALKER-MP — Restore-sink bundle seam (Sprint-012, Step 17)
//
// Composition-root helper: an owner of the six restore-sink implementations that
// exposes them as one RestoreSinkSet for RecoveryPipeline::Recover. This lets
// Bootstrap request the whole write boundary from a single factory
// (adapters::CreateEngineRestoreSinks) without naming the concrete sinks — the
// engine build returns a bundle of the real EngineAdapters sinks; the test build
// returns null sinks. Engine-free (no engine header enters the core).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/saveload/RecoveryPipeline.h" // RestoreSinkSet

namespace stalkermp::saveload
{
    // Owns the six restore sinks and exposes them as one RestoreSinkSet. The bundle
    // outlives every Recover() call made against the returned set.
    class IRestoreSinkBundle
    {
    public:
        virtual ~IRestoreSinkBundle() = default;

        // The write boundary as a single value handle (references into *this).
        [[nodiscard]] virtual RestoreSinkSet Sinks() noexcept = 0;
    };
} // namespace stalkermp::saveload
