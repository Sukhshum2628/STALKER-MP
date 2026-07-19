// STALKER-MP — Persistence snapshot projection (Sprint-011, Step 5)
//
// A read-only, engine-free PROJECTION over an already-captured immutable snapshot
// (Sprint-008 `snapshot::ISnapshotView`). Preserve Before Replace: it introduces
// NO new capture path and copies no simulation state — it merely wraps a reference
// to the acquired snapshot view and exposes the small persistence-facing surface
// that the validation framework (Step 6) and the worker (Step 9) consume.
//
// Lifetime: the projection borrows the view for its own lifetime; it must not
// outlive the acquired snapshot (the manager Acquires/Releases around use).
//
// Read-only over authoritative state (Host Authority; ADR-008): every accessor is
// const and non-mutating. Deterministic; no allocation; no I/O; no clock.
//
// This step introduces the projection ONLY — no validation logic, queue, worker,
// scheduler, or manager.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/snapshot/ISnapshotView.h" // snapshot::ISnapshotView
#include "stalkermp/snapshot/SnapshotTypes.h" // SnapshotState, SnapshotMetadata

namespace stalkermp::persistence
{
    class PersistenceSnapshot
    {
    public:
        // Borrow the acquired snapshot view (no copy, no capture). The referenced
        // view must outlive this projection.
        explicit PersistenceSnapshot(const snapshot::ISnapshotView& view) noexcept : m_view(view) {}

        // The authoritative simulation tick this snapshot captured.
        [[nodiscard]] std::uint64_t Tick() const noexcept { return m_view.Metadata().simulationTick; }

        // Player / entity counts (from the captured containers).
        [[nodiscard]] std::uint32_t PlayerCount() const noexcept
        {
            return static_cast<std::uint32_t>(m_view.Players().size());
        }
        [[nodiscard]] std::uint32_t EntityCount() const noexcept
        {
            return static_cast<std::uint32_t>(m_view.Entities().size());
        }

        // Complete = the snapshot is sealed (immutable) and therefore safe to persist:
        // Finalized or Published. A Building snapshot is not yet sealed; a Retired one
        // has been returned to the pool.
        [[nodiscard]] bool IsComplete() const noexcept
        {
            const snapshot::SnapshotState state = m_view.Metadata().state;
            return state == snapshot::SnapshotState::Finalized || state == snapshot::SnapshotState::Published;
        }

        // Structural integrity: a real (non-none) snapshot id, and the header counts
        // agree with the captured containers. Content validity (checksum) is checked
        // by the validation framework (Step 6).
        [[nodiscard]] bool IntegrityOk() const noexcept
        {
            const snapshot::SnapshotMetadata& meta = m_view.Metadata();
            return !meta.id.IsNone() && meta.entityCount == EntityCount() && meta.playerCount == PlayerCount();
        }

        // The underlying immutable view, for reuse by the metadata builder and the
        // validation framework (read-only).
        [[nodiscard]] const snapshot::ISnapshotView& View() const noexcept { return m_view; }

    private:
        const snapshot::ISnapshotView& m_view; // borrowed; never owned, never mutated
    };
} // namespace stalkermp::persistence
