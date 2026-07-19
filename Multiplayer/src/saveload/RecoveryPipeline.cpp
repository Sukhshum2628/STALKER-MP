// STALKER-MP — Recovery pipeline (Sprint-012, Step 14)
//
// See RecoveryPipeline.h. Engine-free / OS-free; no exceptions, no RTTI, no
// iostream; value outcomes.

#include "stalkermp/saveload/RecoveryPipeline.h"

#include "stalkermp/saveload/AlifeRestorer.h"
#include "stalkermp/saveload/EntityRestorer.h"
#include "stalkermp/saveload/PlayerRestorer.h"
#include "stalkermp/saveload/SaveIntegrityValidator.h"
#include "stalkermp/saveload/SaveReader.h"
#include "stalkermp/saveload/SchedulerRestorer.h"
#include "stalkermp/saveload/WorldRestorer.h"

namespace stalkermp::saveload
{
    RecoveryReport RecoveryPipeline::Recover(const std::uint64_t saveId, const RestoreSinkSet& sinks)
    {
        RecoveryReport report;

        // 1) Load. No bytes -> NothingToLoad (nothing applied).
        report.finalState = LoadState::Reading;
        auto read = m_source.Read(saveId);
        if (!read.HasValue())
        {
            report.outcome = SaveLoadOutcome::NothingToLoad;
            report.finalState = LoadState::Failed;
            return report;
        }

        // 2) Parse (structural). Malformed -> value outcome (nothing applied).
        ParseResult parsed = SaveReader::Parse(read.Value());
        if (parsed.outcome != SaveLoadOutcome::Ok)
        {
            report.outcome = parsed.outcome;
            report.finalState = LoadState::Failed;
            return report;
        }
        LoadedSave loaded = std::move(parsed.save);
        report.saveId = loaded.metadata.saveId;

        // 3) Validate (integrity). Failure -> value outcome (nothing applied).
        report.finalState = LoadState::Validating;
        if (const SaveLoadOutcome v = SaveIntegrityValidator::Validate(m_build, loaded);
            v != SaveLoadOutcome::Ok)
        {
            report.outcome = v;
            report.finalState = LoadState::Failed;
            return report;
        }

        // 4) Migrate to the current build version. Unsupported -> value outcome
        //    (nothing applied).
        report.finalState = LoadState::Migrating;
        if (const SaveLoadOutcome m =
                m_migrator.Migrate(loaded, SaveMigrator::DetectVersion(loaded), m_targetVersion);
            m != SaveLoadOutcome::Ok)
        {
            report.outcome = m;
            report.finalState = LoadState::Failed;
            return report;
        }

        // 5) Restore (fixed deterministic order). A failure here is a PartialRecovery;
        //    the reached phase is recorded. The sinks own the authoritative writes.
        report.finalState = LoadState::Restoring;

        report.reachedPhase = RestorePhase::World;
        if (WorldRestorer::Restore(loaded, sinks.world, sinks.environment) != SaveLoadOutcome::Ok)
        {
            report.outcome = SaveLoadOutcome::PartialRecovery;
            report.finalState = LoadState::Failed;
            return report;
        }

        report.reachedPhase = RestorePhase::Registry;
        if (EntityRestorer::Restore(loaded, sinks.entity) != SaveLoadOutcome::Ok)
        {
            report.outcome = SaveLoadOutcome::PartialRecovery;
            report.finalState = LoadState::Failed;
            return report;
        }
        report.entitiesRestored = loaded.entities.size();

        report.reachedPhase = RestorePhase::Players;
        if (PlayerRestorer::Restore(loaded, sinks.player) != SaveLoadOutcome::Ok)
        {
            report.outcome = SaveLoadOutcome::PartialRecovery;
            report.finalState = LoadState::Failed;
            return report;
        }
        report.playersRestored = loaded.players.size();

        report.reachedPhase = RestorePhase::ALife;
        if (AlifeRestorer::Restore(loaded, sinks.alife) != SaveLoadOutcome::Ok)
        {
            report.outcome = SaveLoadOutcome::PartialRecovery;
            report.finalState = LoadState::Failed;
            return report;
        }

        report.reachedPhase = RestorePhase::Scheduler;
        if (SchedulerRestorer::Restore(loaded, sinks.scheduler) != SaveLoadOutcome::Ok)
        {
            report.outcome = SaveLoadOutcome::PartialRecovery;
            report.finalState = LoadState::Failed;
            return report;
        }

        report.reachedPhase = RestorePhase::Complete;
        report.finalState = LoadState::Completed;
        report.outcome = SaveLoadOutcome::Ok;
        return report;
    }
} // namespace stalkermp::saveload
