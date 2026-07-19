// STALKER-MP — Save manager (Sprint-012, Step 14)
//
// Engine-free save coordination. It creates, overwrites, deletes, enumerates, and
// validates saves by REUSING the frozen Sprint-011 write seam (IPersistenceStore,
// via the Step-9 ISaveBackend) and the Step-8 read seam (ISaveSource) — it
// reimplements no serialization, storage, validation, or versioning. A save is
// written by composing the Step-4 SaveWriter inside the backend; validation reuses
// the Step-6 integrity validators. It never crosses the engine boundary.
//
// This step introduces the save manager (and the recovery pipeline) ONLY — no
// engine adapters, diagnostics collector (Step 15), hardening (Step 16), or wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/adapters/PlatformSaveStore.h"        // adapters::ISaveBackend (Store/Source/Delete)
#include "stalkermp/persistence/PersistenceTypes.h"      // persistence::SaveMetadata (reused)
#include "stalkermp/persistence/VersionManager.h"        // persistence::VersionManager (reused)
#include "stalkermp/saveload/SaveLoadTypes.h"            // SaveLoadOutcome, SaveDescriptor
#include "stalkermp/snapshot/ISnapshotView.h"            // snapshot::ISnapshotView

namespace stalkermp::saveload
{
    class SaveManager
    {
    public:
        // Borrows the backend (write store + read source) and the build's version set.
        SaveManager(adapters::ISaveBackend& backend, const persistence::VersionManager& build) noexcept
            : m_backend(backend), m_build(build)
        {
        }

        // Serialize + persist `view` under `metadata` (Begin -> Write -> Commit; an
        // existing save with the same id is atomically replaced). Value outcome.
        [[nodiscard]] SaveLoadOutcome CreateSave(const snapshot::ISnapshotView& view,
                                                 const persistence::SaveMetadata& metadata);

        // Overwrite an existing save (same semantics as CreateSave — the store
        // replaces by saveId).
        [[nodiscard]] SaveLoadOutcome OverwriteSave(const snapshot::ISnapshotView& view,
                                                    const persistence::SaveMetadata& metadata)
        {
            return CreateSave(view, metadata);
        }

        // Delete a save by id.
        [[nodiscard]] SaveLoadOutcome DeleteSave(std::uint64_t saveId) { return m_backend.Delete(saveId); }

        // All available saves, ascending by saveId.
        [[nodiscard]] std::vector<SaveDescriptor> EnumerateSaves() const { return m_backend.Source().Enumerate(); }

        // Read + parse + integrity-validate a save (structural + counts/registry/
        // references/version). NothingToLoad when absent.
        [[nodiscard]] SaveLoadOutcome ValidateSave(std::uint64_t saveId) const;

    private:
        adapters::ISaveBackend& m_backend;
        persistence::VersionManager m_build;
    };
} // namespace stalkermp::saveload
