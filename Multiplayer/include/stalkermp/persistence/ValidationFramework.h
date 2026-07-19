// STALKER-MP — Persistence validation framework (Sprint-011, Step 6)
//
// Pure, deterministic validators that classify a persistence candidate into a
// PersistenceOutcome value outcome. Each validator is read-only and side-effect
// free — a rejection leaves all state unchanged (ADR-007 value outcomes). The
// validators reuse the Step-05 `PersistenceSnapshot` projection and the Step-03
// `VersionManager`; they perform no I/O, allocate nothing, and read no clock.
//
//   ValidateIntegrity    — structural integrity of the snapshot header/containers.
//   ValidateCompleteness — the snapshot is sealed (Finalized/Published).
//   ValidateVersion      — the candidate version set is reconcilable with the build.
//   ValidateQueueOrdering — save requests are strictly ascending (FIFO integrity).
//
// This step introduces the validators ONLY — no queue, storage, worker, scheduler,
// or manager.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/persistence/PersistenceSnapshot.h" // PersistenceSnapshot
#include "stalkermp/persistence/PersistenceTypes.h"    // PersistenceOutcome, SaveRequest
#include "stalkermp/persistence/VersionManager.h"      // VersionManager, VersionSet

namespace stalkermp::persistence
{
    class ValidationFramework
    {
    public:
        // Ok when the snapshot's header is structurally consistent (real id; header
        // counts agree with the containers); otherwise IntegrityFailure.
        [[nodiscard]] static PersistenceOutcome ValidateIntegrity(const PersistenceSnapshot& snapshot) noexcept;

        // Ok when the snapshot is sealed (Finalized/Published) and thus safe to
        // persist; otherwise IncompleteSnapshot.
        [[nodiscard]] static PersistenceOutcome ValidateCompleteness(const PersistenceSnapshot& snapshot) noexcept;

        // Ok when `candidate` is reconcilable with the build (Equal or
        // MigrationRequired); otherwise VersionMismatch (Incompatible boundary).
        [[nodiscard]] static PersistenceOutcome ValidateVersion(const VersionManager& versions,
                                                                const VersionSet& candidate) noexcept;

        // Ok when `next` follows `previous` in strict ascending order (id increasing,
        // tick non-decreasing); otherwise IntegrityFailure. A `previous` with id 0
        // (none) always validates — there is no predecessor.
        [[nodiscard]] static PersistenceOutcome ValidateQueueOrdering(const SaveRequest& previous,
                                                                      const SaveRequest& next) noexcept;
    };
} // namespace stalkermp::persistence
