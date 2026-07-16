// STALKER-MP — Snapshot publication queue (Sprint-008, Step 7)
//
// The latest-wins handoff between the single simulation producer and the
// asynchronous, read-only consumers (replication, persistence, replay). The
// producer Publish()es an immutable, Finalized SimulationSnapshot; consumers
// Acquire() the current published snapshot (taking a reference) and Release()
// it when done. Superseded snapshots are retired ONLY after (a) a newer
// snapshot has been published and (b) their reference count has reached zero —
// so a consumer that acquired an older snapshot keeps it alive until it
// releases (Architecture §9/§19/§24, FR-6).
//
// Concurrency contract (documented, ADR-011): SINGLE PRODUCER / MULTI CONSUMER.
// Exactly one thread calls Publish(); any number of threads may Acquire()/
// Release(). The queue itself is single-threaded in this step (no thread is
// created); it is concurrency-ready behind this contract. Lifetime is delegated
// to the SnapshotPool's intrusive reference count — the queue holds exactly one
// "publication reference" on the current snapshot and hands it off on supersede.
//
// Acquire() returns `const SimulationSnapshot*` (immutable ISnapshotView only);
// Release() takes `const` — no mutable state is ever exposed.
//
// This step introduces the queue ONLY — no manager, engine adapter, Bootstrap
// wiring, or diagnostics.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI; value outcomes.

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/core/Expected.h"
#include "stalkermp/snapshot/SimulationSnapshot.h"
#include "stalkermp/snapshot/SnapshotPool.h"

namespace stalkermp::snapshot
{
    class SnapshotQueue
    {
    public:
        // The queue borrows the pool that owns the published buffers; it uses the
        // pool's intrusive reference count for all lifetime decisions.
        explicit SnapshotQueue(SnapshotPool& pool) : m_pool(&pool) {}

        // Publish an immutable (Finalized) snapshot as the new current. The prior
        // current snapshot's publication reference is released (retired once its
        // reference count reaches zero). Rejects null or non-Finalized input as a
        // value outcome (never a throw). Single-producer only.
        [[nodiscard]] core::Expected<void> Publish(const SimulationSnapshot* snapshot);

        // Acquire the current published snapshot, taking a reference. Returns
        // nullptr when nothing has been published yet. Immutable view only.
        [[nodiscard]] const SimulationSnapshot* Acquire();

        // Release a previously acquired snapshot (one reference). Null is a no-op.
        void Release(const SimulationSnapshot* snapshot);

        // Number of snapshots currently held as "current" by the queue (0 or 1;
        // latest-wins). Superseded snapshots still referenced by consumers are
        // owned by the pool, not counted here.
        [[nodiscard]] std::size_t Depth() const noexcept { return m_current != nullptr ? 1u : 0u; }

        // Monotonic count of successful publications (deterministic sequence).
        [[nodiscard]] std::uint64_t Published() const noexcept { return m_published; }

        // The current published snapshot without taking a reference (may be null).
        [[nodiscard]] const SimulationSnapshot* Current() const noexcept { return m_current; }

    private:
        SnapshotPool* m_pool;                        // non-owning; owns the buffers
        const SimulationSnapshot* m_current = nullptr; // current published (queue holds 1 ref)
        std::uint64_t m_published = 0;               // monotonic publication counter
    };
} // namespace stalkermp::snapshot
