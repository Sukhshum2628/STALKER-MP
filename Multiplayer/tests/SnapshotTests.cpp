// STALKER-MP — Snapshot subsystem tests (Sprint-008)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers (Architecture §9/§10). Engine-free and OS-free. Types only —
//         no builder, pool, queue, manager, configuration, diagnostics, or seam.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/core/Config.h"
#include "stalkermp/snapshot/ISnapshotView.h"
#include "stalkermp/snapshot/SimulationSnapshot.h"
#include "stalkermp/snapshot/SnapshotConfiguration.h"
#include "stalkermp/snapshot/SnapshotTypes.h"

using namespace stalkermp;

namespace
{
    snapshot::EntitySnapshot Ent(std::uint32_t id)
    {
        snapshot::EntitySnapshot e{};
        e.id = world::EntityId{id};
        return e;
    }
    snapshot::PlayerSnapshot Plr(std::uint32_t id, std::uint32_t entity)
    {
        snapshot::PlayerSnapshot p{};
        p.id = player::PlayerId{id};
        p.entity = world::EntityId{entity};
        return p;
    }
} // namespace

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(SnapshotTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<snapshot::SnapshotKind>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<snapshot::SnapshotState>, std::uint8_t>);
    SUCCEED();
}

// --- SnapshotId: none sentinel + equality -------------------------------------
TEST(SnapshotTypesStep1, SnapshotIdNoneAndEquality)
{
    constexpr snapshot::SnapshotId none{};
    EXPECT_EQ(none.value, 0u);
    EXPECT_TRUE(none.IsNone());
    constexpr snapshot::SnapshotId a{7};
    constexpr snapshot::SnapshotId b{7};
    constexpr snapshot::SnapshotId c{8};
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a.IsNone());
}

// --- POD-style aggregates: trivially copyable, "none"/initial defaults --------
TEST(SnapshotTypesStep1, PodDefaults)
{
    static_assert(std::is_trivially_copyable_v<snapshot::SnapshotMetadata>);
    static_assert(std::is_trivially_copyable_v<snapshot::EntitySnapshot>);
    static_assert(std::is_trivially_copyable_v<snapshot::PlayerSnapshot>);
    static_assert(std::is_trivially_copyable_v<snapshot::EnvironmentSnapshot>);
    static_assert(std::is_trivially_copyable_v<snapshot::SnapshotStatistics>);

    const snapshot::SnapshotMetadata m{};
    EXPECT_EQ(m.id.value, 0u);
    EXPECT_EQ(m.simulationTick, 0u);
    EXPECT_EQ(m.version, 0u);
    EXPECT_EQ(m.entityCount, 0u);
    EXPECT_EQ(m.playerCount, 0u);
    EXPECT_EQ(m.timestampWallClock, 0u);
    EXPECT_EQ(m.kind, snapshot::SnapshotKind::Simulation);
    EXPECT_EQ(m.state, snapshot::SnapshotState::Building);

    const snapshot::EntitySnapshot e{};
    EXPECT_EQ(e.id.value, 0u);
    EXPECT_EQ(e.inventoryRef.value, 0u);
    EXPECT_EQ(e.state, 0u);
    EXPECT_EQ(e.runtimeState, 0u);

    const snapshot::PlayerSnapshot p{};
    EXPECT_EQ(p.id.value, 0u);
    EXPECT_EQ(p.entity.value, 0u);
    EXPECT_EQ(p.connectionState, player::PlayerConnectionState::Connected);

    const snapshot::EnvironmentSnapshot env{};
    EXPECT_EQ(env.simulationTick, 0u);
    EXPECT_EQ(env.environmentVersion, 0u);
}

// --- SnapshotConsistency: healthy by default; any false clears health ---------
TEST(SnapshotTypesStep1, ConsistencyHealth)
{
    snapshot::SnapshotConsistency c{};
    EXPECT_TRUE(c.IsHealthy());
    c.idMonotonic = false;
    EXPECT_FALSE(c.IsHealthy());
    c = snapshot::SnapshotConsistency{};
    c.immutableWhenPublished = false;
    EXPECT_FALSE(c.IsHealthy());
}

// --- Name() helpers are total (incl. out-of-range sentinel) -------------------
TEST(SnapshotTypesStep1, KindNamesTotal)
{
    EXPECT_STREQ(snapshot::SnapshotKindName(snapshot::SnapshotKind::Simulation), "Simulation");
    EXPECT_STREQ(snapshot::SnapshotKindName(snapshot::SnapshotKind::Replication), "Replication");
    EXPECT_STREQ(snapshot::SnapshotKindName(snapshot::SnapshotKind::Persistence), "Persistence");
    EXPECT_STREQ(snapshot::SnapshotKindName(snapshot::SnapshotKind::Thread), "Thread");
    EXPECT_STREQ(snapshot::SnapshotKindName(static_cast<snapshot::SnapshotKind>(200)), "Unknown");
}

TEST(SnapshotTypesStep1, StateNamesTotal)
{
    EXPECT_STREQ(snapshot::SnapshotStateName(snapshot::SnapshotState::Building), "Building");
    EXPECT_STREQ(snapshot::SnapshotStateName(snapshot::SnapshotState::Finalized), "Finalized");
    EXPECT_STREQ(snapshot::SnapshotStateName(snapshot::SnapshotState::Published), "Published");
    EXPECT_STREQ(snapshot::SnapshotStateName(snapshot::SnapshotState::Retired), "Retired");
    EXPECT_STREQ(snapshot::SnapshotStateName(static_cast<snapshot::SnapshotState>(200)), "Unknown");
}

// ============================================================================
// Step 2 — SnapshotConfiguration::FromConfig
// ============================================================================

// --- Missing [snapshot] section => all documented defaults --------------------
TEST(SnapshotConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = snapshot::SnapshotConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.poolCapacity, 4u);
    EXPECT_EQ(c.maxEntities, 4096u);
    EXPECT_EQ(c.maxPlayers, 64u);
    EXPECT_EQ(c.version, 1u);
    EXPECT_EQ(c.queueDepth, 2u);
    EXPECT_EQ(c.buildBudgetTicks, 0u);
}

// --- Each field parses a valid supplied value (override) ----------------------
TEST(SnapshotConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("snapshot", "pool_capacity", "8");
    store.Set("snapshot", "max_entities", "2048");
    store.Set("snapshot", "max_players", "32");
    store.Set("snapshot", "version", "3");
    store.Set("snapshot", "queue_depth", "5");
    store.Set("snapshot", "build_budget_ticks", "10");
    const auto r = snapshot::SnapshotConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.poolCapacity, 8u);
    EXPECT_EQ(c.maxEntities, 2048u);
    EXPECT_EQ(c.maxPlayers, 32u);
    EXPECT_EQ(c.version, 3u);
    EXPECT_EQ(c.queueDepth, 5u);
    EXPECT_EQ(c.buildBudgetTicks, 10u);
}

// --- Invalid per-field values are rejected ------------------------------------
TEST(SnapshotConfigStep2, InvalidValuesRejected)
{
    {
        core::ConfigStore store;
        store.Set("snapshot", "pool_capacity", "1"); // below min 2
        EXPECT_FALSE(snapshot::SnapshotConfiguration::FromConfig(store).HasValue());
    }
    {
        core::ConfigStore store;
        store.Set("snapshot", "version", "0"); // below min 1
        EXPECT_FALSE(snapshot::SnapshotConfiguration::FromConfig(store).HasValue());
    }
    {
        core::ConfigStore store;
        store.Set("snapshot", "max_entities", "-1"); // below min 1
        EXPECT_FALSE(snapshot::SnapshotConfiguration::FromConfig(store).HasValue());
    }
}

// --- Cross-field: queue_depth must be <= pool_capacity ------------------------
TEST(SnapshotConfigStep2, CrossFieldQueueDepthWithinPool)
{
    core::ConfigStore store;
    store.Set("snapshot", "pool_capacity", "3");
    store.Set("snapshot", "queue_depth", "4"); // > pool_capacity
    EXPECT_FALSE(snapshot::SnapshotConfiguration::FromConfig(store).HasValue());

    // Equal is allowed.
    core::ConfigStore ok;
    ok.Set("snapshot", "pool_capacity", "4");
    ok.Set("snapshot", "queue_depth", "4");
    const auto r = snapshot::SnapshotConfiguration::FromConfig(ok);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().queueDepth, 4u);
}

// ============================================================================
// Step 3 — Immutable SimulationSnapshot + ISnapshotView
// ============================================================================

// --- Build -> Finalize: content captured, counts stamped, state Finalized -----
TEST(SimulationSnapshotStep3, BuildAndFinalize)
{
    snapshot::SimulationSnapshot snap;
    snap.BeginBuild(snapshot::SnapshotId{42}, /*tick*/ 1000, /*version*/ 1);
    EXPECT_EQ(snap.State(), snapshot::SnapshotState::Building);

    ASSERT_TRUE(snap.AddEntity(Ent(1)).HasValue());
    ASSERT_TRUE(snap.AddEntity(Ent(5)).HasValue());
    ASSERT_TRUE(snap.AddPlayer(Plr(1, 1)).HasValue());
    snapshot::EnvironmentSnapshot env{};
    env.simulationTick = 1000;
    env.environmentVersion = 7;
    ASSERT_TRUE(snap.SetEnvironment(env).HasValue());

    ASSERT_TRUE(snap.Finalize().HasValue());
    EXPECT_EQ(snap.State(), snapshot::SnapshotState::Finalized);
    EXPECT_TRUE(snap.IsFinalized());

    // Consumer surface (const-only) reflects the captured content.
    const snapshot::ISnapshotView& view = snap;
    EXPECT_EQ(view.Metadata().id.value, 42u);
    EXPECT_EQ(view.Metadata().simulationTick, 1000u);
    EXPECT_EQ(view.Metadata().version, 1u);
    EXPECT_EQ(view.Metadata().entityCount, 2u);
    EXPECT_EQ(view.Metadata().playerCount, 1u);
    ASSERT_EQ(view.Entities().size(), 2u);
    EXPECT_EQ(view.Entities()[0].id.value, 1u);
    EXPECT_EQ(view.Entities()[1].id.value, 5u);
    ASSERT_EQ(view.Players().size(), 1u);
    EXPECT_EQ(view.Players()[0].id.value, 1u);
    EXPECT_EQ(view.Environment().environmentVersion, 7u);
}

// --- Deterministic ordering: non-ascending / duplicate / zero rejected --------
TEST(SimulationSnapshotStep3, AscendingUniqueEnforced)
{
    snapshot::SimulationSnapshot snap;
    snap.BeginBuild(snapshot::SnapshotId{1}, 1, 1);
    ASSERT_TRUE(snap.AddEntity(Ent(3)).HasValue());
    EXPECT_FALSE(snap.AddEntity(Ent(3)).HasValue()); // duplicate
    EXPECT_FALSE(snap.AddEntity(Ent(2)).HasValue()); // out of order
    EXPECT_FALSE(snap.AddEntity(Ent(0)).HasValue()); // none id
    ASSERT_TRUE(snap.AddEntity(Ent(4)).HasValue());  // ascending ok

    ASSERT_TRUE(snap.AddPlayer(Plr(2, 20)).HasValue());
    EXPECT_FALSE(snap.AddPlayer(Plr(2, 21)).HasValue()); // duplicate
    EXPECT_FALSE(snap.AddPlayer(Plr(1, 22)).HasValue()); // out of order

    ASSERT_EQ(snap.Entities().size(), 2u);
    ASSERT_EQ(snap.Players().size(), 1u);
}

// --- Immutability: mutating a Finalized snapshot is rejected (E-G1-S) ---------
TEST(SimulationSnapshotStep3, ImmutableAfterFinalize)
{
    snapshot::SimulationSnapshot snap;
    snap.BeginBuild(snapshot::SnapshotId{1}, 1, 1);
    ASSERT_TRUE(snap.AddEntity(Ent(1)).HasValue());
    ASSERT_TRUE(snap.Finalize().HasValue());

    // Every mutating operation now fails; content is unchanged.
    EXPECT_FALSE(snap.AddEntity(Ent(2)).HasValue());
    EXPECT_FALSE(snap.AddPlayer(Plr(1, 1)).HasValue());
    EXPECT_FALSE(snap.SetEnvironment(snapshot::EnvironmentSnapshot{}).HasValue());
    EXPECT_FALSE(snap.Finalize().HasValue()); // double-finalize rejected
    EXPECT_EQ(snap.Entities().size(), 1u);
    EXPECT_EQ(snap.State(), snapshot::SnapshotState::Finalized);
}

// --- Deterministic replay: identical build sequence => identical content ------
TEST(SimulationSnapshotStep3, DeterministicBuild)
{
    auto build = [](snapshot::SimulationSnapshot& s) {
        s.BeginBuild(snapshot::SnapshotId{7}, 500, 2);
        (void)s.AddEntity(Ent(2));
        (void)s.AddEntity(Ent(9));
        (void)s.AddPlayer(Plr(3, 30));
        snapshot::EnvironmentSnapshot e{};
        e.weatherId = 4;
        (void)s.SetEnvironment(e);
        (void)s.Finalize();
    };
    snapshot::SimulationSnapshot a;
    snapshot::SimulationSnapshot b;
    build(a);
    build(b);
    ASSERT_EQ(a.Entities().size(), b.Entities().size());
    for (std::size_t i = 0; i < a.Entities().size(); ++i)
    {
        EXPECT_EQ(a.Entities()[i].id.value, b.Entities()[i].id.value);
    }
    EXPECT_EQ(a.Metadata().id.value, b.Metadata().id.value);
    EXPECT_EQ(a.Metadata().entityCount, b.Metadata().entityCount);
    EXPECT_EQ(a.Environment().weatherId, b.Environment().weatherId);
    // No wall-clock in content (timestampWallClock is a diagnostic field, unset).
    EXPECT_EQ(a.Metadata().timestampWallClock, b.Metadata().timestampWallClock);
}

// --- BeginBuild restarts a build cycle (owner-driven buffer reuse) -------------
TEST(SimulationSnapshotStep3, BeginBuildResetsForReuse)
{
    snapshot::SimulationSnapshot snap;
    snap.BeginBuild(snapshot::SnapshotId{1}, 1, 1);
    ASSERT_TRUE(snap.AddEntity(Ent(1)).HasValue());
    ASSERT_TRUE(snap.Finalize().HasValue());

    // Restart: prior content is cleared, back to Building (pool reuse in Step 4).
    snap.BeginBuild(snapshot::SnapshotId{2}, 2, 1);
    EXPECT_EQ(snap.State(), snapshot::SnapshotState::Building);
    EXPECT_TRUE(snap.Entities().empty());
    EXPECT_EQ(snap.Metadata().id.value, 2u);
    ASSERT_TRUE(snap.AddEntity(Ent(9)).HasValue());
    ASSERT_TRUE(snap.Finalize().HasValue());
    EXPECT_EQ(snap.Entities().size(), 1u);
    EXPECT_EQ(snap.Entities()[0].id.value, 9u);
}
