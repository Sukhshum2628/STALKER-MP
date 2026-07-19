// STALKER-MP — Save integrity validator (Sprint-012, Step 6)
//
// Pure, deterministic validators over a parsed LoadedSave (Step 5). They classify a
// save into a SaveLoadOutcome value outcome and never mutate state (ADR-007). This
// is the DEEPER integrity layer above the Step-5 structural checks (magic / framing
// / trailer checksum): header/record count agreement, registry integrity (unique,
// ascending, non-none EntityIds), dangling references, and version compatibility —
// the last REUSING the Sprint-011 VersionManager / ValidationFramework unchanged.
//
// This step introduces the validators ONLY — no migration (Step 7), no save source
// (Step 8), no restoration, filesystem, or engine access.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include "stalkermp/persistence/VersionManager.h" // persistence::VersionManager / VersionSet (reused)
#include "stalkermp/saveload/SaveLoadTypes.h"      // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"         // LoadedSave

namespace stalkermp::saveload
{
    class SaveIntegrityValidator
    {
    public:
        // Metadata counts agree with the parsed record containers.
        [[nodiscard]] static SaveLoadOutcome ValidateCounts(const LoadedSave& save) noexcept;

        // Entities are ascending, unique, and non-none (0). A regress/duplicate/none
        // id => IntegrityFailure.
        [[nodiscard]] static SaveLoadOutcome ValidateRegistry(const LoadedSave& save);

        // Every non-none reference (entity inventoryRef/owner, player entity) resolves
        // to a present entity. A dangling reference => MissingData.
        [[nodiscard]] static SaveLoadOutcome ValidateReferences(const LoadedSave& save);

        // The save's versions are reconcilable with the build (Equal / migration).
        // Incompatible => VersionMismatch. Reuses Sprint-011 ValidationFramework.
        [[nodiscard]] static SaveLoadOutcome ValidateVersion(const persistence::VersionManager& build,
                                                             const LoadedSave& save) noexcept;

        // Run every validator in order; return the first failure, else Ok.
        [[nodiscard]] static SaveLoadOutcome Validate(const persistence::VersionManager& build,
                                                      const LoadedSave& save);
    };
} // namespace stalkermp::saveload
