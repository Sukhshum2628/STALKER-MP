// STALKER-MP — Persistence framework value types (Sprint-011, Step 1)
//
// The engine-free, OS-free foundational vocabulary of the host-side PERSISTENCE
// framework: the save request, the save metadata, and the statistics/consistency
// value shapes that every later Sprint-011 step builds on. Types ONLY — no logic,
// no queue, no worker, no scheduler, no manager, no thread, no storage.
//
// Persistence is a READ-ONLY consumer of the authoritative simulation snapshot
// (Host Authority; ADR-008): these value captures hold only plain scalars derived
// from an immutable snapshot — never a live object, reference, or engine handle.
// Deterministic layout, `0 = none/neutral`, wall-clock excluded from any decision
// or checksum (diagnostic only). No serialization, no save-file format, and no OS
// access are introduced here or anywhere in Sprint-011 (those are Sprint-012).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

namespace stalkermp::persistence
{
    // ------------------------------------------------------------- Enumerations
    // Fixed std::uint8_t underlying type (deterministic ABI); reserved 0 carries
    // the neutral/safe meaning; new enumerators are appended, never renumbered.

    // Named value outcome for the (later) fallible persistence operations (ADR-007).
    enum class PersistenceOutcome : std::uint8_t
    {
        Ok = 0,
        QueueFull,          // a bounded persistence queue is full (back-pressure)
        VersionMismatch,    // a version compatibility check failed
        IntegrityFailure,   // a snapshot failed an integrity check
        IncompleteSnapshot, // a snapshot was not complete enough to persist
        WorkerUnavailable,  // the persistence worker is not started
        StorageUnavailable, // the storage backend could not accept the save
        NothingToPersist,   // no save was scheduled or pending this pass
    };

    [[nodiscard]] constexpr const char* PersistenceOutcomeName(const PersistenceOutcome o) noexcept
    {
        switch (o)
        {
        case PersistenceOutcome::Ok:                 return "Ok";
        case PersistenceOutcome::QueueFull:          return "QueueFull";
        case PersistenceOutcome::VersionMismatch:    return "VersionMismatch";
        case PersistenceOutcome::IntegrityFailure:   return "IntegrityFailure";
        case PersistenceOutcome::IncompleteSnapshot: return "IncompleteSnapshot";
        case PersistenceOutcome::WorkerUnavailable:  return "WorkerUnavailable";
        case PersistenceOutcome::StorageUnavailable: return "StorageUnavailable";
        case PersistenceOutcome::NothingToPersist:   return "NothingToPersist";
        }
        return "Unknown";
    }

    // What prompted a save. Reserved 0 is a manual (user/administrator) request.
    enum class SaveTrigger : std::uint8_t
    {
        Manual = 0,     // explicit manual save
        Autosave,       // periodic scheduled autosave
        Administrative, // administrative/operator-initiated save
        Shutdown,       // save issued during graceful shutdown
        Deferred,       // a save deferred to a future tick
    };

    [[nodiscard]] constexpr const char* SaveTriggerName(const SaveTrigger t) noexcept
    {
        switch (t)
        {
        case SaveTrigger::Manual:         return "Manual";
        case SaveTrigger::Autosave:       return "Autosave";
        case SaveTrigger::Administrative: return "Administrative";
        case SaveTrigger::Shutdown:       return "Shutdown";
        case SaveTrigger::Deferred:       return "Deferred";
        }
        return "Unknown";
    }

    // Lifecycle state of a save as it moves through the framework.
    enum class SaveState : std::uint8_t
    {
        Pending = 0, // queued, not yet processed
        Validating,  // snapshot/version validation in progress
        Persisting,  // handed to the storage backend
        Completed,   // committed successfully
        Failed,      // aborted; the previous save is retained
    };

    [[nodiscard]] constexpr const char* SaveStateName(const SaveState s) noexcept
    {
        switch (s)
        {
        case SaveState::Pending:    return "Pending";
        case SaveState::Validating: return "Validating";
        case SaveState::Persisting: return "Persisting";
        case SaveState::Completed:  return "Completed";
        case SaveState::Failed:     return "Failed";
        }
        return "Unknown";
    }

    // ---------------------------------------------------------- Value captures
    // Values only — never a live object, reference, or handle into simulation.

    // One request to persist the current authoritative state. `id` is monotonic per
    // host session; `requestTick` is the simulation tick at which it was raised.
    struct SaveRequest
    {
        std::uint64_t id = 0;                       // monotonic save request id (0 = none)
        SaveTrigger trigger = SaveTrigger::Manual;  // what prompted the save
        std::uint64_t requestTick = 0;              // simulation tick the request was raised at
    };

    // Metadata describing a save, derived deterministically from an immutable
    // snapshot. Exists independently of any serialization or save-file format. The
    // `checksum` is a deterministic digest over snapshot-derived values (never over
    // the wall clock). `timestampWallClock` is DIAGNOSTIC ONLY — it never
    // participates in a decision, the checksum, or replay identity.
    struct SaveMetadata
    {
        std::uint64_t saveId = 0;                // 0 = none
        std::uint64_t simulationTick = 0;        // authoritative tick captured
        std::uint32_t playerCount = 0;
        std::uint32_t entityCount = 0;
        std::uint32_t worldVersion = 0;          // world/content version
        std::uint32_t buildVersion = 0;          // module build version
        std::uint64_t checksum = 0;              // deterministic digest (snapshot-derived)
        std::uint64_t timestampWallClock = 0;    // DIAGNOSTIC ONLY — not replay identity
    };

    // Read-only aggregate tallies (populated by the diagnostics collector later).
    struct PersistenceStatistics
    {
        std::uint64_t saveRequests = 0;
        std::uint64_t autosaves = 0;
        std::uint64_t completedSaves = 0;
        std::uint64_t failedSaves = 0;
        std::uint64_t retries = 0;
        std::uint64_t queueOverflows = 0;
        std::uint32_t currentQueueDepth = 0;
        std::uint32_t maxQueueDepth = 0;
        std::uint64_t lastSaveDurationMicros = 0; // DIAGNOSTIC ONLY (timing)
        std::uint64_t maxSaveDurationMicros = 0;  // DIAGNOSTIC ONLY (timing)
        std::uint64_t timestampWallClock = 0;     // DIAGNOSTIC ONLY — not replay identity
    };

    // Read-only consistency report; value shape fixed here, populated by later
    // steps/diagnostics. Healthy by default.
    struct PersistenceConsistency
    {
        bool readOnly = true;         // persistence never mutates authoritative state
        bool boundedQueue = true;     // the persistence queue is bounded (no unbounded growth)
        bool versionTracked = true;   // save versions are tracked and compared
        bool failureIsolated = true;  // a save failure never corrupts the simulation
        bool snapshotImmutable = true;// only immutable snapshots are consumed

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return readOnly && boundedQueue && versionTracked && failureIsolated && snapshotImmutable;
        }
    };
} // namespace stalkermp::persistence
