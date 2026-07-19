// STALKER-MP — Persistence worker (Sprint-011, Step 9)
//
// The SYNCHRONOUS pipeline that turns a validated snapshot into a persisted save.
// D-PF1 / ADR-011: it creates NO thread and uses NO async primitives — every
// method runs entirely on the calling thread. It is "independent of simulation"
// by consuming an IMMUTABLE snapshot, not by concurrency; the design mirrors the
// Sprint-009 `ReplicationWorker` ("designed to run on a future worker thread behind
// the snapshot seam, but exercised synchronously").
//
// Pipeline per save: Consume (validate integrity/completeness/version via the
// Step-06 framework) -> build SaveMetadata (Step-04) -> Persist through the Step-08
// IPersistenceStore seam (Begin -> Write -> Commit). Failure isolation: ANY failure
// Aborts the transaction, so the previous committed save is RETAINED and the World
// is never affected. `Flush` drains the Step-07 queue, processing each currently
// enqueued request once and re-enqueueing a failed one for a later attempt.
//
// Lifecycle: `Start`/`Stop` are synchronous state toggles (NOT thread control); a
// stopped worker returns WorkerUnavailable. Deterministic: identical inputs yield
// identical outcomes and identical store contents.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PersistenceOutcome).

#pragma once

#include <cstdint>

#include "stalkermp/persistence/IPersistenceStore.h"    // IPersistenceStore
#include "stalkermp/persistence/PersistenceQueue.h"      // PersistenceQueue
#include "stalkermp/persistence/PersistenceSnapshot.h"   // PersistenceSnapshot
#include "stalkermp/persistence/PersistenceTypes.h"      // PersistenceOutcome, SaveMetadata, SaveRequest
#include "stalkermp/persistence/VersionManager.h"        // VersionManager

namespace stalkermp::persistence
{
    class PersistenceWorker
    {
    public:
        // Lifecycle toggles — synchronous, NOT thread control. A stopped worker
        // rejects Persist/ProcessRequest/Flush with WorkerUnavailable.
        void Start() noexcept { m_running = true; }
        void Stop() noexcept { m_running = false; }
        [[nodiscard]] bool IsRunning() const noexcept { return m_running; }

        // Validate a snapshot for persistence (integrity, completeness, version). At
        // save time the candidate versions are the current build's, so version
        // validation is trivially satisfied; the full version gate is a load concern
        // (Sprint-012). Returns Ok or the first failing value outcome.
        [[nodiscard]] PersistenceOutcome Consume(const PersistenceSnapshot& snapshot,
                                                 const VersionManager& versions) const noexcept;

        // Persist an already-built metadata + validated snapshot through the store:
        // Begin -> Write -> Commit. Any failure Aborts (retain previous). Returns Ok,
        // StorageUnavailable, IncompleteSnapshot, or WorkerUnavailable.
        [[nodiscard]] PersistenceOutcome Persist(const SaveMetadata& metadata,
                                                 const PersistenceSnapshot& snapshot,
                                                 IPersistenceStore& store) noexcept;

        // Full pipeline for one request: Consume -> build metadata (from the request's
        // trigger/id) -> Persist. Deterministic; failure-isolated.
        [[nodiscard]] PersistenceOutcome ProcessRequest(const SaveRequest& request,
                                                        const PersistenceSnapshot& snapshot,
                                                        const VersionManager& versions,
                                                        IPersistenceStore& store) noexcept;

        // Drain the queue: process each currently-enqueued request once against
        // `snapshot`. A processed request is Released; a failed one is re-enqueued
        // (bounded) for a later Flush (retain-previous holds in the store). Returns
        // the last outcome (NothingToPersist when the queue was empty).
        [[nodiscard]] PersistenceOutcome Flush(PersistenceQueue& queue, const PersistenceSnapshot& snapshot,
                                               const VersionManager& versions, IPersistenceStore& store) noexcept;

        // Read-only worker counters.
        [[nodiscard]] std::uint64_t ProcessedCount() const noexcept { return m_processed; }
        [[nodiscard]] std::uint64_t CompletedCount() const noexcept { return m_completed; }
        [[nodiscard]] std::uint64_t FailedCount() const noexcept { return m_failed; }

    private:
        bool m_running = false;
        std::uint64_t m_processed = 0;
        std::uint64_t m_completed = 0;
        std::uint64_t m_failed = 0;
    };
} // namespace stalkermp::persistence
