// STALKER-MP — Snapshot manager (Sprint-008, Step 8)
//
// See SnapshotManager.h. Owns the pool/builder/queue and runs the per-tick
// capture lifecycle once at kReplication = 400. Exactly one publication per
// successful tick; any builder/pool value outcome skips the tick and leaves the
// previously published snapshot valid. Single-threaded, engine-free.

#include "stalkermp/snapshot/SnapshotManager.h"

namespace stalkermp::snapshot
{
    core::Expected<void> SnapshotManager::Initialize()
    {
        // Pre-reserve all pooled buffers once (no steady-state allocation).
        m_pool.Reserve(m_config.poolCapacity);
        return core::Success();
    }

    void SnapshotManager::Shutdown() noexcept
    {
        // The pool/builder/queue are owned members and torn down with this
        // service. Draining the queue back into the pool is the composition
        // root's responsibility (reverse-order teardown, Step 10); nothing to
        // do here in the manager-only step.
    }

    void SnapshotManager::Tick(const double /*deltaSeconds*/)
    {
        ++m_tick; // monotonic, deterministic (tick-derived control flow)

        // BeginBuild acquires a pooled buffer. PoolExhausted => skip this tick;
        // the previously published snapshot remains valid.
        auto begun = m_builder.BeginBuild(m_pool, m_tick);
        if (!begun)
        {
            return;
        }

        // Capture entities/players/environment (values only). Overflow or any
        // capture failure aborts the build (buffer returned to the pool) => skip.
        if (auto captured = m_builder.Capture(m_entitySource, m_playerSurface, m_environmentSource); !captured)
        {
            return;
        }

        // Seal the snapshot.
        auto finalized = m_builder.Finalize();
        if (!finalized)
        {
            return;
        }
        ++m_built;

        // Publish exactly once. On the (defensive) publish failure path the buffer
        // remains pool-owned; the previous published snapshot is unaffected.
        const SimulationSnapshot* const snapshot = finalized.Value();
        if (auto published = m_queue.Publish(snapshot); !published)
        {
            return;
        }
        ++m_publishedCount;
        m_lastMetadata = snapshot->Metadata();
    }

    SnapshotStatistics SnapshotManager::Statistics() const noexcept
    {
        SnapshotStatistics stats{};
        stats.built = m_built;
        stats.published = m_publishedCount;
        stats.retired = 0; // refined in the diagnostics step (value shape fixed here)
        stats.queueDepth = static_cast<std::uint32_t>(m_queue.Depth());
        stats.poolInUse = static_cast<std::uint32_t>(m_pool.InUse());
        stats.poolCapacity = static_cast<std::uint32_t>(m_pool.Capacity());
        stats.lastBuildDurationTicks = m_lastMetadata.buildDurationTicks;
        stats.entityCount = m_lastMetadata.entityCount;
        stats.playerCount = m_lastMetadata.playerCount;
        return stats;
    }

    SnapshotConsistency SnapshotManager::ValidateConsistency() const noexcept
    {
        // The value shape is fixed here; the diagnostics step (Step 12) populates
        // the full audit. The one invariant cheaply checkable now: a published
        // snapshot is always Finalized/immutable.
        SnapshotConsistency report{};
        if (const SimulationSnapshot* const current = m_queue.Current())
        {
            report.immutableWhenPublished = current->IsFinalized();
        }
        return report;
    }
} // namespace stalkermp::snapshot
