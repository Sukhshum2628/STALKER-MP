// STALKER-MP — Save/Load System tests (Sprint-012)
//
// Step 1: value types, enumerations, POD restoration records, the save descriptor,
//         and const char* name helpers. Engine-free and OS-free. Types only — no
//         serialization, loading, restoration, migration, or filesystem.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include "stalkermp/core/Config.h"
#include "stalkermp/saveload/SaveByteCursor.h"
#include "stalkermp/saveload/SaveFormat.h"
#include "stalkermp/saveload/SaveLoadConfiguration.h"
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

// ============================================================================
// Step 2 — SaveLoadConfiguration::FromConfig
// ============================================================================

// --- Missing [saveload] section => all documented defaults --------------------
TEST(SaveLoadConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = saveload::SaveLoadConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.saveDirectoryToken, "saves");
    EXPECT_EQ(c.maxGenerations, 8u);
    EXPECT_TRUE(c.loadOnStartup);
    EXPECT_TRUE(c.strictIntegrity);
    EXPECT_TRUE(c.migrationEnabled);
    EXPECT_EQ(c.version, 1u);
}

// --- Each field parses a supplied value (override) ----------------------------
TEST(SaveLoadConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("saveload", "save_directory", "campaign_slot");
    store.Set("saveload", "max_generations", "32");
    store.Set("saveload", "load_on_startup", "false");
    store.Set("saveload", "strict_integrity", "false");
    store.Set("saveload", "migration_enabled", "false");
    store.Set("saveload", "version", "2");
    const auto r = saveload::SaveLoadConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.saveDirectoryToken, "campaign_slot");
    EXPECT_EQ(c.maxGenerations, 32u);
    EXPECT_FALSE(c.loadOnStartup);
    EXPECT_FALSE(c.strictIntegrity);
    EXPECT_FALSE(c.migrationEnabled);
    EXPECT_EQ(c.version, 2u);
}

// --- Boundary minimums accepted (maxGenerations = version = 1) ----------------
TEST(SaveLoadConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("saveload", "max_generations", "1");
    store.Set("saveload", "version", "1");
    const auto r = saveload::SaveLoadConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().maxGenerations, 1u);
    EXPECT_EQ(r.Value().version, 1u);
}

// --- Invalid values rejected (value outcome) ----------------------------------
TEST(SaveLoadConfigStep2, InvalidValuesRejected)
{
    {
        core::ConfigStore s;
        s.Set("saveload", "max_generations", "0"); // below min 1
        EXPECT_FALSE(saveload::SaveLoadConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("saveload", "version", "0"); // below min 1
        EXPECT_FALSE(saveload::SaveLoadConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("saveload", "max_generations", "-3"); // negative
        EXPECT_FALSE(saveload::SaveLoadConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("saveload", "save_directory", ""); // empty token
        EXPECT_FALSE(saveload::SaveLoadConfiguration::FromConfig(s).HasValue());
    }
}

// ============================================================================
// Step 3 — Save format primitives (byte cursor + framing + checksum)
// ============================================================================

// --- Byte writer/reader round-trip for every primitive ------------------------
TEST(SaveFormatStep3, ByteRoundTrip)
{
    saveload::ByteWriter w;
    w.PutU8(0xABu);
    w.PutU16(0x1234u);
    w.PutU32(0x89ABCDEFu);
    w.PutU64(0x0123456789ABCDEFull);
    w.PutF32(3.5f);
    const std::uint8_t blob[3] = {0x10u, 0x20u, 0x30u};
    w.PutBytes(blob, 3);
    EXPECT_EQ(w.Size(), 1u + 2u + 4u + 8u + 4u + 3u);

    saveload::ByteReader r{w.Bytes().data(), w.Bytes().size()};
    std::uint8_t u8 = 0;
    std::uint16_t u16 = 0;
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    float f = 0.0f;
    std::uint8_t out3[3] = {0, 0, 0};
    ASSERT_TRUE(r.GetU8(u8));
    ASSERT_TRUE(r.GetU16(u16));
    ASSERT_TRUE(r.GetU32(u32));
    ASSERT_TRUE(r.GetU64(u64));
    ASSERT_TRUE(r.GetF32(f));
    ASSERT_TRUE(r.GetBytes(out3, 3));
    EXPECT_EQ(u8, 0xABu);
    EXPECT_EQ(u16, 0x1234u);
    EXPECT_EQ(u32, 0x89ABCDEFu);
    EXPECT_EQ(u64, 0x0123456789ABCDEFull);
    EXPECT_FLOAT_EQ(f, 3.5f);
    EXPECT_EQ(out3[0], 0x10u);
    EXPECT_EQ(out3[2], 0x30u);
    EXPECT_TRUE(r.Ok());
    EXPECT_EQ(r.Remaining(), 0u);
}

// --- Little-endian byte order is explicit and platform-independent ------------
TEST(SaveFormatStep3, LittleEndianOrder)
{
    saveload::ByteWriter w;
    w.PutU32(0x01020304u);
    ASSERT_EQ(w.Size(), 4u);
    EXPECT_EQ(w.Bytes()[0], 0x04u);
    EXPECT_EQ(w.Bytes()[1], 0x03u);
    EXPECT_EQ(w.Bytes()[2], 0x02u);
    EXPECT_EQ(w.Bytes()[3], 0x01u);
}

// --- Reader bounds checking: overrun is a value outcome, latched --------------
TEST(SaveFormatStep3, ReaderBoundsChecked)
{
    saveload::ByteWriter w;
    w.PutU16(0x7788u); // only 2 bytes available

    saveload::ByteReader r{w.Bytes().data(), w.Bytes().size()};
    std::uint32_t u32 = 0xDEADBEEFu;
    EXPECT_FALSE(r.GetU32(u32));       // wants 4, only 2 -> fail
    EXPECT_EQ(u32, 0xDEADBEEFu);       // out left unchanged
    EXPECT_FALSE(r.Ok());              // error latched
    std::uint8_t u8 = 0;
    EXPECT_FALSE(r.GetU8(u8));         // subsequent reads keep failing
    EXPECT_EQ(r.Remaining(), 0u);
}

// --- File header: write then validate; bad magic/version rejected -------------
TEST(SaveFormatStep3, HeaderReadWrite)
{
    saveload::ByteWriter w;
    saveload::WriteHeader(w);
    EXPECT_EQ(w.Size(), 8u); // magic(4) + formatVersion(4)

    saveload::ByteReader ok{w.Bytes().data(), w.Bytes().size()};
    EXPECT_EQ(saveload::ReadHeader(ok), saveload::SaveLoadOutcome::Ok);

    // Corrupt magic -> CorruptedSave.
    std::vector<std::uint8_t> badMagic = w.Bytes();
    badMagic[0] ^= 0xFFu;
    saveload::ByteReader bm{badMagic.data(), badMagic.size()};
    EXPECT_EQ(saveload::ReadHeader(bm), saveload::SaveLoadOutcome::CorruptedSave);

    // Wrong format version -> VersionMismatch.
    saveload::ByteWriter vw;
    vw.PutU32(saveload::kSaveMagic);
    vw.PutU32(saveload::kSaveFormatVersion + 1u);
    saveload::ByteReader vr{vw.Bytes().data(), vw.Bytes().size()};
    EXPECT_EQ(saveload::ReadHeader(vr), saveload::SaveLoadOutcome::VersionMismatch);

    // Truncated header -> CorruptedSave.
    saveload::ByteReader tr{w.Bytes().data(), 3};
    EXPECT_EQ(saveload::ReadHeader(tr), saveload::SaveLoadOutcome::CorruptedSave);
}

// --- Section framing: id + length + payload round-trip ------------------------
TEST(SaveFormatStep3, SectionFraming)
{
    const std::uint8_t payload[4] = {0xDEu, 0xADu, 0xBEu, 0xEFu};
    saveload::ByteWriter w;
    saveload::WriteSection(w, saveload::SaveSection::Players, payload, 4);
    EXPECT_EQ(w.Size(), 2u + 4u + 4u); // id(u16) + length(u32) + payload

    saveload::ByteReader r{w.Bytes().data(), w.Bytes().size()};
    saveload::SaveSection id = saveload::SaveSection::Header;
    std::uint32_t length = 0;
    ASSERT_TRUE(saveload::ReadSectionHeader(r, id, length));
    EXPECT_EQ(id, saveload::SaveSection::Players);
    EXPECT_EQ(length, 4u);
    std::uint8_t got[4] = {0, 0, 0, 0};
    ASSERT_TRUE(r.GetBytes(got, length));
    EXPECT_EQ(got[0], 0xDEu);
    EXPECT_EQ(got[3], 0xEFu);
    EXPECT_TRUE(r.Ok());
}

// --- Checksum is deterministic and content-sensitive --------------------------
TEST(SaveFormatStep3, ChecksumDeterministicAndSensitive)
{
    const std::uint8_t a[5] = {1, 2, 3, 4, 5};
    const std::uint8_t b[5] = {1, 2, 3, 4, 5};
    const std::uint8_t c[5] = {1, 2, 3, 4, 6}; // one byte differs

    EXPECT_EQ(saveload::Checksum(a, 5), saveload::Checksum(b, 5)); // deterministic
    EXPECT_NE(saveload::Checksum(a, 5), saveload::Checksum(c, 5)); // content-sensitive
    EXPECT_NE(saveload::Checksum(a, 5), 0u);
    // Empty range is stable.
    EXPECT_EQ(saveload::Checksum(nullptr, 0), saveload::Checksum(a, 0));
}

// --- SaveSection name helper: representative enumerators + Unknown -------------
TEST(SaveFormatStep3, SaveSectionNames)
{
    EXPECT_STREQ(saveload::SaveSectionName(saveload::SaveSection::Header), "Header");
    EXPECT_STREQ(saveload::SaveSectionName(saveload::SaveSection::ALife), "ALife");
    EXPECT_STREQ(saveload::SaveSectionName(saveload::SaveSection::Metadata), "Metadata");
    EXPECT_STREQ(saveload::SaveSectionName(saveload::SaveSection::Trailer), "Trailer");
    EXPECT_STREQ(saveload::SaveSectionName(static_cast<saveload::SaveSection>(9999)), "Unknown");
    static_assert(std::is_same_v<std::underlying_type_t<saveload::SaveSection>, std::uint16_t>);
}
