// STALKER-MP — Client prediction seam factories (Sprint-010, Step 15)
//
// Engine-free / OS-free header declaring the factories for the two engine-touching
// client-presentation seams:
//
//   * ILocalInputSource  — reads the local actor's input each tick.
//   * IPresentationSink   — applies the predicted local + interpolated remote
//                           CLIENT-VISUAL transforms (render only; never
//                           authoritative ALife/simulation state — ADR-008).
//
// The engine implementations (adapters::EngineLocalInputSource /
// EnginePresentationSink) live in EngineAdapters.cpp — the ONLY engine-header TU
// (One Engine Boundary). The test build links tests/support/NullPredictionSeams.cpp
// (inert, deterministic counterparts). No engine type crosses the engine-free
// prediction seams; only values do.

#pragma once

#include <memory>

#include "stalkermp/prediction/ILocalInputSource.h"
#include "stalkermp/prediction/IPresentationSink.h"

namespace stalkermp::adapters
{
    // Reads the local actor's input for the current tick (engine build); the test
    // build returns an inert source that never yields input.
    [[nodiscard]] std::unique_ptr<prediction::ILocalInputSource> CreateEngineLocalInputSource();

    // Applies client-visual presentation transforms for the local + remote entities
    // (engine build); the test build returns an inert sink that observes nothing.
    [[nodiscard]] std::unique_ptr<prediction::IPresentationSink> CreateEnginePresentationSink();
} // namespace stalkermp::adapters
