// STALKER-MP — Persistence validation framework (Sprint-011, Step 6)
//
// See ValidationFramework.h. Pure, deterministic value-outcome validators.
// Engine-free / OS-free; no exceptions, no RTTI, no iostream.

#include "stalkermp/persistence/ValidationFramework.h"

namespace stalkermp::persistence
{
    PersistenceOutcome ValidationFramework::ValidateIntegrity(const PersistenceSnapshot& snapshot) noexcept
    {
        return snapshot.IntegrityOk() ? PersistenceOutcome::Ok : PersistenceOutcome::IntegrityFailure;
    }

    PersistenceOutcome ValidationFramework::ValidateCompleteness(const PersistenceSnapshot& snapshot) noexcept
    {
        return snapshot.IsComplete() ? PersistenceOutcome::Ok : PersistenceOutcome::IncompleteSnapshot;
    }

    PersistenceOutcome ValidationFramework::ValidateVersion(const VersionManager& versions,
                                                            const VersionSet& candidate) noexcept
    {
        // Equal and MigrationRequired are both reconcilable; only an Incompatible
        // compatibility boundary is a mismatch.
        return versions.IsCompatible(candidate) ? PersistenceOutcome::Ok : PersistenceOutcome::VersionMismatch;
    }

    PersistenceOutcome ValidationFramework::ValidateQueueOrdering(const SaveRequest& previous,
                                                                  const SaveRequest& next) noexcept
    {
        if (previous.id == 0)
        {
            return PersistenceOutcome::Ok; // no predecessor
        }
        if (next.id > previous.id && next.requestTick >= previous.requestTick)
        {
            return PersistenceOutcome::Ok;
        }
        return PersistenceOutcome::IntegrityFailure; // ordering violated
    }
} // namespace stalkermp::persistence
