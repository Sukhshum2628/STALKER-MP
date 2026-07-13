// STALKER-MP — Snapshot buffer pool (Sprint-008, Step 4)
//
// See SnapshotPool.h. Fixed-capacity, pre-reserved, exception-free intrusive
// ref-count; deterministic (lowest-free-index) acquisition; PoolExhausted value
// outcome. No steady-state heap allocation. Engine-free / OS-free.

#include "stalkermp/snapshot/SnapshotPool.h"

namespace stalkermp::snapshot
{
    void SnapshotPool::Reserve(const std::size_t capacity)
    {
        // One-time storage allocation. Constructing the slots here (default
        // SimulationSnapshot per slot) is the only pool allocation; steady-state
        // Acquire/ReturnToPool never allocate a buffer.
        m_slots.clear();
        m_slots.resize(capacity); // value-initializes Slot{ snapshot, refCount=0, inUse=false }
        m_inUse = 0;
    }

    std::size_t SnapshotPool::IndexOf(const SimulationSnapshot* buffer) const noexcept
    {
        if (buffer == nullptr)
        {
            return m_slots.size();
        }
        for (std::size_t i = 0; i < m_slots.size(); ++i)
        {
            if (&m_slots[i].snapshot == buffer)
            {
                return i;
            }
        }
        return m_slots.size(); // not found
    }

    core::Expected<SimulationSnapshot*> SnapshotPool::Acquire()
    {
        // Deterministic: the lowest-index free slot.
        for (std::size_t i = 0; i < m_slots.size(); ++i)
        {
            if (!m_slots[i].inUse)
            {
                m_slots[i].inUse = true;
                m_slots[i].refCount = 1;
                ++m_inUse;
                return &m_slots[i].snapshot;
            }
        }
        return core::MakeError(core::ErrorCode::InvalidArgument, "SnapshotPool: no free buffer (PoolExhausted)");
    }

    void SnapshotPool::AddRef(const SimulationSnapshot* const buffer) noexcept
    {
        const std::size_t idx = IndexOf(buffer);
        if (idx < m_slots.size() && m_slots[idx].inUse)
        {
            ++m_slots[idx].refCount;
        }
    }

    void SnapshotPool::ReturnToPool(const SimulationSnapshot* const buffer) noexcept
    {
        const std::size_t idx = IndexOf(buffer);
        if (idx >= m_slots.size() || !m_slots[idx].inUse)
        {
            return; // null / unknown / already-free -> benign no-op
        }
        if (m_slots[idx].refCount > 0)
        {
            --m_slots[idx].refCount;
        }
        if (m_slots[idx].refCount == 0)
        {
            m_slots[idx].inUse = false; // returned to the free set for reuse
            --m_inUse;
        }
    }

    std::uint32_t SnapshotPool::RefCount(const SimulationSnapshot* const buffer) const noexcept
    {
        const std::size_t idx = IndexOf(buffer);
        return (idx < m_slots.size()) ? m_slots[idx].refCount : 0u;
    }
} // namespace stalkermp::snapshot
