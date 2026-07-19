// STALKER-MP — Save integrity validator (Sprint-012, Step 6)
//
// See SaveIntegrityValidator.h. Pure, deterministic value-outcome validators.
// Engine-free / OS-free; no exceptions, no RTTI, no iostream.

#include "stalkermp/saveload/SaveIntegrityValidator.h"

#include <unordered_set>

#include "stalkermp/persistence/ValidationFramework.h" // persistence::ValidationFramework (reused)

namespace stalkermp::saveload
{
    SaveLoadOutcome SaveIntegrityValidator::ValidateCounts(const LoadedSave& save) noexcept
    {
        if (save.metadata.entityCount != save.entities.size() ||
            save.metadata.playerCount != save.players.size())
        {
            return SaveLoadOutcome::IntegrityFailure;
        }
        return SaveLoadOutcome::Ok;
    }

    SaveLoadOutcome SaveIntegrityValidator::ValidateRegistry(const LoadedSave& save)
    {
        std::uint32_t previous = 0;
        for (std::size_t i = 0; i < save.entities.size(); ++i)
        {
            const std::uint32_t id = save.entities[i].id.value;
            if (id == 0)
            {
                return SaveLoadOutcome::IntegrityFailure; // none id
            }
            if (i > 0 && id <= previous)
            {
                return SaveLoadOutcome::IntegrityFailure; // not strictly ascending -> regress/duplicate
            }
            previous = id;
        }
        return SaveLoadOutcome::Ok;
    }

    SaveLoadOutcome SaveIntegrityValidator::ValidateReferences(const LoadedSave& save)
    {
        std::unordered_set<std::uint32_t> present;
        present.reserve(save.entities.size());
        for (const EntityRestoreRecord& e : save.entities)
        {
            present.insert(e.id.value);
        }

        const auto missing = [&present](std::uint32_t id) { return id != 0 && present.find(id) == present.end(); };

        for (const EntityRestoreRecord& e : save.entities)
        {
            if (missing(e.inventoryRef.value) || missing(e.owner.value))
            {
                return SaveLoadOutcome::MissingData;
            }
        }
        for (const PlayerRestoreRecord& p : save.players)
        {
            if (missing(p.entity.value))
            {
                return SaveLoadOutcome::MissingData;
            }
        }
        return SaveLoadOutcome::Ok;
    }

    SaveLoadOutcome SaveIntegrityValidator::ValidateVersion(const persistence::VersionManager& build,
                                                            const LoadedSave& save) noexcept
    {
        // Build a candidate VersionSet from the save's stored versions: buildVersion is
        // the compatibility boundary, worldVersion is the world version; the schema and
        // persistence dimensions default to the build's (not stored in the format).
        persistence::VersionSet candidate = build.Current();
        candidate.world = save.metadata.worldVersion;
        candidate.compatibility = save.metadata.buildVersion;

        const persistence::PersistenceOutcome o =
            persistence::ValidationFramework::ValidateVersion(build, candidate);
        return o == persistence::PersistenceOutcome::Ok ? SaveLoadOutcome::Ok : SaveLoadOutcome::VersionMismatch;
    }

    SaveLoadOutcome SaveIntegrityValidator::Validate(const persistence::VersionManager& build,
                                                     const LoadedSave& save)
    {
        if (const SaveLoadOutcome o = ValidateCounts(save); o != SaveLoadOutcome::Ok)
        {
            return o;
        }
        if (const SaveLoadOutcome o = ValidateRegistry(save); o != SaveLoadOutcome::Ok)
        {
            return o;
        }
        if (const SaveLoadOutcome o = ValidateReferences(save); o != SaveLoadOutcome::Ok)
        {
            return o;
        }
        return ValidateVersion(build, save);
    }
} // namespace stalkermp::saveload
