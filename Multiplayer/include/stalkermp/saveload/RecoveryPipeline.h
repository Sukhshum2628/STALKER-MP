// STALKER-MP — Recovery pipeline (Sprint-012, Step 14)
//
// The engine-free startup orchestration that recovers the authoritative world from a
// save: Load (ISaveSource + SaveReader) → Validate (Step-6 integrity) → Migrate
// (Step-7) → Restore World/Environment → Entities → Players → ALife → Scheduler
// (Steps 11-13) through the Step-10 restore-sink seams, returning a RecoveryReport.
// It composes ONLY previously implemented engine-free components and NEVER crosses
// the engine boundary — the sinks (fake/recording in tests; the real EngineAdapters
// implementations in Step 17) perform the sanctioned authoritative writes (ADR-008).
//
// Recovery isolation (E-G5-SL): any failure during Load/Validate/Migrate returns the
// specific value outcome with NOTHING applied; a failure DURING restoration returns
// PartialRecovery with the reached phase recorded. Deterministic; host-side; runs
// before networking (enforced by the composition root, Step 17).
//
// This step introduces the pipeline (and SaveManager) ONLY — no engine adapters,
// diagnostics collector (Step 15), hardening (Step 16), integration, or closure.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; value
// outcomes (SaveLoadOutcome).

#pragma once

#include <cstdint>

#include "stalkermp/persistence/VersionManager.h" // persistence::VersionManager (reused)
#include "stalkermp/saveload/IRestoreSinks.h"      // restore-sink seams (Step 10)
#include "stalkermp/saveload/ISaveSource.h"        // saveload::ISaveSource (Step 8)
#include "stalkermp/saveload/SaveLoadTypes.h"      // SaveLoadOutcome, RestorePhase, LoadState
#include "stalkermp/saveload/SaveMigrator.h"       // SaveMigrator (Step 7)

namespace stalkermp::saveload
{
    // The set of restore-sink seams a recovery applies through (references borrowed
    // for the call).
    struct RestoreSinkSet
    {
        IWorldRestoreSink& world;
        IEnvironmentRestoreSink& environment;
        IEntityRestoreSink& entity;
        IPlayerRestoreSink& player;
        IAlifeRestoreSink& alife;
        ISchedulerRestoreSink& scheduler;
    };

    // The value result of a recovery attempt.
    struct RecoveryReport
    {
        SaveLoadOutcome outcome = SaveLoadOutcome::NothingToLoad;
        LoadState finalState = LoadState::Idle;
        RestorePhase reachedPhase = RestorePhase::World;
        std::uint64_t saveId = 0;
        std::uint64_t entitiesRestored = 0;
        std::uint64_t playersRestored = 0;
    };

    class RecoveryPipeline
    {
    public:
        // `targetVersion` is the current build's save version (migration target).
        RecoveryPipeline(ISaveSource& source, const persistence::VersionManager& build, SaveMigrator& migrator,
                         std::uint32_t targetVersion) noexcept
            : m_source(source), m_build(build), m_migrator(migrator), m_targetVersion(targetVersion)
        {
        }

        // Recover the save `saveId` through the given sinks. Deterministic.
        [[nodiscard]] RecoveryReport Recover(std::uint64_t saveId, const RestoreSinkSet& sinks);

    private:
        ISaveSource& m_source;
        persistence::VersionManager m_build;
        SaveMigrator& m_migrator;
        std::uint32_t m_targetVersion;
    };
} // namespace stalkermp::saveload
