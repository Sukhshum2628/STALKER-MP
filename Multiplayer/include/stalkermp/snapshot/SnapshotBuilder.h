// STALKER-MP — Snapshot builder (Sprint-008, Step 6)
//
// The engine-free, sim-thread-only pass that captures a per-tick immutable
// SimulationSnapshot into a pooled buffer: BeginBuild (acquire buffer + stamp
// metadata) -> Capture (append entities via IEntitySnapshotSource, players via
// the Sprint-007 read-only PlayerManager surface, environment via the Sprint-002
// IEnvironmentSource) -> Finalize (seal, immutable). VALUES ONLY — no live object
// or raw pointer enters a snapshot. Deterministic: monotonic SnapshotId, ascending
// EntityId/PlayerId, tick-derived content (no wall-clock). PoolExhausted and
// Overflow are value outcomes; on a failed Capture the pooled buffer is returned
// to the pool (no orphan).
//
// This step introduces the builder ONLY — no queue, manager, engine adapter,
// Bootstrap wiring, or diagnostics.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI; Expected<T>.

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"
#include "stalkermp/player/PlayerManager.h"          // Sprint-007 read-only player surface
#include "stalkermp/snapshot/SimulationSnapshot.h"
#include "stalkermp/snapshot/SnapshotConfiguration.h"
#include "stalkermp/snapshot/SnapshotPool.h"
#include "stalkermp/snapshot/SnapshotTypes.h"
#include "stalkermp/world/IEntitySnapshotSource.h"
#include "stalkermp/world/IEnvironmentSource.h"      // Sprint-002 environment source

namespace stalkermp::snapshot
{
    class SnapshotBuilder
    {
    public:
        explicit SnapshotBuilder(const SnapshotConfiguration& config) : m_config(config) {}

        // Acquire a pooled buffer and enter Building with a monotonic SnapshotId
        // for `tick`. PoolExhausted (from the pool) is returned as a value outcome.
        [[nodiscard]] core::Expected<SimulationSnapshot*> BeginBuild(SnapshotPool& pool, std::uint64_t tick);

        // Capture entities/players/environment into the in-progress buffer (values
        // only, ascending). Over the configured caps => Overflow; on any failure the
        // buffer is returned to the pool and the build is aborted (no orphan).
        [[nodiscard]] core::Expected<void> Capture(const world::IEntitySnapshotSource& entitySource,
                                                   const player::PlayerManager& playerSurface,
                                                   const world::IEnvironmentSource& environmentSource);

        // Seal the buffer (Building -> Finalized) and return the immutable view.
        [[nodiscard]] core::Expected<const SimulationSnapshot*> Finalize();

        [[nodiscard]] bool Building() const noexcept { return m_current != nullptr; }
        [[nodiscard]] SnapshotId LastId() const noexcept { return m_lastId; }

    private:
        void AbortToPool();

        SnapshotConfiguration m_config;
        SnapshotPool* m_pool = nullptr;       // non-owning; set at BeginBuild
        SimulationSnapshot* m_current = nullptr; // non-owning pooled buffer being built
        std::uint64_t m_nextId = 1;           // monotonic SnapshotId generator (never 0)
        SnapshotId m_lastId{};
    };
} // namespace stalkermp::snapshot
