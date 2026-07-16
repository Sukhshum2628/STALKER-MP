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
#include "stalkermp/snapshot/SnapshotPool.h"
#include "stalkermp/snapshot/SnapshotBuilder.h"
#include "stalkermp/adapters/EntitySnapshotSource.h"
#include "stalkermp/world/IEntitySnapshotSource.h"
#include "stalkermp/world/IEnvironmentSource.h"
#include "stalkermp/net/Session.h"
#include "stalkermp/player/IPlayerSpawnGateway.h"
#include "stalkermp/player/PlayerManager.h"
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

// ============================================================================
// Step 4 — SnapshotPool (fixed-capacity, exception-free intrusive ref-count)
// ============================================================================

// --- Reserve sets capacity; deterministic acquisition (lowest free index) -----
TEST(SnapshotPoolStep4, DeterministicAcquisitionOrder)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(3);
    EXPECT_EQ(pool.Capacity(), 3u);
    EXPECT_EQ(pool.InUse(), 0u);

    const auto a = pool.Acquire();
    const auto b = pool.Acquire();
    const auto c = pool.Acquire();
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());
    ASSERT_TRUE(c.HasValue());
    EXPECT_EQ(pool.InUse(), 3u);
    // Distinct buffers, each with ref-count 1.
    EXPECT_NE(a.Value(), b.Value());
    EXPECT_NE(b.Value(), c.Value());
    EXPECT_EQ(pool.RefCount(a.Value()), 1u);
}

// --- Capacity exhaustion is a value outcome (PoolExhausted) -------------------
TEST(SnapshotPoolStep4, CapacityExhaustion)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(2);
    ASSERT_TRUE(pool.Acquire().HasValue());
    ASSERT_TRUE(pool.Acquire().HasValue());
    EXPECT_TRUE(pool.Full());
    const auto over = pool.Acquire();
    EXPECT_FALSE(over.HasValue()); // PoolExhausted, not a throw
    EXPECT_EQ(pool.InUse(), 2u);   // unchanged
}

// --- Buffer reuse: a returned buffer is re-acquired (same storage) ------------
TEST(SnapshotPoolStep4, BufferReuse)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(2);
    const auto a = pool.Acquire();
    const auto b = pool.Acquire();
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());
    snapshot::SimulationSnapshot* const first = a.Value();

    pool.ReturnToPool(first);       // ref 1 -> 0 -> freed (slot 0)
    EXPECT_EQ(pool.InUse(), 1u);
    const auto reacquired = pool.Acquire(); // lowest free = slot 0 again
    ASSERT_TRUE(reacquired.HasValue());
    EXPECT_EQ(reacquired.Value(), first); // same underlying buffer reused
    EXPECT_EQ(pool.InUse(), 2u);
}

// --- Reference-count lifecycle: retire only at zero (multi-consumer) ----------
TEST(SnapshotPoolStep4, RefCountLifecycle)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(2);
    const auto acq = pool.Acquire();
    ASSERT_TRUE(acq.HasValue());
    snapshot::SimulationSnapshot* const buf = acq.Value();
    EXPECT_EQ(pool.RefCount(buf), 1u);

    pool.AddRef(buf); // second consumer
    pool.AddRef(buf); // third consumer
    EXPECT_EQ(pool.RefCount(buf), 3u);
    EXPECT_EQ(pool.InUse(), 1u);

    pool.ReturnToPool(buf); // 3 -> 2 : still in use
    EXPECT_EQ(pool.RefCount(buf), 2u);
    EXPECT_EQ(pool.InUse(), 1u);
    pool.ReturnToPool(buf); // 2 -> 1 : still in use
    EXPECT_EQ(pool.InUse(), 1u);
    pool.ReturnToPool(buf); // 1 -> 0 : retired
    EXPECT_EQ(pool.RefCount(buf), 0u);
    EXPECT_EQ(pool.InUse(), 0u);

    // Extra release / unknown / null are benign no-ops.
    pool.ReturnToPool(buf);
    pool.ReturnToPool(nullptr);
    EXPECT_EQ(pool.InUse(), 0u);
}

// --- Acquired pooled buffers are usable SimulationSnapshots -------------------
TEST(SnapshotPoolStep4, PooledBufferIsBuildable)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(1);
    const auto acq = pool.Acquire();
    ASSERT_TRUE(acq.HasValue());
    snapshot::SimulationSnapshot* const s = acq.Value();
    s->BeginBuild(snapshot::SnapshotId{1}, 100, 1);
    ASSERT_TRUE(s->AddEntity(Ent(1)).HasValue());
    ASSERT_TRUE(s->Finalize().HasValue());
    EXPECT_EQ(s->State(), snapshot::SnapshotState::Finalized);
    pool.ReturnToPool(s);
    EXPECT_EQ(pool.InUse(), 0u);
}

// ============================================================================
// Step 5 — world::IEntitySnapshotSource + NullEntitySnapshotSource
// ============================================================================

// --- Factory yields a usable, engine-free source ------------------------------
TEST(EntitySnapshotSourceStep5, FactoryProducesSource)
{
    std::unique_ptr<world::IEntitySnapshotSource> src = adapters::CreateEngineEntitySnapshotSource();
    ASSERT_NE(src, nullptr);
}

// --- Null source captures deterministically (inert, engine-free) --------------
TEST(EntitySnapshotSourceStep5, NullCaptureDeterministic)
{
    std::unique_ptr<world::IEntitySnapshotSource> src = adapters::CreateEngineEntitySnapshotSource();
    std::vector<snapshot::EntitySnapshot> a;
    std::vector<snapshot::EntitySnapshot> b;
    src->Capture(a);
    src->Capture(b);
    EXPECT_EQ(a.size(), b.size()); // same result every call (deterministic)
}

// --- Capture is APPEND-ONLY: pre-existing entries are preserved ---------------
TEST(EntitySnapshotSourceStep5, CaptureIsAppendOnly)
{
    std::unique_ptr<world::IEntitySnapshotSource> src = adapters::CreateEngineEntitySnapshotSource();
    std::vector<snapshot::EntitySnapshot> out;
    snapshot::EntitySnapshot pre{};
    pre.id = world::EntityId{42};
    out.push_back(pre);
    const std::size_t before = out.size();

    src->Capture(out); // null appends nothing; must not clear
    ASSERT_GE(out.size(), before);
    EXPECT_EQ(out[0].id.value, 42u); // pre-existing entry preserved
}

// --- Usable polymorphically through the engine-free seam ----------------------
TEST(EntitySnapshotSourceStep5, UsableThroughInterface)
{
    std::unique_ptr<world::IEntitySnapshotSource> src = adapters::CreateEngineEntitySnapshotSource();
    world::IEntitySnapshotSource& iface = *src; // the SnapshotBuilder consumes this
    std::vector<snapshot::EntitySnapshot> out;
    iface.Capture(out);
    // Result is a valid vector (possibly empty for the inert null), ascending-safe.
    for (std::size_t i = 1; i < out.size(); ++i)
    {
        EXPECT_LT(out[i - 1].id.value, out[i].id.value);
    }
    SUCCEED();
}

// ============================================================================
// Step 6 — SnapshotBuilder (deterministic, value-only capture pass)
// ============================================================================

namespace
{
    // Test entity source: appends `count` ascending value-only entities. The
    // per-entity `state` marker lets a test prove capture copies values.
    class FakeEntitySource final : public world::IEntitySnapshotSource
    {
    public:
        explicit FakeEntitySource(std::uint32_t count) : m_count(count) {}
        void SetCount(std::uint32_t count) { m_count = count; }
        void Capture(std::vector<snapshot::EntitySnapshot>& out) const override
        {
            for (std::uint32_t i = 1; i <= m_count; ++i)
            {
                snapshot::EntitySnapshot e{};
                e.id = world::EntityId{i};
                e.state = i * 10u; // observable value payload
                out.push_back(e);
            }
        }
    private:
        std::uint32_t m_count;
    };

    // Test environment source: a fixed, engine-free sample.
    class FakeEnvSource final : public world::IEnvironmentSource
    {
    public:
        [[nodiscard]] std::optional<world::EnvironmentSample> Sample() const override
        {
            world::EnvironmentSample s{};
            s.weatherName = "clear";
            s.emissionActive = true;
            return s;
        }
    };

    // Minimal deterministic spawn gateway for building a player surface.
    class BuilderSpawnGateway final : public player::IPlayerSpawnGateway
    {
    public:
        [[nodiscard]] core::Expected<world::EntityId> Spawn(const player::PlayerProfile&,
                                                            const world::PlayerPosition&) override
        {
            return world::EntityId{m_next++};
        }
        [[nodiscard]] player::SpawnOutcome Despawn(world::EntityId) override
        {
            return player::SpawnOutcome::Spawned;
        }
    private:
        std::uint32_t m_next = 2000;
    };

    player::PlayerConfiguration BuilderPlayerConfig(std::uint32_t maxPlayers)
    {
        player::PlayerConfiguration c;
        c.maxPlayers = maxPlayers;
        return c;
    }
} // namespace

// --- Successful build lifecycle: BeginBuild -> Capture -> Finalize -------------
TEST(SnapshotBuilderStep6, SuccessfulBuildLifecycle)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(2);
    snapshot::SnapshotBuilder builder{snapshot::SnapshotConfiguration{}};

    FakeEntitySource entities{3};
    FakeEnvSource env;
    BuilderSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager players(BuilderPlayerConfig(8), gw, session);
    ASSERT_EQ(players.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100),
              player::JoinOutcome::Accepted);
    ASSERT_EQ(players.RequestJoin(net::ConnectionId{7}, player::PlayerProfile{}, 100),
              player::JoinOutcome::Accepted);

    const auto begun = builder.BeginBuild(pool, /*tick*/ 500);
    ASSERT_TRUE(begun.HasValue());
    EXPECT_TRUE(builder.Building());
    EXPECT_EQ(builder.LastId().value, 1u); // monotonic, never 0

    ASSERT_TRUE(builder.Capture(entities, players, env).HasValue());

    const auto finalized = builder.Finalize();
    ASSERT_TRUE(finalized.HasValue());
    EXPECT_FALSE(builder.Building());

    const snapshot::ISnapshotView& view = *finalized.Value();
    EXPECT_EQ(view.Metadata().state, snapshot::SnapshotState::Finalized);
    EXPECT_EQ(view.Metadata().simulationTick, 500u);
    EXPECT_EQ(view.Metadata().id.value, 1u);
    ASSERT_EQ(view.Entities().size(), 3u);
    EXPECT_EQ(view.Entities()[0].id.value, 1u);
    EXPECT_EQ(view.Entities()[2].id.value, 3u);
    EXPECT_EQ(view.Players().size(), 2u);
    EXPECT_NE(view.Environment().weatherId, 0u); // "clear" hashed
    EXPECT_EQ(view.Environment().emissionState, 1u);

    pool.ReturnToPool(finalized.Value());
    EXPECT_EQ(pool.InUse(), 0u);
}

// --- Deterministic snapshot generation: identical inputs => identical content --
TEST(SnapshotBuilderStep6, DeterministicSnapshotGeneration)
{
    FakeEnvSource env;
    auto run = [&env](std::vector<std::uint32_t>& entityIds, std::uint32_t& weatherId) {
        snapshot::SnapshotPool pool;
        pool.Reserve(1);
        snapshot::SnapshotBuilder builder{snapshot::SnapshotConfiguration{}};
        FakeEntitySource entities{4};
        BuilderSpawnGateway gw;
        net::Session session(8);
        player::PlayerManager players(BuilderPlayerConfig(8), gw, session);
        (void)players.RequestJoin(net::ConnectionId{3}, player::PlayerProfile{}, 10);

        ASSERT_TRUE(builder.BeginBuild(pool, 7).HasValue());
        ASSERT_TRUE(builder.Capture(entities, players, env).HasValue());
        const auto fin = builder.Finalize();
        ASSERT_TRUE(fin.HasValue());
        for (const auto& e : fin.Value()->Entities()) entityIds.push_back(e.id.value);
        weatherId = fin.Value()->Environment().weatherId;
        // First minted id is always 1 within a fresh builder (monotonic).
        EXPECT_EQ(fin.Value()->Metadata().id.value, 1u);
    };
    std::vector<std::uint32_t> a, b;
    std::uint32_t wa = 0, wb = 0;
    run(a, wa);
    run(b, wb);
    EXPECT_EQ(a, b);
    EXPECT_EQ(wa, wb);
}

// --- Value-only capture: post-capture source mutation cannot affect snapshot ---
TEST(SnapshotBuilderStep6, ValueOnlyCapture)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(1);
    snapshot::SnapshotBuilder builder{snapshot::SnapshotConfiguration{}};
    FakeEntitySource entities{2};
    FakeEnvSource env;
    BuilderSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager players(BuilderPlayerConfig(8), gw, session);

    ASSERT_TRUE(builder.BeginBuild(pool, 1).HasValue());
    ASSERT_TRUE(builder.Capture(entities, players, env).HasValue());
    const auto fin = builder.Finalize();
    ASSERT_TRUE(fin.HasValue());

    ASSERT_EQ(fin.Value()->Entities().size(), 2u);
    // Snapshot holds copied values (the state payload), not a live reference.
    EXPECT_EQ(fin.Value()->Entities()[0].state, 10u);
    EXPECT_EQ(fin.Value()->Entities()[1].state, 20u);

    // Mutating the source afterwards does not change the finalized snapshot.
    entities.SetCount(9);
    EXPECT_EQ(fin.Value()->Entities().size(), 2u);
    pool.ReturnToPool(fin.Value());
}

// --- PoolExhausted is a value outcome (no throw) ------------------------------
TEST(SnapshotBuilderStep6, PoolExhaustedHandling)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(1); // single buffer shared by two builders
    snapshot::SnapshotBuilder first{snapshot::SnapshotConfiguration{}};
    snapshot::SnapshotBuilder second{snapshot::SnapshotConfiguration{}};

    ASSERT_TRUE(first.BeginBuild(pool, 1).HasValue()); // takes the only buffer
    EXPECT_TRUE(pool.Full());

    const auto over = second.BeginBuild(pool, 2);
    EXPECT_FALSE(over.HasValue()); // PoolExhausted, not an exception
    EXPECT_FALSE(second.Building());
    EXPECT_EQ(pool.InUse(), 1u); // unchanged
}

// --- Overflow is a value outcome; the pooled buffer is returned (no orphan) ----
TEST(SnapshotBuilderStep6, OverflowHandling)
{
    snapshot::SnapshotPool pool;
    pool.Reserve(1);
    snapshot::SnapshotConfiguration cfg{};
    cfg.maxEntities = 2; // below the source's entity count
    snapshot::SnapshotBuilder builder{cfg};

    FakeEntitySource entities{3}; // exceeds maxEntities
    FakeEnvSource env;
    BuilderSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager players(BuilderPlayerConfig(8), gw, session);

    ASSERT_TRUE(builder.BeginBuild(pool, 1).HasValue());
    const auto captured = builder.Capture(entities, players, env);
    EXPECT_FALSE(captured.HasValue()); // Overflow value outcome
    EXPECT_FALSE(builder.Building());  // build aborted
    EXPECT_EQ(pool.InUse(), 0u);       // buffer returned to the pool (no orphan)
}
