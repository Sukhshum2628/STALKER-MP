// STALKER-MP — Persistence worker (Sprint-011, Step 9)
//
// See PersistenceWorker.h. Synchronous, no thread, no async primitives; failure
// isolated (Abort => retain previous). Engine-free / OS-free; no exceptions, no
// RTTI, no iostream; value outcomes.

#include "stalkermp/persistence/PersistenceWorker.h"

#include <cstddef>

#include "stalkermp/persistence/SaveMetadataBuilder.h" // SaveMetadataBuilder
#include "stalkermp/persistence/ValidationFramework.h" // ValidationFramework

namespace stalkermp::persistence
{
    PersistenceOutcome PersistenceWorker::Consume(const PersistenceSnapshot& snapshot,
                                                  const VersionManager& versions) const noexcept
    {
        if (const PersistenceOutcome o = ValidationFramework::ValidateIntegrity(snapshot);
            o != PersistenceOutcome::Ok)
        {
            return o;
        }
        if (const PersistenceOutcome o = ValidationFramework::ValidateCompleteness(snapshot);
            o != PersistenceOutcome::Ok)
        {
            return o;
        }
        // Save-time version validation: the candidate versions are the current build's,
        // so this is trivially compatible. The full version gate is a load concern
        // (Sprint-012); wiring it here keeps the check in one place.
        return ValidationFramework::ValidateVersion(versions, versions.Current());
    }

    PersistenceOutcome PersistenceWorker::Persist(const SaveMetadata& metadata,
                                                  const PersistenceSnapshot& snapshot,
                                                  IPersistenceStore& store) noexcept
    {
        if (!m_running)
        {
            return PersistenceOutcome::WorkerUnavailable;
        }

        auto begin = store.Begin(metadata);
        if (!begin.HasValue())
        {
            return PersistenceOutcome::StorageUnavailable; // backend could not open a transaction
        }
        const StoreHandle handle = begin.Value();

        if (const PersistenceOutcome w = store.Write(handle, snapshot); w != PersistenceOutcome::Ok)
        {
            store.Abort(handle); // retain previous
            return w;
        }
        if (const PersistenceOutcome c = store.Commit(handle); c != PersistenceOutcome::Ok)
        {
            store.Abort(handle); // retain previous
            return c;
        }
        return PersistenceOutcome::Ok;
    }

    PersistenceOutcome PersistenceWorker::ProcessRequest(const SaveRequest& request,
                                                         const PersistenceSnapshot& snapshot,
                                                         const VersionManager& versions,
                                                         IPersistenceStore& store) noexcept
    {
        if (!m_running)
        {
            return PersistenceOutcome::WorkerUnavailable;
        }
        if (const PersistenceOutcome v = Consume(snapshot, versions); v != PersistenceOutcome::Ok)
        {
            return v; // validation rejection (integrity/completeness/version)
        }
        const SaveMetadata metadata = SaveMetadataBuilder::Build(snapshot.View(), request.trigger, request.id);
        return Persist(metadata, snapshot, store);
    }

    PersistenceOutcome PersistenceWorker::Flush(PersistenceQueue& queue, const PersistenceSnapshot& snapshot,
                                                const VersionManager& versions, IPersistenceStore& store) noexcept
    {
        if (!m_running)
        {
            return PersistenceOutcome::WorkerUnavailable;
        }

        // Process each request that is enqueued NOW exactly once. Re-enqueued failures
        // are attempted on a later Flush (they are not reprocessed within this drain).
        const std::size_t pending = queue.Size();
        if (pending == 0)
        {
            return PersistenceOutcome::NothingToPersist;
        }

        PersistenceOutcome last = PersistenceOutcome::NothingToPersist;
        for (std::size_t i = 0; i < pending; ++i)
        {
            const SaveRequest* front = queue.Acquire();
            if (front == nullptr)
            {
                break;
            }
            const SaveRequest request = *front; // copy before releasing the slot
            queue.Release();

            last = ProcessRequest(request, snapshot, versions, store);
            ++m_processed;
            if (last == PersistenceOutcome::Ok)
            {
                ++m_completed;
            }
            else
            {
                ++m_failed;
                (void)queue.Retry(request); // re-enqueue for a later attempt (bounded)
            }
        }
        return last;
    }
} // namespace stalkermp::persistence
