// STALKER-MP — Save migration framework (Sprint-012, Step 7)
//
// Deterministic, engine-free version detection + migration pipeline. A migration is
// a pure record→record transform of a LoadedSave that advances it from one save
// version to the next; `Migrate` walks the registered chain from the detected
// version to the target. No migration PATHS exist for the first save format version
// — this step delivers the INFRASTRUCTURE (detection, ordered pipeline, unsupported
// detection, applied-step logging), which Sprint-012+ populate as the format
// evolves.
//
// No filesystem, engine, or platform integration. Uses the Sprint-011 VersionManager
// value shapes conceptually; the migrator operates on stored save versions only.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/saveload/SaveLoadTypes.h" // SaveLoadOutcome
#include "stalkermp/saveload/SaveReader.h"    // LoadedSave

namespace stalkermp::saveload
{
    class SaveMigrator
    {
    public:
        // A pure migration step function: transforms `save` in place, returning Ok or
        // a value outcome. Function pointer only (no RTTI, no captured state).
        using StepFn = SaveLoadOutcome (*)(LoadedSave& save);

        // The save version encoded in the artifact (the metadata build version).
        [[nodiscard]] static std::uint32_t DetectVersion(const LoadedSave& save) noexcept
        {
            return save.metadata.buildVersion;
        }

        // Register a migration step `from` -> `to` (must satisfy to > from). Steps are
        // matched by their `from` version during Migrate.
        void Register(std::uint32_t from, std::uint32_t to, StepFn apply);

        // Whether a supported chain exists from `fromVersion` to `toVersion` (a dry
        // run; applies nothing).
        [[nodiscard]] bool CanMigrate(std::uint32_t fromVersion, std::uint32_t toVersion) const noexcept;

        // Migrate `save` from `fromVersion` to `toVersion`. from == to is a no-op Ok.
        // A missing/backward chain => VersionUnsupported; a step's own failure is
        // propagated. Deterministic: identical input + registry => identical result.
        [[nodiscard]] SaveLoadOutcome Migrate(LoadedSave& save, std::uint32_t fromVersion,
                                              std::uint32_t toVersion);

        // Number of steps applied by the most recent Migrate (diagnostic).
        [[nodiscard]] std::uint32_t AppliedSteps() const noexcept { return m_appliedSteps; }

    private:
        struct Step
        {
            std::uint32_t from = 0;
            std::uint32_t to = 0;
            StepFn apply = nullptr;
        };

        [[nodiscard]] const Step* FindStep(std::uint32_t from) const noexcept;

        std::vector<Step> m_steps;
        std::uint32_t m_appliedSteps = 0;
    };
} // namespace stalkermp::saveload
