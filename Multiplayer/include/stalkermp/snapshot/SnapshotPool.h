// STALKER-MP — Snapshot buffer pool (Sprint-008, Step 4)
//
// A fixed-capacity pool of reusable SimulationSnapshot buffers with an exception-
// free intrusive reference count per buffer (Architecture §9/§19/§24, FR-6). All
// storage is pre-reserved once at Reserve(capacity); Acquire/ReturnToPool/AddRef
// perform NO heap allocation in steady state (they flip flags and adjust counts).
// Buffer acquisition is deterministic: the lowest free slot is returned. Capacity
// exhaustion is a value outcome (PoolExhausted), never a throw or a blocking wait
// (ADR-007).
//
// This step introduces the pool ONLY — no builder, queue, manager, engine seam,
// Bootstrap wiring, or diagnostics.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/snapshot/SimulationSnapshot.h"

namespace stalkermp::snapshot
{
    // Named outcome for pool acquisition (value outcome; ADR-007).
    enum class PoolOutcome : std::uint8_t
    {
        Ok,
        PoolExhausted, // no free buffer available
    };

    [[nodiscard]] constexpr const char* PoolOutcomeName(const PoolOutcome o) noexcept
    {
        switch (o)
        {
        case PoolOutcome::Ok:            return "Ok";
        case PoolOutcome::PoolExhausted: return "PoolExhausted";
        }
        return "Unknown";
    }

    class SnapshotPool
    {
    public:
        SnapshotPool() = default;

        // Pre-allocate all buffers ONCE. Any prior (unused) storage is replaced.
        // After this, the buffer set never grows — no steady-state allocation.
        void Reserve(std::size_t capacity);

        // Acquire the lowest-index free buffer with reference count 1. Returns
        // PoolExhausted (value outcome) when none is free. Never allocates a buffer.
        [[nodiscard]] core::Expected<SimulationSnapshot*> Acquire();

        // Intrusive reference count: add a reference (a new consumer).
        void AddRef(const SimulationSnapshot* buffer) noexcept;

        // Release one reference; when the count reaches zero the buffer is returned
        // to the free set for reuse. A null/unknown/already-free buffer is a no-op.
        void ReturnToPool(const SimulationSnapshot* buffer) noexcept;

        [[nodiscard]] std::size_t InUse() const noexcept { return m_inUse; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_slots.size(); }
        [[nodiscard]] bool Full() const noexcept { return m_inUse >= m_slots.size(); }

        // Current reference count of a buffer (0 if free/unknown) — for the queue
        // (Step 7) lifetime logic and verification.
        [[nodiscard]] std::uint32_t RefCount(const SimulationSnapshot* buffer) const noexcept;

    private:
        struct Slot
        {
            SimulationSnapshot snapshot;
            std::uint32_t refCount = 0;
            bool inUse = false;
        };

        [[nodiscard]] std::size_t IndexOf(const SimulationSnapshot* buffer) const noexcept;

        std::vector<Slot> m_slots; // fixed after Reserve; never resized in steady state
        std::size_t m_inUse = 0;
    };
} // namespace stalkermp::snapshot
