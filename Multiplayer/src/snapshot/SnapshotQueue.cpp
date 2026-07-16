// STALKER-MP — Snapshot publication queue (Sprint-008, Step 7)
//
// See SnapshotQueue.h. Latest-wins, single-producer / multi-consumer handoff.
// Lifetime is delegated entirely to SnapshotPool's intrusive reference count:
// the queue holds exactly one publication reference on the current snapshot and
// releases it when a newer snapshot supersedes it (retired at count zero).

#include "stalkermp/snapshot/SnapshotQueue.h"

namespace stalkermp::snapshot
{
    core::Expected<void> SnapshotQueue::Publish(const SimulationSnapshot* snapshot)
    {
        if (snapshot == nullptr)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "SnapshotQueue: Publish(nullptr)");
        }
        if (!snapshot->IsFinalized())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "SnapshotQueue: Publish of a non-Finalized snapshot");
        }

        // Latest-wins: the incoming snapshot becomes current and carries forward its
        // existing publication reference (the build reference from the pool). The
        // previously current snapshot's publication reference is released now — it
        // is retired only once its reference count reaches zero, so any consumer
        // that already Acquired it keeps it alive until they Release.
        const SimulationSnapshot* const superseded = m_current;
        m_current = snapshot;
        ++m_published;
        if (superseded != nullptr)
        {
            m_pool->ReturnToPool(superseded);
        }
        return core::Success();
    }

    const SimulationSnapshot* SnapshotQueue::Acquire()
    {
        if (m_current == nullptr)
        {
            return nullptr;
        }
        m_pool->AddRef(m_current); // a new consumer reference
        return m_current;          // immutable view only
    }

    void SnapshotQueue::Release(const SimulationSnapshot* snapshot)
    {
        if (snapshot == nullptr)
        {
            return; // no-op
        }
        m_pool->ReturnToPool(snapshot); // retired by the pool at reference count zero
    }
} // namespace stalkermp::snapshot
