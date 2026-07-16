// STALKER-MP — Entity snapshot source factory (Sprint-008, Step 5)
//
// Engine-free / OS-free header declaring the factory for the entity snapshot
// capture source. The engine implementation (adapters::EngineEntitySnapshotSource)
// lives in EngineAdapters.cpp — the ONLY engine-header TU (One Engine Boundary),
// delivered in Step 9. The test build links tests/support/NullEntitySnapshotSource.cpp
// (an inert, deterministic counterpart). No engine type crosses the engine-free
// world::IEntitySnapshotSource seam.
//
// Step 5 provides the DECLARATION only; no definition is emitted here.

#pragma once

#include <memory>

#include "stalkermp/world/IEntitySnapshotSource.h"

namespace stalkermp::adapters
{
    [[nodiscard]] std::unique_ptr<world::IEntitySnapshotSource> CreateEngineEntitySnapshotSource();
} // namespace stalkermp::adapters
