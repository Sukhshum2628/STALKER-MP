// STALKER-MP — Entity snapshot capture seam (Sprint-008, Step 5)
//
// An ADDITIVE, SNAPSHOT-SPECIFIC capture seam (frozen Architecture §15
// clarification). It supplements — and does NOT replace, widen, or deprecate —
// the existing Sprint-002/003 read-only entity interfaces (world::IEntitySource,
// IWorldQueries, ISpatialQueries, EntityView) or the Entity Registry, all of
// which remain the canonical entity-access APIs. Its sole purpose is to marshal
// the richer per-entity VALUE data required to construct a snapshot
// (snapshot::EntitySnapshot). It is consumed ONLY by the SnapshotBuilder and MUST
// NOT become a general-purpose entity-access API for other subsystems.
//
// Engine-free / OS-free: the seam names no engine call and exposes no engine type
// (the single concrete engine implementation, adapters::EngineEntitySnapshotSource,
// arrives in Step 9 inside EngineAdapters.cpp). ADR-007: no exceptions.

#pragma once

#include <vector>

#include "stalkermp/snapshot/SnapshotTypes.h" // snapshot::EntitySnapshot

namespace stalkermp::world
{
    class IEntitySnapshotSource
    {
    public:
        virtual ~IEntitySnapshotSource() = default;

        // Append captured entities to `out`, ascending by EntityId. APPEND-ONLY:
        // existing entries in `out` are preserved. Values only — no engine type
        // crosses this seam. Deterministic given the same underlying state.
        virtual void Capture(std::vector<snapshot::EntitySnapshot>& out) const = 0;
    };
} // namespace stalkermp::world
