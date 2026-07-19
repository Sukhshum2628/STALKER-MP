// STALKER-MP — Persistence framework tests (Sprint-011)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers. Engine-free and OS-free. Types only — no configuration,
//         queue, worker, scheduler, manager, storage, or thread.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include "stalkermp/core/Config.h"
#include "stalkermp/persistence/InMemoryPersistenceStore.h"
#include "stalkermp/persistence/IPersistenceStore.h"
#include "stalkermp/persistence/NullPersistenceStore.h"
#include "stalkermp/persistence/PersistenceConfiguration.h"
#include "stalkermp/persistence/PersistenceQueue.h"
#include "stalkermp/persistence/PersistenceSnapshot.h"
#include "stalkermp/persistence/PersistenceTypes.h"
#include "stalkermp/persistence/SaveMetadataBuilder.h"
#include "stalkermp/persistence/ValidationFramework.h"
#include "stalkermp/persistence/VersionManager.h"
#include "stalkermp/snapshot/ISnapshotView.h"
#include "stalkermp/snapshot/SnapshotTypes.h"

using namespace stalkermp;

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(PersistenceTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<persistence::PersistenceOutcome>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<persistence::SaveTrigger>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<persistence::SaveState>, std::uint8_t>);
    SUCCEED();
}

// --- Reserved 0 carries the neutral/safe meaning ------------------------------
TEST(PersistenceTypesStep1, ReservedZeroIsNeutral)
{
    static_assert(static_cast<std::uint8_t>(persistence::PersistenceOutcome::Ok) == 0u);
    static_assert(static_cast<std::uint8_t>(persistence::SaveTrigger::Manual) == 0u);
    static_assert(static_cast<std::uint8_t>(persistence::SaveState::Pending) == 0u);
    SUCCEED();
}

// --- POD value captures: trivially copyable + documented defaults -------------
TEST(PersistenceTypesStep1, PodDefaults)
{
    static_assert(std::is_trivially_copyable_v<persistence::SaveRequest>);
    static_assert(std::is_trivially_copyable_v<persistence::SaveMetadata>);
    static_assert(std::is_trivially_copyable_v<persistence::PersistenceStatistics>);
    static_assert(std::is_trivially_copyable_v<persistence::PersistenceConsistency>);

    const persistence::SaveRequest req{};
    EXPECT_EQ(req.id, 0u);
    EXPECT_EQ(req.trigger, persistence::SaveTrigger::Manual);
    EXPECT_EQ(req.requestTick, 0u);

    const persistence::SaveMetadata meta{};
    EXPECT_EQ(meta.saveId, 0u);
    EXPECT_EQ(meta.simulationTick, 0u);
    EXPECT_EQ(meta.playerCount, 0u);
    EXPECT_EQ(meta.entityCount, 0u);
    EXPECT_EQ(meta.worldVersion, 0u);
    EXPECT_EQ(meta.buildVersion, 0u);
    EXPECT_EQ(meta.checksum, 0u);
    EXPECT_EQ(meta.timestampWallClock, 0u);

    const persistence::PersistenceStatistics stats{};
    EXPECT_EQ(stats.saveRequests, 0u);
    EXPECT_EQ(stats.autosaves, 0u);
    EXPECT_EQ(stats.completedSaves, 0u);
    EXPECT_EQ(stats.failedSaves, 0u);
    EXPECT_EQ(stats.retries, 0u);
    EXPECT_EQ(stats.queueOverflows, 0u);
    EXPECT_EQ(stats.currentQueueDepth, 0u);
    EXPECT_EQ(stats.maxQueueDepth, 0u);
    EXPECT_EQ(stats.lastSaveDurationMicros, 0u);
    EXPECT_EQ(stats.maxSaveDurationMicros, 0u);
    EXPECT_EQ(stats.timestampWallClock, 0u);
}

// --- Consistency defaults healthy; any false flag makes it unhealthy ----------
TEST(PersistenceTypesStep1, ConsistencyHealthyByDefault)
{
    persistence::PersistenceConsistency c{};
    EXPECT_TRUE(c.readOnly);
    EXPECT_TRUE(c.boundedQueue);
    EXPECT_TRUE(c.versionTracked);
    EXPECT_TRUE(c.failureIsolated);
    EXPECT_TRUE(c.snapshotImmutable);
    EXPECT_TRUE(c.IsHealthy());

    c.readOnly = false;
    EXPECT_FALSE(c.IsHealthy());

    persistence::PersistenceConsistency d{};
    d.failureIsolated = false;
    EXPECT_FALSE(d.IsHealthy());
}

// --- PersistenceOutcome names: every enumerator + Unknown fallback ------------
TEST(PersistenceTypesStep1, OutcomeNamesTotal)
{
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::Ok), "Ok");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::QueueFull), "QueueFull");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::VersionMismatch),
                 "VersionMismatch");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::IntegrityFailure),
                 "IntegrityFailure");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::IncompleteSnapshot),
                 "IncompleteSnapshot");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::WorkerUnavailable),
                 "WorkerUnavailable");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::StorageUnavailable),
                 "StorageUnavailable");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::NothingToPersist),
                 "NothingToPersist");
    EXPECT_STREQ(persistence::PersistenceOutcomeName(static_cast<persistence::PersistenceOutcome>(200)), "Unknown");
}

// --- SaveTrigger names: every enumerator + Unknown fallback -------------------
TEST(PersistenceTypesStep1, TriggerNamesTotal)
{
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Manual), "Manual");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Autosave), "Autosave");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Administrative), "Administrative");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Shutdown), "Shutdown");
    EXPECT_STREQ(persistence::SaveTriggerName(persistence::SaveTrigger::Deferred), "Deferred");
    EXPECT_STREQ(persistence::SaveTriggerName(static_cast<persistence::SaveTrigger>(200)), "Unknown");
}

// --- SaveState names: every enumerator + Unknown fallback --------------------
TEST(PersistenceTypesStep1, StateNamesTotal)
{
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Pending), "Pending");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Validating), "Validating");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Persisting), "Persisting");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Completed), "Completed");
    EXPECT_STREQ(persistence::SaveStateName(persistence::SaveState::Failed), "Failed");
    EXPECT_STREQ(persistence::SaveStateName(static_cast<persistence::SaveState>(200)), "Unknown");
}

// --- Name helpers are constexpr (compile-time evaluable) ----------------------
TEST(PersistenceTypesStep1, NameHelpersAreConstexpr)
{
    static_assert(persistence::PersistenceOutcomeName(persistence::PersistenceOutcome::Ok)[0] == 'O');
    static_assert(persistence::SaveTriggerName(persistence::SaveTrigger::Autosave)[0] == 'A');
    static_assert(persistence::SaveStateName(persistence::SaveState::Failed)[0] == 'F');
    SUCCEED();
}

// ============================================================================
// Step 2 — PersistenceConfiguration::FromConfig
// ============================================================================

// --- Missing [persistence] section => all documented defaults -----------------
TEST(PersistenceConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.queueDepth, 16u);
    EXPECT_EQ(c.autosaveIntervalTicks, 0u);
    EXPECT_EQ(c.maxRetries, 3u);
    EXPECT_EQ(c.retryBackoffTicks, 30u);
    EXPECT_EQ(c.backpressureHighWatermark, 12u);
    EXPECT_EQ(c.backpressureLowWatermark, 4u);
    EXPECT_EQ(c.version, 1u);
}

// --- Each field parses a valid supplied value (override) ----------------------
TEST(PersistenceConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("persistence", "queue_depth", "64");
    store.Set("persistence", "autosave_interval_ticks", "1800");
    store.Set("persistence", "max_retries", "5");
    store.Set("persistence", "retry_backoff_ticks", "60");
    store.Set("persistence", "backpressure_high_watermark", "50");
    store.Set("persistence", "backpressure_low_watermark", "10");
    store.Set("persistence", "version", "2");
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.queueDepth, 64u);
    EXPECT_EQ(c.autosaveIntervalTicks, 1800u);
    EXPECT_EQ(c.maxRetries, 5u);
    EXPECT_EQ(c.retryBackoffTicks, 60u);
    EXPECT_EQ(c.backpressureHighWatermark, 50u);
    EXPECT_EQ(c.backpressureLowWatermark, 10u);
    EXPECT_EQ(c.version, 2u);
}

// --- Zeroable fields accept 0 (autosave disabled; no retry/backoff; wm off) ----
TEST(PersistenceConfigStep2, ZeroableFieldsAcceptZero)
{
    core::ConfigStore store;
    store.Set("persistence", "autosave_interval_ticks", "0");
    store.Set("persistence", "max_retries", "0");
    store.Set("persistence", "retry_backoff_ticks", "0");
    store.Set("persistence", "backpressure_high_watermark", "0");
    store.Set("persistence", "backpressure_low_watermark", "0");
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.autosaveIntervalTicks, 0u);
    EXPECT_EQ(c.maxRetries, 0u);
    EXPECT_EQ(c.retryBackoffTicks, 0u);
    EXPECT_EQ(c.backpressureHighWatermark, 0u);
    EXPECT_EQ(c.backpressureLowWatermark, 0u);
}

// --- Boundary minimums accepted (queueDepth = version = 1; wm = queueDepth) ----
TEST(PersistenceConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("persistence", "queue_depth", "1");
    store.Set("persistence", "backpressure_high_watermark", "1"); // == queueDepth
    store.Set("persistence", "backpressure_low_watermark", "1");  // == high
    store.Set("persistence", "version", "1");
    const auto r = persistence::PersistenceConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.queueDepth, 1u);
    EXPECT_EQ(c.backpressureHighWatermark, 1u);
    EXPECT_EQ(c.backpressureLowWatermark, 1u);
    EXPECT_EQ(c.version, 1u);
}

// --- Invalid per-field values are rejected (value outcome) --------------------
TEST(PersistenceConfigStep2, InvalidPerFieldValuesRejected)
{
    {
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "0"); // below min 1
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("persistence", "version", "0"); // below min 1
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "-5"); // negative
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
}

// --- Cross-field watermark ordering enforced: low <= high <= queueDepth --------
TEST(PersistenceConfigStep2, WatermarkOrderingEnforced)
{
    {
        // low > high
        core::ConfigStore s;
        s.Set("persistence", "backpressure_high_watermark", "4");
        s.Set("persistence", "backpressure_low_watermark", "8");
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        // high > queueDepth
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "8");
        s.Set("persistence", "backpressure_high_watermark", "16");
        s.Set("persistence", "backpressure_low_watermark", "2");
        EXPECT_FALSE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
    {
        // valid ordering accepted
        core::ConfigStore s;
        s.Set("persistence", "queue_depth", "8");
        s.Set("persistence", "backpressure_high_watermark", "8");
        s.Set("persistence", "backpressure_low_watermark", "8");
        EXPECT_TRUE(persistence::PersistenceConfiguration::FromConfig(s).HasValue());
    }
}

// ============================================================================
// Step 3 — VersionManager
// ============================================================================

namespace
{
    persistence::VersionSet MakeVersions(std::uint32_t p, std::uint32_t w, std::uint32_t s, std::uint32_t c)
    {
        persistence::VersionSet v{};
        v.persistence = p;
        v.world = w;
        v.schema = s;
        v.compatibility = c;
        return v;
    }
} // namespace

// --- VersionSet defaults + equality ------------------------------------------
TEST(VersionManagerStep3, VersionSetDefaultsAndEquality)
{
    constexpr persistence::VersionSet a{};
    static_assert(a.persistence == 1u && a.world == 1u && a.schema == 1u && a.compatibility == 1u);
    static_assert(persistence::VersionSet{} == persistence::VersionSet{});
    EXPECT_TRUE(MakeVersions(1, 1, 1, 1) == persistence::VersionSet{});
    EXPECT_TRUE(MakeVersions(2, 1, 1, 1) != persistence::VersionSet{});
}

// --- Identical version sets compare Equal + compatible + no migration ---------
TEST(VersionManagerStep3, EqualWhenIdentical)
{
    const persistence::VersionManager mgr{MakeVersions(3, 4, 5, 6)};
    const auto candidate = MakeVersions(3, 4, 5, 6);
    EXPECT_EQ(mgr.Compare(candidate), persistence::VersionCompatibility::Equal);
    EXPECT_TRUE(mgr.IsCompatible(candidate));
    EXPECT_FALSE(mgr.MigrationRequired(candidate));
    EXPECT_TRUE(mgr.Current() == MakeVersions(3, 4, 5, 6));
}

// --- Differing persistence/world/schema (same compatibility) => migration -----
TEST(VersionManagerStep3, MigrationRequiredWhenInnerVersionsDiffer)
{
    const persistence::VersionManager mgr{MakeVersions(2, 2, 2, 5)};

    // persistence differs
    EXPECT_EQ(mgr.Compare(MakeVersions(1, 2, 2, 5)), persistence::VersionCompatibility::MigrationRequired);
    // world differs
    EXPECT_EQ(mgr.Compare(MakeVersions(2, 9, 2, 5)), persistence::VersionCompatibility::MigrationRequired);
    // schema differs
    EXPECT_EQ(mgr.Compare(MakeVersions(2, 2, 7, 5)), persistence::VersionCompatibility::MigrationRequired);

    EXPECT_TRUE(mgr.IsCompatible(MakeVersions(1, 2, 2, 5)));       // migratable
    EXPECT_TRUE(mgr.MigrationRequired(MakeVersions(2, 2, 7, 5)));
}

// --- Differing compatibility boundary => Incompatible (hard gate) -------------
TEST(VersionManagerStep3, IncompatibleWhenCompatibilityDiffers)
{
    const persistence::VersionManager mgr{MakeVersions(2, 2, 2, 5)};

    // Even when everything else matches, a differing compatibility is Incompatible.
    EXPECT_EQ(mgr.Compare(MakeVersions(2, 2, 2, 6)), persistence::VersionCompatibility::Incompatible);
    EXPECT_EQ(mgr.Compare(MakeVersions(2, 2, 2, 4)), persistence::VersionCompatibility::Incompatible);
    // Compatibility gate dominates inner differences too.
    EXPECT_EQ(mgr.Compare(MakeVersions(9, 9, 9, 6)), persistence::VersionCompatibility::Incompatible);

    EXPECT_FALSE(mgr.IsCompatible(MakeVersions(2, 2, 2, 6)));
    EXPECT_FALSE(mgr.MigrationRequired(MakeVersions(2, 2, 2, 6))); // incompatible, not migratable
}

// --- Compare is deterministic + constexpr-evaluable ---------------------------
TEST(VersionManagerStep3, DeterministicAndConstexpr)
{
    constexpr persistence::VersionManager mgr{persistence::VersionSet{}};
    static_assert(mgr.Compare(persistence::VersionSet{}) == persistence::VersionCompatibility::Equal);
    static_assert(mgr.IsCompatible(persistence::VersionSet{}));

    const persistence::VersionManager m{MakeVersions(1, 2, 3, 4)};
    const auto cand = MakeVersions(1, 9, 3, 4);
    EXPECT_EQ(m.Compare(cand), m.Compare(cand)); // stable/deterministic
}

// --- VersionCompatibility name helper: all enumerators + Unknown --------------
TEST(VersionManagerStep3, CompatibilityNamesTotal)
{
    EXPECT_STREQ(persistence::VersionCompatibilityName(persistence::VersionCompatibility::Equal), "Equal");
    EXPECT_STREQ(persistence::VersionCompatibilityName(persistence::VersionCompatibility::MigrationRequired),
                 "MigrationRequired");
    EXPECT_STREQ(persistence::VersionCompatibilityName(persistence::VersionCompatibility::Incompatible),
                 "Incompatible");
    EXPECT_STREQ(persistence::VersionCompatibilityName(static_cast<persistence::VersionCompatibility>(200)),
                 "Unknown");
    static_assert(std::is_same_v<std::underlying_type_t<persistence::VersionCompatibility>, std::uint8_t>);
}

// ============================================================================
// Step 4 — SaveMetadataBuilder
// ============================================================================

namespace
{
    // A minimal, configurable ISnapshotView for tests. Owns its captured value data.
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

    snapshot::EntitySnapshot SnapEntity(std::uint32_t id, float x)
    {
        snapshot::EntitySnapshot e{};
        e.id = world::EntityId{id};
        e.position = world::Vec3{x, 0.0f, 0.0f};
        return e;
    }

    // A representative populated view: tick 4200, 2 entities, 1 player, env version 7.
    FakeSnapshotView MakeView()
    {
        FakeSnapshotView v;
        v.m_metadata.simulationTick = 4200;
        v.m_metadata.version = 3; // snapshot/build/format version
        v.m_metadata.entityCount = 2;
        v.m_metadata.playerCount = 1;
        v.m_entities.push_back(SnapEntity(1, 1.0f));
        v.m_entities.push_back(SnapEntity(2, 2.5f));
        snapshot::PlayerSnapshot p{};
        p.id = player::PlayerId{9};
        p.entity = world::EntityId{1};
        p.position = world::Vec3{3.0f, 0.0f, -1.0f};
        v.m_players.push_back(p);
        v.m_environment.environmentVersion = 7; // world version
        v.m_environment.weatherId = 2;
        v.m_environment.timeOfDaySeconds = 36000;
        return v;
    }
} // namespace

// --- Field capture: tick, counts, versions, saveId ----------------------------
TEST(SaveMetadataStep4, CapturesFields)
{
    const FakeSnapshotView view = MakeView();
    const auto meta = persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Manual, /*saveId*/ 42);

    EXPECT_EQ(meta.saveId, 42u);
    EXPECT_EQ(meta.simulationTick, 4200u);
    EXPECT_EQ(meta.entityCount, 2u);
    EXPECT_EQ(meta.playerCount, 1u);
    EXPECT_EQ(meta.worldVersion, 7u); // Environment().environmentVersion
    EXPECT_EQ(meta.buildVersion, 3u); // snapshot Metadata().version
    EXPECT_NE(meta.checksum, 0u);
}

// --- Checksum determinism: identical content => identical checksum -------------
TEST(SaveMetadataStep4, ChecksumDeterministic)
{
    const FakeSnapshotView a = MakeView();
    const FakeSnapshotView b = MakeView();
    EXPECT_EQ(persistence::SaveMetadataBuilder::Checksum(a), persistence::SaveMetadataBuilder::Checksum(b));

    // Build stamps the same checksum it exposes; saveId does not affect it.
    const auto m1 = persistence::SaveMetadataBuilder::Build(a, persistence::SaveTrigger::Manual, 1);
    const auto m2 = persistence::SaveMetadataBuilder::Build(b, persistence::SaveTrigger::Manual, 999);
    EXPECT_EQ(m1.checksum, persistence::SaveMetadataBuilder::Checksum(a));
    EXPECT_EQ(m1.checksum, m2.checksum);
}

// --- Trigger is provenance only: it does not change the metadata/checksum ------
TEST(SaveMetadataStep4, TriggerDoesNotAffectOutput)
{
    const FakeSnapshotView view = MakeView();
    const auto manual = persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Manual, 5);
    const auto autosv = persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Autosave, 5);
    EXPECT_EQ(manual.checksum, autosv.checksum);
    EXPECT_EQ(manual.simulationTick, autosv.simulationTick);
    EXPECT_EQ(manual.entityCount, autosv.entityCount);
}

// --- Checksum sensitivity: any content change changes the checksum ------------
TEST(SaveMetadataStep4, ChecksumSensitiveToContent)
{
    const std::uint64_t base = persistence::SaveMetadataBuilder::Checksum(MakeView());

    {
        FakeSnapshotView v = MakeView();
        v.m_metadata.simulationTick = 4201; // tick changed
        EXPECT_NE(persistence::SaveMetadataBuilder::Checksum(v), base);
    }
    {
        FakeSnapshotView v = MakeView();
        v.m_entities[1].position.x = 2.6f; // entity position changed
        EXPECT_NE(persistence::SaveMetadataBuilder::Checksum(v), base);
    }
    {
        FakeSnapshotView v = MakeView();
        v.m_players[0].authorityFlags = 1; // player field changed
        EXPECT_NE(persistence::SaveMetadataBuilder::Checksum(v), base);
    }
    {
        FakeSnapshotView v = MakeView();
        v.m_environment.weatherId = 3; // environment changed
        EXPECT_NE(persistence::SaveMetadataBuilder::Checksum(v), base);
    }
    {
        FakeSnapshotView v = MakeView();
        v.m_entities.push_back(SnapEntity(3, 9.0f)); // entity count changed
        EXPECT_NE(persistence::SaveMetadataBuilder::Checksum(v), base);
    }
}

// --- Wall-clock is diagnostic-only: excluded from the checksum, passed through -
TEST(SaveMetadataStep4, WallClockExcludedFromChecksumButPassedThrough)
{
    FakeSnapshotView a = MakeView();
    FakeSnapshotView b = MakeView();
    a.m_metadata.timestampWallClock = 111111u;
    b.m_metadata.timestampWallClock = 999999u;   // differs only in diagnostic wall-clock
    a.m_metadata.buildDurationTicks = 5u;
    b.m_metadata.buildDurationTicks = 5000u;      // and the diagnostic build duration

    // Checksum ignores both diagnostic fields.
    EXPECT_EQ(persistence::SaveMetadataBuilder::Checksum(a), persistence::SaveMetadataBuilder::Checksum(b));

    // But the wall-clock is passed through into the metadata (diagnostic field).
    const auto ma = persistence::SaveMetadataBuilder::Build(a, persistence::SaveTrigger::Autosave, 7);
    EXPECT_EQ(ma.timestampWallClock, 111111u);
    EXPECT_EQ(ma.checksum, persistence::SaveMetadataBuilder::Checksum(b)); // content-equal
}

// --- Empty snapshot builds valid metadata (zero counts, stable checksum) ------
TEST(SaveMetadataStep4, EmptySnapshot)
{
    const FakeSnapshotView empty; // no entities/players; tick 0
    const auto meta = persistence::SaveMetadataBuilder::Build(empty, persistence::SaveTrigger::Shutdown, 1);
    EXPECT_EQ(meta.entityCount, 0u);
    EXPECT_EQ(meta.playerCount, 0u);
    EXPECT_EQ(meta.simulationTick, 0u);
    EXPECT_EQ(meta.checksum, persistence::SaveMetadataBuilder::Checksum(empty)); // deterministic
}

// ============================================================================
// Step 5 — PersistenceSnapshot projection
// ============================================================================

namespace
{
    // A sealed, structurally-consistent view: id 100, tick 4200, 2 entities, 1
    // player, header counts matching the containers, state Finalized.
    FakeSnapshotView MakeSealedView()
    {
        FakeSnapshotView v = MakeView(); // tick 4200, 2 entities, 1 player
        v.m_metadata.id = snapshot::SnapshotId{100};
        v.m_metadata.entityCount = 2; // matches Entities().size()
        v.m_metadata.playerCount = 1; // matches Players().size()
        v.m_metadata.state = snapshot::SnapshotState::Finalized;
        return v;
    }
} // namespace

// --- Projection accessors mirror the underlying view (no copy) -----------------
TEST(PersistenceSnapshotStep5, AccessorsProjectView)
{
    const FakeSnapshotView view = MakeSealedView();
    const persistence::PersistenceSnapshot snap{view};

    EXPECT_EQ(snap.Tick(), 4200u);
    EXPECT_EQ(snap.EntityCount(), 2u);
    EXPECT_EQ(snap.PlayerCount(), 1u);
    EXPECT_EQ(&snap.View(), &static_cast<const snapshot::ISnapshotView&>(view)); // borrows, no copy
}

// --- IsComplete: sealed (Finalized/Published) is complete; Building/Retired not -
TEST(PersistenceSnapshotStep5, IsCompleteReflectsSealedState)
{
    FakeSnapshotView v = MakeSealedView();

    v.m_metadata.state = snapshot::SnapshotState::Finalized;
    EXPECT_TRUE(persistence::PersistenceSnapshot{v}.IsComplete());

    v.m_metadata.state = snapshot::SnapshotState::Published;
    EXPECT_TRUE(persistence::PersistenceSnapshot{v}.IsComplete());

    v.m_metadata.state = snapshot::SnapshotState::Building;
    EXPECT_FALSE(persistence::PersistenceSnapshot{v}.IsComplete());

    v.m_metadata.state = snapshot::SnapshotState::Retired;
    EXPECT_FALSE(persistence::PersistenceSnapshot{v}.IsComplete());
}

// --- IntegrityOk: real id + header counts agree with the containers -----------
TEST(PersistenceSnapshotStep5, IntegrityOkRequiresConsistentHeader)
{
    // Consistent + real id -> ok.
    EXPECT_TRUE(persistence::PersistenceSnapshot{MakeSealedView()}.IntegrityOk());

    // None id -> not ok.
    {
        FakeSnapshotView v = MakeSealedView();
        v.m_metadata.id = snapshot::SnapshotId{0};
        EXPECT_FALSE(persistence::PersistenceSnapshot{v}.IntegrityOk());
    }
    // entityCount mismatch -> not ok.
    {
        FakeSnapshotView v = MakeSealedView();
        v.m_metadata.entityCount = 5; // containers hold 2
        EXPECT_FALSE(persistence::PersistenceSnapshot{v}.IntegrityOk());
    }
    // playerCount mismatch -> not ok.
    {
        FakeSnapshotView v = MakeSealedView();
        v.m_metadata.playerCount = 9; // containers hold 1
        EXPECT_FALSE(persistence::PersistenceSnapshot{v}.IntegrityOk());
    }
}

// --- Deterministic + non-mutating: repeated reads are stable, view unchanged ---
TEST(PersistenceSnapshotStep5, DeterministicAndReadOnly)
{
    FakeSnapshotView view = MakeSealedView();
    const persistence::PersistenceSnapshot snap{view};

    EXPECT_EQ(snap.Tick(), snap.Tick());
    EXPECT_EQ(snap.IsComplete(), snap.IsComplete());
    EXPECT_EQ(snap.IntegrityOk(), snap.IntegrityOk());

    // The projection never mutates the underlying view.
    EXPECT_EQ(view.Entities().size(), 2u);
    EXPECT_EQ(view.m_metadata.simulationTick, 4200u);
    EXPECT_EQ(view.m_metadata.state, snapshot::SnapshotState::Finalized);
}

// ============================================================================
// Step 6 — ValidationFramework
// ============================================================================

namespace
{
    persistence::SaveRequest MakeRequest(std::uint64_t id, std::uint64_t tick,
                                         persistence::SaveTrigger trigger = persistence::SaveTrigger::Manual)
    {
        persistence::SaveRequest r{};
        r.id = id;
        r.trigger = trigger;
        r.requestTick = tick;
        return r;
    }
} // namespace

// --- ValidateIntegrity: consistent header Ok; inconsistent => IntegrityFailure -
TEST(ValidationStep6, ValidateIntegrity)
{
    const FakeSnapshotView good = MakeSealedView();
    EXPECT_EQ(persistence::ValidationFramework::ValidateIntegrity(persistence::PersistenceSnapshot{good}),
              persistence::PersistenceOutcome::Ok);

    FakeSnapshotView bad = MakeSealedView();
    bad.m_metadata.entityCount = 99; // header disagrees with containers
    EXPECT_EQ(persistence::ValidationFramework::ValidateIntegrity(persistence::PersistenceSnapshot{bad}),
              persistence::PersistenceOutcome::IntegrityFailure);

    FakeSnapshotView noneId = MakeSealedView();
    noneId.m_metadata.id = snapshot::SnapshotId{0};
    EXPECT_EQ(persistence::ValidationFramework::ValidateIntegrity(persistence::PersistenceSnapshot{noneId}),
              persistence::PersistenceOutcome::IntegrityFailure);
}

// --- ValidateCompleteness: sealed Ok; unsealed => IncompleteSnapshot ----------
TEST(ValidationStep6, ValidateCompleteness)
{
    FakeSnapshotView v = MakeSealedView();

    v.m_metadata.state = snapshot::SnapshotState::Finalized;
    EXPECT_EQ(persistence::ValidationFramework::ValidateCompleteness(persistence::PersistenceSnapshot{v}),
              persistence::PersistenceOutcome::Ok);

    v.m_metadata.state = snapshot::SnapshotState::Published;
    EXPECT_EQ(persistence::ValidationFramework::ValidateCompleteness(persistence::PersistenceSnapshot{v}),
              persistence::PersistenceOutcome::Ok);

    v.m_metadata.state = snapshot::SnapshotState::Building;
    EXPECT_EQ(persistence::ValidationFramework::ValidateCompleteness(persistence::PersistenceSnapshot{v}),
              persistence::PersistenceOutcome::IncompleteSnapshot);

    v.m_metadata.state = snapshot::SnapshotState::Retired;
    EXPECT_EQ(persistence::ValidationFramework::ValidateCompleteness(persistence::PersistenceSnapshot{v}),
              persistence::PersistenceOutcome::IncompleteSnapshot);
}

// --- ValidateVersion: Equal/MigrationRequired Ok; Incompatible => VersionMismatch
TEST(ValidationStep6, ValidateVersion)
{
    const persistence::VersionManager mgr{MakeVersions(2, 2, 2, 5)};

    // Equal -> Ok
    EXPECT_EQ(persistence::ValidationFramework::ValidateVersion(mgr, MakeVersions(2, 2, 2, 5)),
              persistence::PersistenceOutcome::Ok);
    // MigrationRequired (inner differs, same compatibility) -> Ok (reconcilable)
    EXPECT_EQ(persistence::ValidationFramework::ValidateVersion(mgr, MakeVersions(1, 2, 9, 5)),
              persistence::PersistenceOutcome::Ok);
    // Incompatible (compatibility boundary differs) -> VersionMismatch
    EXPECT_EQ(persistence::ValidationFramework::ValidateVersion(mgr, MakeVersions(2, 2, 2, 6)),
              persistence::PersistenceOutcome::VersionMismatch);
}

// --- ValidateQueueOrdering: strict ascending id + non-decreasing tick ----------
TEST(ValidationStep6, ValidateQueueOrdering)
{
    // No predecessor (id 0) always validates.
    EXPECT_EQ(persistence::ValidationFramework::ValidateQueueOrdering(MakeRequest(0, 0), MakeRequest(1, 10)),
              persistence::PersistenceOutcome::Ok);

    // Ascending id, non-decreasing tick -> Ok.
    EXPECT_EQ(persistence::ValidationFramework::ValidateQueueOrdering(MakeRequest(1, 10), MakeRequest(2, 10)),
              persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(persistence::ValidationFramework::ValidateQueueOrdering(MakeRequest(1, 10), MakeRequest(2, 20)),
              persistence::PersistenceOutcome::Ok);

    // Non-increasing id -> IntegrityFailure.
    EXPECT_EQ(persistence::ValidationFramework::ValidateQueueOrdering(MakeRequest(2, 10), MakeRequest(2, 20)),
              persistence::PersistenceOutcome::IntegrityFailure);
    EXPECT_EQ(persistence::ValidationFramework::ValidateQueueOrdering(MakeRequest(5, 10), MakeRequest(3, 20)),
              persistence::PersistenceOutcome::IntegrityFailure);

    // Regressing tick -> IntegrityFailure.
    EXPECT_EQ(persistence::ValidationFramework::ValidateQueueOrdering(MakeRequest(1, 20), MakeRequest(2, 10)),
              persistence::PersistenceOutcome::IntegrityFailure);
}

// --- Validators are deterministic and leave inputs unchanged ------------------
TEST(ValidationStep6, DeterministicAndNonMutating)
{
    FakeSnapshotView v = MakeSealedView();
    const persistence::PersistenceSnapshot snap{v};

    EXPECT_EQ(persistence::ValidationFramework::ValidateIntegrity(snap),
              persistence::ValidationFramework::ValidateIntegrity(snap));
    EXPECT_EQ(persistence::ValidationFramework::ValidateCompleteness(snap),
              persistence::ValidationFramework::ValidateCompleteness(snap));

    // Inputs unchanged by validation.
    EXPECT_EQ(v.m_metadata.entityCount, 2u);
    EXPECT_EQ(v.m_metadata.state, snapshot::SnapshotState::Finalized);
}

// ============================================================================
// Step 7 — PersistenceQueue
// ============================================================================

namespace
{
    persistence::PersistenceConfiguration QueueConfig(std::uint32_t depth, std::uint32_t high, std::uint32_t low)
    {
        persistence::PersistenceConfiguration c{};
        c.queueDepth = depth;
        c.backpressureHighWatermark = high;
        c.backpressureLowWatermark = low;
        return c;
    }
} // namespace

// --- FIFO publish / acquire / release ordering --------------------------------
TEST(PersistenceQueueStep7, FifoOrder)
{
    persistence::PersistenceQueue q{QueueConfig(4, 0, 0)};
    EXPECT_TRUE(q.Empty());
    EXPECT_EQ(q.Publish(MakeRequest(1, 10)), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(q.Publish(MakeRequest(2, 20)), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(q.Publish(MakeRequest(3, 30)), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(q.Size(), 3u);

    ASSERT_NE(q.Acquire(), nullptr);
    EXPECT_EQ(q.Acquire()->id, 1u); // front, not removed
    EXPECT_EQ(q.Acquire()->id, 1u);
    q.Release();
    EXPECT_EQ(q.Acquire()->id, 2u);
    q.Release();
    EXPECT_EQ(q.Acquire()->id, 3u);
    q.Release();
    EXPECT_TRUE(q.Empty());
    EXPECT_EQ(q.Acquire(), nullptr);
    EXPECT_EQ(q.PublishedCount(), 3u);
    EXPECT_EQ(q.MaxDepth(), 3u);
}

// --- Overflow is a value outcome leaving the queue unchanged ------------------
TEST(PersistenceQueueStep7, OverflowLeavesStateUnchanged)
{
    persistence::PersistenceQueue q{QueueConfig(2, 0, 0)};
    EXPECT_EQ(q.Publish(MakeRequest(1, 10)), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(q.Publish(MakeRequest(2, 20)), persistence::PersistenceOutcome::Ok);
    EXPECT_TRUE(q.Full());

    EXPECT_EQ(q.Publish(MakeRequest(3, 30)), persistence::PersistenceOutcome::QueueFull);
    EXPECT_EQ(q.Size(), 2u);            // unchanged
    EXPECT_EQ(q.Acquire()->id, 1u);     // front unchanged
    EXPECT_EQ(q.OverflowCount(), 1u);

    // After a release, publishing succeeds again (ring reuse).
    q.Release();
    EXPECT_EQ(q.Publish(MakeRequest(3, 30)), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(q.Size(), 2u);
}

// --- Retry re-enqueues (bounded) and is counted separately --------------------
TEST(PersistenceQueueStep7, RetryReenqueuesAndCounts)
{
    persistence::PersistenceQueue q{QueueConfig(3, 0, 0)};
    EXPECT_EQ(q.Publish(MakeRequest(1, 10)), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(q.Retry(MakeRequest(1, 10)), persistence::PersistenceOutcome::Ok); // re-enqueued
    EXPECT_EQ(q.Size(), 2u);
    EXPECT_EQ(q.RetryCount(), 1u);
    EXPECT_EQ(q.PublishedCount(), 1u); // retry is not a publish

    // Retry is bounded by capacity.
    EXPECT_EQ(q.Publish(MakeRequest(2, 20)), persistence::PersistenceOutcome::Ok);
    EXPECT_TRUE(q.Full());
    EXPECT_EQ(q.Retry(MakeRequest(1, 10)), persistence::PersistenceOutcome::QueueFull);
    EXPECT_EQ(q.Size(), 3u);
}

// --- Back-pressure hysteresis: engage at high, release at low ------------------
TEST(PersistenceQueueStep7, BackpressureHysteresis)
{
    persistence::PersistenceQueue q{QueueConfig(8, /*high*/ 4, /*low*/ 2)};
    EXPECT_FALSE(q.IsBackpressured());

    for (std::uint64_t i = 1; i <= 3; ++i)
    {
        EXPECT_EQ(q.Publish(MakeRequest(i, i * 10)), persistence::PersistenceOutcome::Ok);
    }
    EXPECT_FALSE(q.IsBackpressured());        // depth 3 < high 4

    EXPECT_EQ(q.Publish(MakeRequest(4, 40)), persistence::PersistenceOutcome::Ok);
    EXPECT_TRUE(q.IsBackpressured());         // depth 4 >= high -> engaged

    q.Release();                              // depth 3 (> low, still engaged)
    EXPECT_TRUE(q.IsBackpressured());
    q.Release();                              // depth 2 <= low -> released
    EXPECT_FALSE(q.IsBackpressured());
}

// --- High watermark 0 disables back-pressure ----------------------------------
TEST(PersistenceQueueStep7, BackpressureDisabledWhenHighZero)
{
    persistence::PersistenceQueue q{QueueConfig(4, 0, 0)};
    for (std::uint64_t i = 1; i <= 4; ++i)
    {
        EXPECT_EQ(q.Publish(MakeRequest(i, i)), persistence::PersistenceOutcome::Ok);
    }
    EXPECT_TRUE(q.Full());
    EXPECT_FALSE(q.IsBackpressured()); // never engages when high == 0
}

// ============================================================================
// Step 8 — IPersistenceStore (in-memory + null)
// ============================================================================

// --- In-memory: begin -> write -> commit records a save -----------------------
TEST(PersistenceStoreStep8, InMemoryCommitRecordsSave)
{
    persistence::InMemoryPersistenceStore store;
    const FakeSnapshotView view = MakeSealedView();
    const persistence::PersistenceSnapshot snap{view};
    const auto meta = persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Manual, 7);

    auto begin = store.Begin(meta);
    ASSERT_TRUE(begin.HasValue());
    const persistence::StoreHandle handle = begin.Value();
    EXPECT_TRUE(handle.IsValid());
    EXPECT_EQ(store.PendingCount(), 1u);

    EXPECT_EQ(store.Write(handle, snap), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(store.Commit(handle), persistence::PersistenceOutcome::Ok);

    EXPECT_EQ(store.CommittedCount(), 1u);
    EXPECT_EQ(store.PendingCount(), 0u);
    EXPECT_EQ(store.LastCommitted().saveId, 7u);
    EXPECT_EQ(store.LastCommitted().checksum, meta.checksum);
}

// --- Abort discards the transaction; the previous save is retained ------------
TEST(PersistenceStoreStep8, AbortRetainsPrevious)
{
    persistence::InMemoryPersistenceStore store;
    const FakeSnapshotView view = MakeSealedView();
    const persistence::PersistenceSnapshot snap{view};

    // Commit a first save (the "previous").
    auto b1 = store.Begin(persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Manual, 1));
    ASSERT_TRUE(b1.HasValue());
    EXPECT_EQ(store.Write(b1.Value(), snap), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(store.Commit(b1.Value()), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(store.CommittedCount(), 1u);

    // Begin + abort a second save.
    auto b2 = store.Begin(persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Autosave, 2));
    ASSERT_TRUE(b2.HasValue());
    store.Abort(b2.Value());

    EXPECT_EQ(store.PendingCount(), 0u);
    EXPECT_EQ(store.CommittedCount(), 1u);         // previous retained
    EXPECT_EQ(store.LastCommitted().saveId, 1u);   // still the first save
}

// --- Storage unavailable: Begin fails; write failure => StorageUnavailable -----
TEST(PersistenceStoreStep8, FailureModes)
{
    persistence::InMemoryPersistenceStore store;
    const FakeSnapshotView view = MakeSealedView();
    const persistence::PersistenceSnapshot snap{view};
    const auto meta = persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Shutdown, 5);

    // Unavailable backend -> Begin errors, nothing recorded.
    store.SetAvailable(false);
    EXPECT_FALSE(store.Begin(meta).HasValue());
    EXPECT_EQ(store.PendingCount(), 0u);
    EXPECT_EQ(store.CommittedCount(), 0u);

    // Available again, but writes fail -> StorageUnavailable; commit cannot proceed.
    store.SetAvailable(true);
    store.SetFailWrites(true);
    auto begin = store.Begin(meta);
    ASSERT_TRUE(begin.HasValue());
    EXPECT_EQ(store.Write(begin.Value(), snap), persistence::PersistenceOutcome::StorageUnavailable);
    EXPECT_EQ(store.Commit(begin.Value()), persistence::PersistenceOutcome::IncompleteSnapshot); // nothing written
    EXPECT_EQ(store.CommittedCount(), 0u);

    // Unknown handle is a value outcome, not a crash.
    EXPECT_EQ(store.Write(persistence::StoreHandle{9999}, snap), persistence::PersistenceOutcome::StorageUnavailable);
    EXPECT_EQ(store.Commit(persistence::StoreHandle{9999}), persistence::PersistenceOutcome::StorageUnavailable);
    store.Abort(persistence::StoreHandle{9999}); // benign
}

// --- Null store: accepts and discards everything, records nothing --------------
TEST(PersistenceStoreStep8, NullStoreInert)
{
    persistence::NullPersistenceStore store;
    const FakeSnapshotView view = MakeSealedView();
    const persistence::PersistenceSnapshot snap{view};
    const auto meta = persistence::SaveMetadataBuilder::Build(view, persistence::SaveTrigger::Manual, 3);

    auto begin = store.Begin(meta);
    ASSERT_TRUE(begin.HasValue());
    EXPECT_TRUE(begin.Value().IsValid());
    EXPECT_EQ(store.Write(begin.Value(), snap), persistence::PersistenceOutcome::Ok);
    EXPECT_EQ(store.Commit(begin.Value()), persistence::PersistenceOutcome::Ok);
    store.Abort(begin.Value()); // benign

    // Consume through the interface to confirm the seam is polymorphic.
    persistence::IPersistenceStore& seam = store;
    auto b2 = seam.Begin(meta);
    ASSERT_TRUE(b2.HasValue());
    EXPECT_EQ(seam.Commit(b2.Value()), persistence::PersistenceOutcome::Ok);
}
