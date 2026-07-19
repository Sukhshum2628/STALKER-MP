// STALKER-MP — Persistence storage seam (Sprint-011, Step 8)
//
// The engine-free, OS-free storage boundary the persistence worker hands a
// validated snapshot + metadata to. This is the ONE PLATFORM BOUNDARY seam for
// persistence: Sprint-011 defines NO save-file format, NO serialization, and NO OS
// access here — a store transfers ownership of a validated save to a backend. The
// real filesystem backend arrives in Sprint-012 behind this frozen interface; the
// default and test builds link the in-memory / null stores.
//
// Transaction semantics (atomic): Begin -> Write -> Commit persists a save fully,
// or Abort discards it and the previous save is retained. A store never mutates
// simulation state and never blocks it.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (PersistenceOutcome) + Expected<StoreHandle>.

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"                     // core::Expected
#include "stalkermp/persistence/PersistenceSnapshot.h"   // PersistenceSnapshot
#include "stalkermp/persistence/PersistenceTypes.h"      // SaveMetadata, PersistenceOutcome

namespace stalkermp::persistence
{
    // Opaque handle to an in-progress save transaction. Value 0 = invalid.
    struct StoreHandle
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
        [[nodiscard]] constexpr bool operator==(const StoreHandle& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const StoreHandle& o) const noexcept { return value != o.value; }
    };

    class IPersistenceStore
    {
    public:
        virtual ~IPersistenceStore() = default;

        // Open a save transaction for `metadata`. Returns a valid handle, or an error
        // (e.g. IoError) when the backend is unavailable. No previous save is affected
        // until Commit.
        [[nodiscard]] virtual core::Expected<StoreHandle> Begin(const SaveMetadata& metadata) = 0;

        // Transfer the validated snapshot to the open transaction. Value outcome
        // (StorageUnavailable on backend failure or an unknown handle).
        [[nodiscard]] virtual PersistenceOutcome Write(StoreHandle handle,
                                                       const PersistenceSnapshot& snapshot) = 0;

        // Atomically finalize the transaction; on success this save becomes current.
        // Value outcome (StorageUnavailable on failure or an unknown handle).
        [[nodiscard]] virtual PersistenceOutcome Commit(StoreHandle handle) = 0;

        // Discard the transaction; the previous save is retained. Idempotent; benign
        // for an unknown handle. Never throws.
        virtual void Abort(StoreHandle handle) noexcept = 0;
    };
} // namespace stalkermp::persistence
