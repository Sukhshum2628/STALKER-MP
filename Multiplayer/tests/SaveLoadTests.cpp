// STALKER-MP — Save/Load System tests (Sprint-012)
//
// Step 1: value types, enumerations, POD restoration records, the save descriptor,
//         and const char* name helpers. Engine-free and OS-free. Types only — no
//         serialization, loading, restoration, migration, or filesystem.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/saveload/SaveLoadTypes.h"

using namespace stalkermp;

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(SaveLoadTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<saveload::SaveLoadOutcome>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<saveload::RestorePhase>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<saveload::LoadState>, std::uint8_t>);
    SUCCEED();
}

// --- Reserved 0 carries the neutral/safe meaning ------------------------------
TEST(SaveLoadTypesStep1, ReservedZeroIsNeutral)
{
    static_assert(static_cast<std::uint8_t>(saveload::SaveLoadOutcome::Ok) == 0u);
    static_assert(static_cast<std::uint8_t>(saveload::RestorePhase::World) == 0u);
    static_assert(static_cast<std::uint8_t>(saveload::LoadState::Idle) == 0u);
    SUCCEED();
}

// --- POD value captures: trivially copyable ----------------------------------
TEST(SaveLoadTypesStep1, RecordsAreTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable_v<saveload::SaveDescriptor>);
    static_assert(std::is_trivially_copyable_v<saveload::WorldRestoreRecord>);
    static_assert(std::is_trivially_copyable_v<saveload::EnvironmentRestoreRecord>);
    static_assert(std::is_trivially_copyable_v<saveload::EntityRestoreRecord>);
    static_assert(std::is_trivially_copyable_v<saveload::PlayerRestoreRecord>);
    static_assert(std::is_trivially_copyable_v<saveload::AlifeRestoreRecord>);
    static_assert(std::is_trivially_copyable_v<saveload::SchedulerRestoreRecord>);
    static_assert(std::is_trivially_copyable_v<saveload::RecoveryStatistics>);
    static_assert(std::is_trivially_copyable_v<saveload::RecoveryConsistency>);
    SUCCEED();
}

// --- Documented default construction (0 = none) ------------------------------
TEST(SaveLoadTypesStep1, DefaultConstructionZeroed)
{
    const saveload::SaveDescriptor d{};
    EXPECT_EQ(d.saveId, 0u);
    EXPECT_EQ(d.generation, 0u);
    EXPECT_EQ(d.metadata.saveId, 0u);   // reused Sprint-011 SaveMetadata default
    EXPECT_EQ(d.metadata.checksum, 0u);

    const saveload::WorldRestoreRecord w{};
    EXPECT_EQ(w.simulationTick, 0u);
    EXPECT_EQ(w.worldTimeSeconds, 0u);
    EXPECT_EQ(w.weatherId, 0u);
    EXPECT_EQ(w.globalStateFlags, 0u);
    EXPECT_EQ(w.zoneConfigId, 0u);

    const saveload::EnvironmentRestoreRecord e{};
    EXPECT_EQ(e.weatherId, 0u);
    EXPECT_EQ(e.environmentVersion, 0u);

    const saveload::EntityRestoreRecord en{};
    EXPECT_EQ(en.id.value, 0u);
    EXPECT_EQ(en.inventoryRef.value, 0u);
    EXPECT_EQ(en.owner.value, 0u);
    EXPECT_EQ(en.runtimeState, 0u);
    EXPECT_EQ(en.relationship, 0u);
    EXPECT_FLOAT_EQ(en.position.x, 0.0f);
    EXPECT_FLOAT_EQ(en.velocity.z, 0.0f);

    const saveload::PlayerRestoreRecord p{};
    EXPECT_EQ(p.id.value, 0u);
    EXPECT_EQ(p.entity.value, 0u);
    EXPECT_EQ(p.statistics, 0u);
    EXPECT_EQ(p.questProgress, 0u);
    EXPECT_EQ(p.factionId, 0u);
    EXPECT_EQ(p.connectionState, player::PlayerConnectionState::Connected);

    const saveload::AlifeRestoreRecord a{};
    EXPECT_EQ(a.smartTerrainId, 0u);
    EXPECT_EQ(a.offlineObject.value, 0u);
    EXPECT_EQ(a.storyLinkId, 0u);

    const saveload::SchedulerRestoreRecord s{};
    EXPECT_EQ(s.simulationTick, 0u);
    EXPECT_EQ(s.pendingUpdates, 0u);
    EXPECT_EQ(s.queueDepth, 0u);
    EXPECT_EQ(s.deferredTasks, 0u);
    EXPECT_EQ(s.executionOrder, 0u);

    const saveload::RecoveryStatistics rs{};
    EXPECT_EQ(rs.entitiesRestored, 0u);
    EXPECT_EQ(rs.playersRestored, 0u);
    EXPECT_EQ(rs.validationFailures, 0u);
    EXPECT_EQ(rs.recoveryAttempts, 0u);
    EXPECT_EQ(rs.recoverySuccesses, 0u);
    EXPECT_EQ(rs.timestampWallClock, 0u);
}

// --- Value semantics: copies are independent ----------------------------------
TEST(SaveLoadTypesStep1, ValueSemanticsIndependentCopies)
{
    saveload::EntityRestoreRecord a{};
    a.id = world::EntityId{7};
    a.position = world::Vec3{1.0f, 2.0f, 3.0f};
    a.runtimeState = 42u;

    saveload::EntityRestoreRecord b = a; // copy
    b.id = world::EntityId{9};
    b.position.x = 100.0f;

    EXPECT_EQ(a.id.value, 7u);            // original unchanged
    EXPECT_FLOAT_EQ(a.position.x, 1.0f);
    EXPECT_EQ(a.runtimeState, 42u);
    EXPECT_EQ(b.id.value, 9u);
    EXPECT_FLOAT_EQ(b.position.x, 100.0f);
    EXPECT_EQ(b.runtimeState, 42u);      // copied through
}

// --- RecoveryConsistency healthy by default; any false flag => unhealthy -------
TEST(SaveLoadTypesStep1, ConsistencyHealthyByDefault)
{
    saveload::RecoveryConsistency c{};
    EXPECT_TRUE(c.deterministicRestore);
    EXPECT_TRUE(c.restoredBeforeNetworking);
    EXPECT_TRUE(c.authoritativeOnly);
    EXPECT_TRUE(c.versionValidated);
    EXPECT_TRUE(c.integrityValidated);
    EXPECT_TRUE(c.engineWritesSanctioned);
    EXPECT_TRUE(c.IsHealthy());

    c.restoredBeforeNetworking = false;
    EXPECT_FALSE(c.IsHealthy());

    saveload::RecoveryConsistency d{};
    d.engineWritesSanctioned = false;
    EXPECT_FALSE(d.IsHealthy());
}

// --- SaveLoadOutcome names: every enumerator + Unknown fallback ---------------
TEST(SaveLoadTypesStep1, OutcomeNamesTotal)
{
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::Ok), "Ok");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::CorruptedSave), "CorruptedSave");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::VersionUnsupported), "VersionUnsupported");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::VersionMismatch), "VersionMismatch");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::IntegrityFailure), "IntegrityFailure");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::MissingData), "MissingData");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::ChecksumFailure), "ChecksumFailure");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::InterruptedLoad), "InterruptedLoad");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::PartialRecovery), "PartialRecovery");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::StorageUnavailable), "StorageUnavailable");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::NothingToLoad), "NothingToLoad");
    EXPECT_STREQ(saveload::SaveLoadOutcomeName(static_cast<saveload::SaveLoadOutcome>(200)), "Unknown");
}

// --- RestorePhase names: every enumerator + Unknown fallback ------------------
TEST(SaveLoadTypesStep1, RestorePhaseNamesTotal)
{
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::World), "World");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::Environment), "Environment");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::Registry), "Registry");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::Players), "Players");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::ALife), "ALife");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::Scheduler), "Scheduler");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::Story), "Story");
    EXPECT_STREQ(saveload::RestorePhaseName(saveload::RestorePhase::Complete), "Complete");
    EXPECT_STREQ(saveload::RestorePhaseName(static_cast<saveload::RestorePhase>(200)), "Unknown");
}

// --- LoadState names: every enumerator + Unknown fallback --------------------
TEST(SaveLoadTypesStep1, LoadStateNamesTotal)
{
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Idle), "Idle");
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Reading), "Reading");
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Validating), "Validating");
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Migrating), "Migrating");
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Restoring), "Restoring");
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Completed), "Completed");
    EXPECT_STREQ(saveload::LoadStateName(saveload::LoadState::Failed), "Failed");
    EXPECT_STREQ(saveload::LoadStateName(static_cast<saveload::LoadState>(200)), "Unknown");
}

// --- Name helpers are constexpr (compile-time evaluable) ----------------------
TEST(SaveLoadTypesStep1, NameHelpersAreConstexpr)
{
    static_assert(saveload::SaveLoadOutcomeName(saveload::SaveLoadOutcome::Ok)[0] == 'O');
    static_assert(saveload::RestorePhaseName(saveload::RestorePhase::ALife)[0] == 'A');
    static_assert(saveload::LoadStateName(saveload::LoadState::Failed)[0] == 'F');
    SUCCEED();
}
