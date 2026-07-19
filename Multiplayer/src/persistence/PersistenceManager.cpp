// STALKER-MP — Persistence manager (Sprint-011, Step 11)
//
// See PersistenceManager.h. One synchronous, deterministic pass per Tick; no
// thread; read-only snapshot consumption. Engine-free / OS-free; no exceptions,
// no RTTI, no iostream; value outcomes.

#include "stalkermp/persistence/PersistenceManager.h"

#include "stalkermp/persistence/PersistenceSnapshot.h" // PersistenceSnapshot
#include "stalkermp/snapshot/SimulationSnapshot.h"     // snapshot::SimulationSnapshot (Acquire result)

namespace stalkermp::persistence
{
    core::Expected<void> PersistenceManager::Initialize()
    {
        m_worker.Start(); // synchronous worker ready to persist (no thread created)
        return core::Success();
    }

    void PersistenceManager::Shutdown() noexcept
    {
        m_worker.Stop();
    }

    PersistenceOutcome PersistenceManager::RequestSave(const SaveTrigger trigger) noexcept
    {
        switch (trigger)
        {
        case SaveTrigger::Manual:         m_scheduler.RequestManual(); return PersistenceOutcome::Ok;
        case SaveTrigger::Administrative: m_scheduler.RequestAdministrative(); return PersistenceOutcome::Ok;
        case SaveTrigger::Shutdown:       m_scheduler.RequestShutdown(); return PersistenceOutcome::Ok;
        case SaveTrigger::Autosave:
        case SaveTrigger::Deferred:
            // Not caller-initiated through RequestSave: autosave is periodic; a
            // deferred save is scheduled via DeferSave(tick).
            return PersistenceOutcome::NothingToPersist;
        }
        return PersistenceOutcome::NothingToPersist;
    }

    void PersistenceManager::Update(const std::uint64_t tick) noexcept
    {
        // 1) Ask the scheduler for at most one due request and enqueue it.
        if (const std::optional<SaveRequest> request = m_scheduler.Tick(tick))
        {
            ++m_scheduledRequests;
            if (request->trigger == SaveTrigger::Autosave)
            {
                ++m_autosaves;
            }
            (void)m_queue.Publish(*request); // QueueFull is a value outcome (overflow counted)
        }

        // 2) Acquire the current authoritative snapshot (read-only). Without one, any
        //    queued requests wait for a later tick.
        const snapshot::SimulationSnapshot* snapshot = m_snapshotSource.Acquire();
        if (snapshot == nullptr)
        {
            return;
        }

        // 3) Project (Preserve Before Replace) and run the synchronous worker once.
        const PersistenceSnapshot projection{*snapshot};
        (void)m_worker.Flush(m_queue, projection, m_versions, m_store);

        // 4) Release the snapshot reference.
        m_snapshotSource.Release(snapshot);
    }

    PersistenceStatistics PersistenceManager::Statistics() const noexcept
    {
        PersistenceStatistics stats{};
        stats.saveRequests = m_scheduledRequests;
        stats.autosaves = m_autosaves;
        stats.completedSaves = m_worker.CompletedCount();
        stats.failedSaves = m_worker.FailedCount();
        stats.retries = m_queue.RetryCount();
        stats.queueOverflows = m_queue.OverflowCount();
        stats.currentQueueDepth = static_cast<std::uint32_t>(m_queue.Size());
        stats.maxQueueDepth = m_queue.MaxDepth();
        return stats;
    }
} // namespace stalkermp::persistence
