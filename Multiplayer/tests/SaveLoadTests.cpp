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
#include "stalkermp/saveload/SaveReader.h"
#include "stalkermp/saveload/SaveWriter.h"
#include "stalkermp/snapshot/ISnapshotView.h"
#include "stalkermp/snapshot/SnapshotTypes.h"

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

// ============================================================================
// Batch-2 shared fixture — a minimal fake ISnapshotView
// ============================================================================

namespace
{
    class FakeSnapshotView final : public snapshot::ISnapshotView
    {
    public:
        [[nodiscard]] const snapshot::SnapshotMetadata& Metadata() const override { return m_metadata; }
        [[nodiscard]] const std::vector<snapshot::EntitySnapshot>& Entities() const override { return m_entities; }
        [[nodiscard]] const std::vector<snapshot::PlayerSnapshot>& Players() const override { return m_players; }
        [[nodiscard]] const snapshot::EnvironmentSnapshot& Environment() const override { return m_environment; }

        snapshot::SnapshotMetadata m_metadata{};
        std::vector<snapshot::EntitySnapshot> m_entities;
        std::vector<snapshot::PlayerSnapshot> m_players;
        snapshot::EnvironmentSnapshot m_environment{};
    };

    // A representative populated view: tick 4200, 2 entities, 1 player, env set.
    FakeSnapshotView MakeView()
    {
        FakeSnapshotView v;
        v.m_metadata.id = snapshot::SnapshotId{7};
        v.m_metadata.simulationTick = 4200;
        v.m_metadata.version = 3;
        v.m_metadata.state = snapshot::SnapshotState::Finalized;

        snapshot::EntitySnapshot e1{};
        e1.id = world::EntityId{1};
        e1.position = world::Vec3{1.5f, 0.0f, -2.0f};
        e1.velocity = world::Vec3{0.25f, 0.0f, 0.0f};
        e1.state = 11u;
        e1.flags = 22u;
        e1.inventoryRef = world::EntityId{5};
        e1.runtimeState = 0xABCDEF01u;
        snapshot::EntitySnapshot e2{};
        e2.id = world::EntityId{2};
        e2.position = world::Vec3{100.0f, 1.0f, 3.0f};
        v.m_entities.push_back(e1);
        v.m_entities.push_back(e2);

        snapshot::PlayerSnapshot p{};
        p.id = player::PlayerId{9};
        p.entity = world::EntityId{1};
        p.position = world::Vec3{3.0f, 0.0f, -1.0f};
        p.simulationState = 77u;
        p.authorityFlags = 4u;
        v.m_players.push_back(p);

        v.m_environment.weatherId = 2;
        v.m_environment.timeOfDaySeconds = 36000;
        v.m_environment.emissionState = 1;
        v.m_environment.lighting = 5;
        v.m_environment.environmentVersion = 7;
        return v;
    }

    persistence::SaveMetadata MakeMetadata()
    {
        persistence::SaveMetadata m{};
        m.saveId = 42;
        m.simulationTick = 4200;
        m.playerCount = 1;
        m.entityCount = 2;
        m.worldVersion = 7;
        m.buildVersion = 3;
        m.checksum = 0x1122334455667788ull;
        m.timestampWallClock = 999999u; // diagnostic — must NOT be serialized
        return m;
    }
} // namespace

// ============================================================================
// Step 4 — SaveWriter
// ============================================================================

// --- Serialize produces a non-empty artifact with a valid header --------------
TEST(SaveWriterStep4, ProducesFramedArtifact)
{
    const FakeSnapshotView view = MakeView();
    const auto bytes = saveload::SaveWriter::Serialize(view, MakeMetadata());
    ASSERT_GT(bytes.size(), 12u);

    saveload::ByteReader r{bytes.data(), bytes.size()};
    EXPECT_EQ(saveload::ReadHeader(r), saveload::SaveLoadOutcome::Ok); // magic + version framed
}

// --- Deterministic: identical content => byte-identical output ----------------
TEST(SaveWriterStep4, Deterministic)
{
    const FakeSnapshotView a = MakeView();
    const FakeSnapshotView b = MakeView();
    const auto ba = saveload::SaveWriter::Serialize(a, MakeMetadata());
    const auto bb = saveload::SaveWriter::Serialize(b, MakeMetadata());
    EXPECT_EQ(ba, bb);
}

// --- The diagnostic wall-clock does not affect the serialized identity --------
TEST(SaveWriterStep4, WallClockExcludedFromArtifact)
{
    const FakeSnapshotView view = MakeView();
    persistence::SaveMetadata m1 = MakeMetadata();
    persistence::SaveMetadata m2 = MakeMetadata();
    m1.timestampWallClock = 1u;
    m2.timestampWallClock = 123456789u; // differs only in the diagnostic field
    EXPECT_EQ(saveload::SaveWriter::Serialize(view, m1), saveload::SaveWriter::Serialize(view, m2));
}

// --- Content changes change the artifact --------------------------------------
TEST(SaveWriterStep4, ContentSensitive)
{
    const auto base = saveload::SaveWriter::Serialize(MakeView(), MakeMetadata());

    FakeSnapshotView moved = MakeView();
    moved.m_entities[0].position.x = 999.0f;
    EXPECT_NE(saveload::SaveWriter::Serialize(moved, MakeMetadata()), base);

    FakeSnapshotView extra = MakeView();
    snapshot::EntitySnapshot e3{};
    e3.id = world::EntityId{3};
    extra.m_entities.push_back(e3);
    EXPECT_NE(saveload::SaveWriter::Serialize(extra, MakeMetadata()), base);
}

// ============================================================================
// Step 5 — SaveReader
// ============================================================================

// --- Round-trip: writer output parses back to the same records ----------------
TEST(SaveReaderStep5, RoundTrip)
{
    const FakeSnapshotView view = MakeView();
    const auto bytes = saveload::SaveWriter::Serialize(view, MakeMetadata());

    const saveload::ParseResult result = saveload::SaveReader::Parse(bytes);
    ASSERT_EQ(result.outcome, saveload::SaveLoadOutcome::Ok);
    const saveload::LoadedSave& s = result.save;

    // World / scheduler tick carried from the snapshot.
    EXPECT_EQ(s.world.simulationTick, 4200u);
    EXPECT_EQ(s.scheduler.simulationTick, 4200u);

    // Environment.
    EXPECT_EQ(s.environment.weatherId, 2u);
    EXPECT_EQ(s.environment.timeOfDaySeconds, 36000u);
    EXPECT_EQ(s.environment.environmentVersion, 7u);

    // Entities (ascending, values preserved).
    ASSERT_EQ(s.entities.size(), 2u);
    EXPECT_EQ(s.entities[0].id.value, 1u);
    EXPECT_FLOAT_EQ(s.entities[0].position.x, 1.5f);
    EXPECT_FLOAT_EQ(s.entities[0].velocity.x, 0.25f);
    EXPECT_EQ(s.entities[0].stateFlags, 11u);
    EXPECT_EQ(s.entities[0].runtimeFlags, 22u);
    EXPECT_EQ(s.entities[0].inventoryRef.value, 5u);
    EXPECT_EQ(s.entities[0].runtimeState, 0xABCDEF01u);
    EXPECT_EQ(s.entities[1].id.value, 2u);
    EXPECT_FLOAT_EQ(s.entities[1].position.x, 100.0f);

    // Players.
    ASSERT_EQ(s.players.size(), 1u);
    EXPECT_EQ(s.players[0].id.value, 9u);
    EXPECT_EQ(s.players[0].entity.value, 1u);
    EXPECT_FLOAT_EQ(s.players[0].position.z, -1.0f);
    EXPECT_EQ(s.players[0].statistics, 77u);
    EXPECT_EQ(s.players[0].connectionState, player::PlayerConnectionState::Connected);

    // ALife empty (no snapshot source); metadata round-trips (wall-clock defaults 0).
    EXPECT_TRUE(s.alife.empty());
    EXPECT_EQ(s.metadata.saveId, 42u);
    EXPECT_EQ(s.metadata.entityCount, 2u);
    EXPECT_EQ(s.metadata.checksum, 0x1122334455667788ull);
    EXPECT_EQ(s.metadata.timestampWallClock, 0u); // not serialized
}

// --- Bad magic => CorruptedSave ------------------------------------------------
TEST(SaveReaderStep5, BadMagicRejected)
{
    auto bytes = saveload::SaveWriter::Serialize(MakeView(), MakeMetadata());
    bytes[0] ^= 0xFFu;
    EXPECT_EQ(saveload::SaveReader::Parse(bytes).outcome, saveload::SaveLoadOutcome::CorruptedSave);
}

// --- Wrong format version => VersionMismatch ----------------------------------
TEST(SaveReaderStep5, WrongFormatVersionRejected)
{
    auto bytes = saveload::SaveWriter::Serialize(MakeView(), MakeMetadata());
    // Format version is the 4 bytes after the 4-byte magic (little-endian); bump it.
    bytes[4] = static_cast<std::uint8_t>(saveload::kSaveFormatVersion + 1u);
    EXPECT_EQ(saveload::SaveReader::Parse(bytes).outcome, saveload::SaveLoadOutcome::VersionMismatch);
}

// --- A flipped content byte => ChecksumFailure --------------------------------
TEST(SaveReaderStep5, ChecksumMismatchRejected)
{
    auto bytes = saveload::SaveWriter::Serialize(MakeView(), MakeMetadata());
    // Corrupt a byte in the body (after the header, before the trailer).
    bytes[20] ^= 0x01u;
    EXPECT_EQ(saveload::SaveReader::Parse(bytes).outcome, saveload::SaveLoadOutcome::ChecksumFailure);
}

// --- Truncated artifact => CorruptedSave --------------------------------------
TEST(SaveReaderStep5, TruncatedRejected)
{
    auto bytes = saveload::SaveWriter::Serialize(MakeView(), MakeMetadata());
    bytes.resize(bytes.size() / 2); // cut off the tail
    const auto outcome = saveload::SaveReader::Parse(bytes).outcome;
    EXPECT_NE(outcome, saveload::SaveLoadOutcome::Ok);
    EXPECT_EQ(outcome, saveload::SaveLoadOutcome::CorruptedSave);

    // Empty input is also structurally rejected (no header).
    EXPECT_EQ(saveload::SaveReader::Parse(std::vector<std::uint8_t>{}).outcome,
              saveload::SaveLoadOutcome::CorruptedSave);
}

// --- Empty world (no entities/players) still round-trips ----------------------
TEST(SaveReaderStep5, EmptyWorldRoundTrip)
{
    FakeSnapshotView empty;
    empty.m_metadata.id = snapshot::SnapshotId{1};
    empty.m_metadata.state = snapshot::SnapshotState::Finalized;
    const auto bytes = saveload::SaveWriter::Serialize(empty, persistence::SaveMetadata{});

    const auto result = saveload::SaveReader::Parse(bytes);
    ASSERT_EQ(result.outcome, saveload::SaveLoadOutcome::Ok);
    EXPECT_TRUE(result.save.entities.empty());
    EXPECT_TRUE(result.save.players.empty());
    EXPECT_TRUE(result.save.alife.empty());
}
