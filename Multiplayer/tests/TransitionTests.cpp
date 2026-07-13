// STALKER-MP — ALife Transition Layer tests (Sprint-005)
//
// Step 1: value-type / enum foundation tests ONLY. No manager, no gateway, no
// service — those arrive in later steps. Engine-free (ADR-007 / One Engine
// Boundary): this TU includes no engine headers and instantiates no engine type.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "stalkermp/world/TransitionTypes.h"
#include "stalkermp/world/IAlifeSwitchGateway.h"
#include "stalkermp/adapters/NullAlifeSwitchGateway.h"
#include "stalkermp/world/TransitionIntentStore.h"
#include "stalkermp/world/TransitionManager.h"
#include "stalkermp/world/TransitionManagerService.h"
#include "stalkermp/core/Expected.h"
#include "stalkermp/world/BubbleManager.h"
#include "stalkermp/world/BubbleConfiguration.h"
#include "stalkermp/world/EntityRegistry.h"
#include "stalkermp/world/EntityMetadata.h"
#include "stalkermp/world/ISpatialQueries.h"
#include "stalkermp/world/LocalPlayerPositionSource.h"
#include "stalkermp/adapters/EngineAdapters.h"
#include "stalkermp/core/FrameDispatcher.h"

using namespace stalkermp;
using namespace stalkermp::world;
using stalkermp::adapters::NullAlifeSwitchGateway;

namespace
{
    // Fake spatial source: entities are "inside the bubble" when within radius of
    // a player. Mirrors the Sprint-004 test idiom (anonymous namespace: no ODR
    // clash with BubbleTests' own fakes).
    class TxnSpatialQueries final : public ISpatialQueries
    {
    public:
        struct Entry { EntityId id; Vec3 position; };
        std::vector<Entry> entities;

        std::vector<EntityView> QueryRadius(const Vec3& center, float radius) const override
        {
            std::vector<EntityView> result;
            const float r2 = radius * radius;
            for (const auto& e : entities)
            {
                const float dx = e.position.x - center.x;
                const float dy = e.position.y - center.y;
                const float dz = e.position.z - center.z;
                if (dx * dx + dy * dy + dz * dz <= r2)
                {
                    EntityView v;
                    v.id = e.id;
                    v.position = e.position;
                    result.push_back(v);
                }
            }
            return result;
        }
        std::vector<EntityView> QueryBox(const Vec3&, const Vec3&) const override { return {}; }
        std::optional<Vec3> PositionOf(EntityId id) const override
        {
            for (const auto& e : entities)
            {
                if (e.id == id) return e.position;
            }
            return std::nullopt;
        }
        std::optional<EntityView> NearestEntity(const Vec3&) const override { return std::nullopt; }
    };

    EntityMetadata MakeMeta()
    {
        EntityMetadata md;
        md.category = EntityCategory::Npc;
        md.section = "stalker";
        md.flags = kNoEntityFlags;
        md.creationTick = 1;
        md.spawnSource = EntitySpawnSource::EngineWorld;
        md.state = EntityRegistrationState::Constructed;
        return md;
    }

    void Register(EntityRegistry& registry, const std::uint32_t value)
    {
        const auto r = registry.RegisterEntity(EntityId{value}, "e", MakeMeta());
        ASSERT_TRUE(r.HasValue());
    }
} // namespace

// --- ADR-007 posture: value types are trivial and non-throwing to copy. -------
TEST(TransitionTypesStep1, ValueTypesAreTrivialAndNoThrow)
{
    static_assert(std::is_trivially_copyable<TransitionCommand>::value, "TransitionCommand must be trivially copyable");
    static_assert(std::is_trivially_copyable<TransitionRecord>::value, "TransitionRecord must be trivially copyable");
    static_assert(std::is_trivially_copyable<TransitionStatistics>::value, "TransitionStatistics must be trivially copyable");
    static_assert(std::is_nothrow_default_constructible<TransitionCommand>::value, "TransitionCommand default ctor must be noexcept");
    static_assert(std::is_nothrow_default_constructible<TransitionRecord>::value, "TransitionRecord default ctor must be noexcept");

    SUCCEED();
}

// --- Enum default values are stable (§10/§12). --------------------------------
TEST(TransitionTypesStep1, EnumDefaultsAreStable)
{
    EXPECT_EQ(static_cast<std::uint8_t>(TransitionState::Offline), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(TransitionKind::Activate), 0u);
    EXPECT_EQ(static_cast<std::uint8_t>(TransitionOutcome::Applied), 0u);

    TransitionCommand cmd{};
    EXPECT_EQ(cmd.id.value, 0u);
    EXPECT_EQ(cmd.kind, TransitionKind::Activate);

    TransitionRecord rec{};
    EXPECT_EQ(rec.kind, TransitionKind::Activate);
    EXPECT_EQ(rec.outcome, TransitionOutcome::Applied);
}

// --- Name helpers cover every enumerator (mirrors Sprint-004 style). ----------
TEST(TransitionTypesStep1, EnumNamesAreCorrect)
{
    EXPECT_STREQ(TransitionStateName(TransitionState::Offline), "Offline");
    EXPECT_STREQ(TransitionStateName(TransitionState::PendingOnline), "PendingOnline");
    EXPECT_STREQ(TransitionStateName(TransitionState::Online), "Online");
    EXPECT_STREQ(TransitionStateName(TransitionState::PendingOffline), "PendingOffline");

    EXPECT_STREQ(TransitionKindName(TransitionKind::Activate), "Activate");
    EXPECT_STREQ(TransitionKindName(TransitionKind::Deactivate), "Deactivate");

    EXPECT_STREQ(TransitionOutcomeName(TransitionOutcome::Applied), "Applied");
    EXPECT_STREQ(TransitionOutcomeName(TransitionOutcome::AlreadyInState), "AlreadyInState");
    EXPECT_STREQ(TransitionOutcomeName(TransitionOutcome::EntityMissing), "EntityMissing");
    EXPECT_STREQ(TransitionOutcomeName(TransitionOutcome::Failed), "Failed");
}

// --- Equality semantics for TransitionCommand. --------------------------------
TEST(TransitionTypesStep1, CommandEquality)
{
    TransitionCommand a{EntityId{5}, TransitionKind::Activate};
    TransitionCommand b{EntityId{5}, TransitionKind::Activate};
    TransitionCommand c{EntityId{5}, TransitionKind::Deactivate};
    TransitionCommand d{EntityId{6}, TransitionKind::Activate};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);
}

// --- Total order: ascending EntityId, then kind (determinism, §13). -----------
TEST(TransitionTypesStep1, CommandOrdersAscendingByEntityIdThenKind)
{
    std::vector<TransitionCommand> cmds{
        {EntityId{9}, TransitionKind::Activate},
        {EntityId{2}, TransitionKind::Deactivate},
        {EntityId{2}, TransitionKind::Activate},
        {EntityId{5}, TransitionKind::Activate},
    };

    std::sort(cmds.begin(), cmds.end());

    ASSERT_EQ(cmds.size(), 4u);
    EXPECT_EQ(cmds[0].id.value, 2u);
    EXPECT_EQ(cmds[0].kind, TransitionKind::Activate);   // Activate < Deactivate at equal id
    EXPECT_EQ(cmds[1].id.value, 2u);
    EXPECT_EQ(cmds[1].kind, TransitionKind::Deactivate);
    EXPECT_EQ(cmds[2].id.value, 5u);
    EXPECT_EQ(cmds[3].id.value, 9u);

    EXPECT_TRUE(std::is_sorted(cmds.begin(), cmds.end()));
}

// --- TransitionResult defaults empty at tick 0 (§10). -------------------------
TEST(TransitionTypesStep1, ResultDefaultsEmpty)
{
    TransitionResult result{};
    EXPECT_TRUE(result.broughtOnline.empty());
    EXPECT_TRUE(result.broughtOffline.empty());
    EXPECT_EQ(result.tick, 0u);
}

// --- TransitionStatistics defaults all-zero (§10). ----------------------------
TEST(TransitionTypesStep1, StatisticsDefaultZero)
{
    TransitionStatistics stats{};
    EXPECT_EQ(stats.online, 0u);
    EXPECT_EQ(stats.pendingOnline, 0u);
    EXPECT_EQ(stats.pendingOffline, 0u);
    EXPECT_EQ(stats.offlineTracked, 0u);
    EXPECT_EQ(stats.appliedThisTick, 0u);
    EXPECT_EQ(stats.skippedThisTick, 0u);
    EXPECT_EQ(stats.failedThisTick, 0u);
}

// ============================================================================
// Step 2 — IAlifeSwitchGateway seam + NullAlifeSwitchGateway (engine-free).
// ============================================================================

// --- The seam is abstract and engine-free. ------------------------------------
TEST(TransitionGatewayStep2, InterfaceIsAbstract)
{
    static_assert(std::is_abstract<IAlifeSwitchGateway>::value, "IAlifeSwitchGateway must be abstract");
    static_assert(std::has_virtual_destructor<IAlifeSwitchGateway>::value, "IAlifeSwitchGateway needs a virtual destructor");
    static_assert(std::is_base_of<IAlifeSwitchGateway, NullAlifeSwitchGateway>::value, "Null gateway must implement the seam");
    SUCCEED();
}

// --- Apply returns one outcome per command, in input order. -------------------
TEST(TransitionGatewayStep2, ApplyIsOrderPreservingOnePerCommand)
{
    NullAlifeSwitchGateway gateway;
    const std::vector<TransitionCommand> commands{
        {EntityId{2}, TransitionKind::Activate},
        {EntityId{5}, TransitionKind::Deactivate},
        {EntityId{9}, TransitionKind::Activate},
    };

    const std::vector<TransitionOutcome> outcomes = gateway.Apply(commands);

    ASSERT_EQ(outcomes.size(), commands.size());
    for (const TransitionOutcome outcome : outcomes)
    {
        EXPECT_EQ(outcome, TransitionOutcome::AlreadyInState); // default no-op
    }

    // The applied log mirrors input order exactly.
    ASSERT_EQ(gateway.applied.size(), commands.size());
    EXPECT_EQ(gateway.applied[0].id.value, 2u);
    EXPECT_EQ(gateway.applied[1].id.value, 5u);
    EXPECT_EQ(gateway.applied[2].id.value, 9u);
}

// --- Empty batch yields an empty result, no side effects. ---------------------
TEST(TransitionGatewayStep2, ApplyEmptyBatch)
{
    NullAlifeSwitchGateway gateway;
    const std::vector<TransitionOutcome> outcomes = gateway.Apply({});
    EXPECT_TRUE(outcomes.empty());
    EXPECT_TRUE(gateway.applied.empty());
}

// --- Configurable default outcome (test scripting hook). ----------------------
TEST(TransitionGatewayStep2, ApplyHonorsConfiguredOutcome)
{
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;

    const std::vector<TransitionOutcome> outcomes =
        gateway.Apply({{EntityId{1}, TransitionKind::Activate}});

    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_EQ(outcomes[0], TransitionOutcome::Applied);
}

// --- IsOnline returns seeded values, nullopt for unknown ids. -----------------
TEST(TransitionGatewayStep2, IsOnlineReportsSeededAndUnknown)
{
    NullAlifeSwitchGateway gateway;
    gateway.SetOnline(EntityId{7}, true);
    gateway.SetOnline(EntityId{8}, false);

    const std::optional<bool> online7 = gateway.IsOnline(EntityId{7});
    const std::optional<bool> online8 = gateway.IsOnline(EntityId{8});
    const std::optional<bool> unknown = gateway.IsOnline(EntityId{99});

    ASSERT_TRUE(online7.has_value());
    EXPECT_TRUE(*online7);
    ASSERT_TRUE(online8.has_value());
    EXPECT_FALSE(*online8);
    EXPECT_FALSE(unknown.has_value());
}

// --- Reset clears seeded state and the applied log. ---------------------------
TEST(TransitionGatewayStep2, ResetClearsState)
{
    NullAlifeSwitchGateway gateway;
    gateway.SetOnline(EntityId{3}, true);
    (void)gateway.Apply({{EntityId{3}, TransitionKind::Activate}});
    ASSERT_FALSE(gateway.applied.empty());
    ASSERT_TRUE(gateway.IsOnline(EntityId{3}).has_value());

    gateway.Reset();

    EXPECT_TRUE(gateway.applied.empty());
    EXPECT_FALSE(gateway.IsOnline(EntityId{3}).has_value());
}

// --- Null gateway is usable through the abstract seam (polymorphism). ---------
TEST(TransitionGatewayStep2, UsableThroughInterfacePointer)
{
    NullAlifeSwitchGateway concrete;
    concrete.defaultOutcome = TransitionOutcome::Applied;
    IAlifeSwitchGateway& gateway = concrete;

    const std::vector<TransitionOutcome> outcomes =
        gateway.Apply({{EntityId{4}, TransitionKind::Deactivate}});

    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_EQ(outcomes[0], TransitionOutcome::Applied);
    EXPECT_FALSE(gateway.IsOnline(EntityId{4}).has_value());
}

// ============================================================================
// Step 3 — TransitionIntentStore + TransitionManager core (engine-free).
// ============================================================================

// --- Store: unknown ids are implicitly Offline. -------------------------------
TEST(TransitionIntentStoreStep3, UnknownIsOffline)
{
    TransitionIntentStore store;
    EXPECT_EQ(store.Count(), 0u);
    EXPECT_FALSE(store.Contains(EntityId{1}));
    EXPECT_EQ(store.Get(EntityId{1}), TransitionState::Offline);
    EXPECT_TRUE(store.ValidateConsistency().IsHealthy());
}

// --- Store: insertions keep canonical ascending order regardless of input. ----
TEST(TransitionIntentStoreStep3, InsertionsStayAscending)
{
    TransitionIntentStore store;
    store.Set(EntityId{9}, TransitionState::Online);
    store.Set(EntityId{2}, TransitionState::PendingOnline);
    store.Set(EntityId{5}, TransitionState::Offline);
    store.Set(EntityId{7}, TransitionState::PendingOffline);

    const std::vector<EntityId> ids = store.Entities();
    ASSERT_EQ(ids.size(), 4u);
    EXPECT_EQ(ids[0].value, 2u);
    EXPECT_EQ(ids[1].value, 5u);
    EXPECT_EQ(ids[2].value, 7u);
    EXPECT_EQ(ids[3].value, 9u);
    EXPECT_TRUE(store.ValidateConsistency().IsHealthy());
}

// --- Store: Set upserts in place (no duplicate, ordering preserved). ----------
TEST(TransitionIntentStoreStep3, SetUpsertsInPlace)
{
    TransitionIntentStore store;
    store.Set(EntityId{4}, TransitionState::PendingOnline);
    store.Set(EntityId{4}, TransitionState::Online); // update same id

    EXPECT_EQ(store.Count(), 1u);
    EXPECT_EQ(store.Get(EntityId{4}), TransitionState::Online);
    EXPECT_TRUE(store.ValidateConsistency().IsHealthy());
}

// --- Store: Erase returns an entity to implicit Offline, keeps consistency. ----
TEST(TransitionIntentStoreStep3, EraseRemovesAndStaysConsistent)
{
    TransitionIntentStore store;
    store.Set(EntityId{2}, TransitionState::Online);
    store.Set(EntityId{5}, TransitionState::Online);
    store.Set(EntityId{9}, TransitionState::Online);

    store.Erase(EntityId{5});

    EXPECT_EQ(store.Count(), 2u);
    EXPECT_FALSE(store.Contains(EntityId{5}));
    EXPECT_EQ(store.Get(EntityId{5}), TransitionState::Offline);
    const std::vector<EntityId> ids = store.Entities();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0].value, 2u);
    EXPECT_EQ(ids[1].value, 9u);
    EXPECT_TRUE(store.ValidateConsistency().IsHealthy());

    store.Erase(EntityId{999}); // erasing an unknown id is a no-op
    EXPECT_EQ(store.Count(), 2u);
}

// --- Store: accelerator stays consistent with the canonical vector. -----------
TEST(TransitionIntentStoreStep3, AcceleratorConsistentAfterMutations)
{
    TransitionIntentStore store;
    for (std::uint32_t v : {6u, 1u, 3u, 8u, 2u})
    {
        store.Set(EntityId{v}, TransitionState::PendingOnline);
    }
    store.Erase(EntityId{3});
    store.Set(EntityId{1}, TransitionState::Online);

    const TransitionStoreReport report = store.ValidateConsistency();
    EXPECT_TRUE(report.sortedUnique);
    EXPECT_TRUE(report.acceleratorConsistent);
    EXPECT_EQ(store.Get(EntityId{1}), TransitionState::Online);
}

// --- Manager: constructs against real empty deps + null gateway. --------------
TEST(TransitionManagerStep3, ConstructsAndStartsEmpty)
{
    BubbleManager bubble{BubbleConfiguration{}};
    EntityRegistry registry;
    NullAlifeSwitchGateway gateway;

    TransitionManager manager{&bubble, &registry, &gateway};

    EXPECT_EQ(manager.TrackedCount(), 0u);
    EXPECT_EQ(manager.StateOf(EntityId{1}), TransitionState::Offline);
    EXPECT_EQ(manager.Bubble(), &bubble);
    EXPECT_EQ(manager.Registry(), &registry);
    EXPECT_EQ(manager.Gateway(), &gateway);
}

// --- Manager: default construction is valid (all deps null). ------------------
TEST(TransitionManagerStep3, DefaultConstructionIsValid)
{
    TransitionManager manager;
    EXPECT_EQ(manager.TrackedCount(), 0u);
    EXPECT_EQ(manager.StateOf(EntityId{42}), TransitionState::Offline);
    EXPECT_EQ(manager.Bubble(), nullptr);
    EXPECT_EQ(manager.Registry(), nullptr);
    EXPECT_EQ(manager.Gateway(), nullptr);
}

// --- Manager: LastResult is empty at tick 0 until Update runs (later step). ----
TEST(TransitionManagerStep3, LastResultEmptyBeforeUpdate)
{
    TransitionManager manager;
    const TransitionResult& result = manager.LastResult();
    EXPECT_TRUE(result.broughtOnline.empty());
    EXPECT_TRUE(result.broughtOffline.empty());
    EXPECT_EQ(result.tick, 0u);
}

// --- Manager: ValidateConsistency passes on an empty manager. -----------------
TEST(TransitionManagerStep3, ValidateConsistencyHealthyWhenEmpty)
{
    TransitionManager manager;
    EXPECT_TRUE(manager.ValidateConsistency().IsHealthy());
}

// ============================================================================
// Step 4 — Update ingestion + dedup (ordered command batch). Engine-free.
// Drives a real BubbleManager (fake spatial + local player source) to produce
// deterministic Activations()/Deactivations(); no gateway apply, no result.
// ============================================================================

namespace
{
    // Build a bubble whose entities sit at the origin (inside the default radius).
    struct BubbleRig
    {
        TxnSpatialQueries spatial;
        LocalPlayerPositionSource players;
        BubbleManager bubble{BubbleConfiguration{}, &spatial, &players};

        void SeedEntity(std::uint32_t value)
        {
            spatial.entities.push_back({EntityId{value}, Vec3{0.0f, 0.0f, 0.0f}});
        }
        void PlayerAt(float x, std::uint64_t tick)
        {
            (void)players.SetPlayer(PlayerId{1}, Vec3{x, 0.0f, 0.0f}, tick);
        }
    };
} // namespace

// --- Activation edges become one ascending batch; intent -> PendingOnline. -----
TEST(TransitionIngestStep4, ActivationEdgesProduceAscendingBatch)
{
    BubbleRig rig;
    rig.SeedEntity(9);
    rig.SeedEntity(2);
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u); // entities 2,5,9 cross inward -> activations

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 5);
    Register(registry, 9);
    NullAlifeSwitchGateway gateway;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    const auto& batch = manager.StagedBatch();
    ASSERT_EQ(batch.size(), 3u);
    EXPECT_EQ(batch[0].id.value, 2u);
    EXPECT_EQ(batch[1].id.value, 5u);
    EXPECT_EQ(batch[2].id.value, 9u);
    for (const auto& c : batch) EXPECT_EQ(c.kind, TransitionKind::Activate);
    EXPECT_TRUE(std::is_sorted(batch.begin(), batch.end()));

    // Intent advanced to PendingOnline (internal bookkeeping only).
    EXPECT_EQ(manager.StateOf(EntityId{2}), TransitionState::PendingOnline);
    EXPECT_EQ(manager.StateOf(EntityId{9}), TransitionState::PendingOnline);
    EXPECT_EQ(manager.LastTick(), 1u);

    // Step 5 builds the TransitionResult.
    EXPECT_TRUE(manager.LastResult().broughtOnline.empty()); // AlreadyInState
    EXPECT_EQ(manager.LastResult().tick, 1u);
}

// --- Entities not in the registry are dropped (no command). --------------------
TEST(TransitionIngestStep4, UnregisteredEntitiesDropped)
{
    BubbleRig rig;
    rig.SeedEntity(7);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u); // activation for 7

    EntityRegistry registry; // 7 is NOT registered
    NullAlifeSwitchGateway gateway;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    EXPECT_TRUE(manager.StagedBatch().empty());
    EXPECT_EQ(manager.StateOf(EntityId{7}), TransitionState::Offline);
}

// --- Dedup: re-ingesting the same activation state emits no duplicate. ---------
TEST(TransitionIngestStep4, ActivationDedupAcrossRepeatedUpdates)
{
    BubbleRig rig;
    rig.SeedEntity(3);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u); // activation for 3

    EntityRegistry registry;
    Register(registry, 3);
    NullAlifeSwitchGateway gateway;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);
    ASSERT_EQ(manager.StagedBatch().size(), 1u); // first: one Activate

    // Bubble state unchanged; intent is now PendingOnline -> no duplicate.
    manager.Update(2u);
    EXPECT_TRUE(manager.StagedBatch().empty());
    EXPECT_EQ(manager.StateOf(EntityId{3}), TransitionState::PendingOnline);
}

// --- Deactivation edge stages once, then dedups. -------------------------------
TEST(TransitionIngestStep4, DeactivationEdgeThenDedup)
{
    BubbleRig rig;
    rig.SeedEntity(4);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u); // activate 4

    EntityRegistry registry;
    Register(registry, 4);
    NullAlifeSwitchGateway gateway;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // stages Activate{4}, intent PendingOnline
    ASSERT_EQ(manager.StagedBatch().size(), 1u);

    // Move the player far away so entity 4 leaves the bubble -> deactivation.
    rig.PlayerAt(10000.0f, 2u);
    rig.bubble.Update(2u);

    manager.Update(2u); // deactivation edge
    ASSERT_EQ(manager.StagedBatch().size(), 1u);
    EXPECT_EQ(manager.StagedBatch()[0].id.value, 4u);
    EXPECT_EQ(manager.StagedBatch()[0].kind, TransitionKind::Deactivate);
    EXPECT_EQ(manager.StateOf(EntityId{4}), TransitionState::PendingOffline);

    // Bubble deactivation state unchanged -> dedup, no duplicate.
    manager.Update(3u);
    EXPECT_TRUE(manager.StagedBatch().empty());
    EXPECT_EQ(manager.StateOf(EntityId{4}), TransitionState::PendingOffline);
}

// --- Null Bubble dependency: Update is a deterministic no-op. -------------------
TEST(TransitionIngestStep4, NullBubbleIsNoOp)
{
    TransitionManager manager; // all deps null
    manager.Update(42u);
    EXPECT_TRUE(manager.StagedBatch().empty());
    EXPECT_EQ(manager.LastTick(), 42u);
    EXPECT_EQ(manager.TrackedCount(), 0u);
}

// ============================================================================
// Step 5 — Gateway apply + outcome recording + TransitionResult. Engine-free.
// No confirmation, no IsOnline(), no Pending* -> final advancement (that is Step 6).
// ============================================================================

// --- One outcome per staged command, parallel to staged order. -----------------
TEST(TransitionApplyStep5, RecordsOneOutcomePerCommandInOrder)
{
    BubbleRig rig;
    rig.SeedEntity(9);
    rig.SeedEntity(2);
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 5);
    Register(registry, 9);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    const auto& records = manager.LastRecords();
    ASSERT_EQ(records.size(), manager.StagedBatch().size());
    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(records[0].id.value, 2u);
    EXPECT_EQ(records[1].id.value, 5u);
    EXPECT_EQ(records[2].id.value, 9u);
    for (const auto& r : records)
    {
        EXPECT_EQ(r.kind, TransitionKind::Activate);
        EXPECT_EQ(r.outcome, TransitionOutcome::Applied);
    }
}

// --- Applied activations become broughtOnline (ascending); result tick set. ----
TEST(TransitionApplyStep5, ResultListsAppliedActivationsAscending)
{
    BubbleRig rig;
    rig.SeedEntity(2);
    rig.SeedEntity(5);
    rig.SeedEntity(9);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 5);
    Register(registry, 9);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(7u);

    const TransitionResult& result = manager.LastResult();
    ASSERT_EQ(result.broughtOnline.size(), 3u);
    EXPECT_EQ(result.broughtOnline[0].value, 2u);
    EXPECT_EQ(result.broughtOnline[1].value, 5u);
    EXPECT_EQ(result.broughtOnline[2].value, 9u);
    EXPECT_TRUE(result.broughtOffline.empty());
    EXPECT_EQ(result.tick, 7u);
}

// --- AlreadyInState is a no-op: recorded, but NOT a delta. Intent unchanged. ----
TEST(TransitionApplyStep5, AlreadyInStateIsRecordedButNotCounted)
{
    BubbleRig rig;
    rig.SeedEntity(3);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 3);
    NullAlifeSwitchGateway gateway; // default outcome is AlreadyInState
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    ASSERT_EQ(manager.LastRecords().size(), 1u);
    EXPECT_EQ(manager.LastRecords()[0].outcome, TransitionOutcome::AlreadyInState);
    EXPECT_TRUE(manager.LastResult().broughtOnline.empty());
    EXPECT_TRUE(manager.LastResult().broughtOffline.empty());
    // Step 5 does not advance/confirm: intent stays PendingOnline (from staging).
    EXPECT_EQ(manager.StateOf(EntityId{3}), TransitionState::PendingOnline);
}

// --- Failed is recorded, not counted, and does not mutate intent (Step 6 job). --
TEST(TransitionApplyStep5, FailedIsRecordedNotCountedIntentUnchanged)
{
    BubbleRig rig;
    rig.SeedEntity(4);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 4);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Failed;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    ASSERT_EQ(manager.LastRecords().size(), 1u);
    EXPECT_EQ(manager.LastRecords()[0].outcome, TransitionOutcome::Failed);
    EXPECT_TRUE(manager.LastResult().broughtOnline.empty());
    EXPECT_EQ(manager.StateOf(EntityId{4}), TransitionState::PendingOnline);
}

// --- Applied deactivations become broughtOffline. ------------------------------
TEST(TransitionApplyStep5, AppliedDeactivationGoesToBroughtOffline)
{
    BubbleRig rig;
    rig.SeedEntity(4);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 4);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // activate 4 -> broughtOnline
    ASSERT_EQ(manager.LastResult().broughtOnline.size(), 1u);

    rig.PlayerAt(10000.0f, 2u);
    rig.bubble.Update(2u);
    manager.Update(2u); // deactivate 4 -> broughtOffline

    const TransitionResult& result = manager.LastResult();
    EXPECT_TRUE(result.broughtOnline.empty());
    ASSERT_EQ(result.broughtOffline.size(), 1u);
    EXPECT_EQ(result.broughtOffline[0].value, 4u);
    EXPECT_EQ(result.tick, 2u);
}

// --- Null gateway: batch still staged (Step 4), but nothing applied. -----------
TEST(TransitionApplyStep5, NullGatewayAppliesNothing)
{
    BubbleRig rig;
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 5);
    TransitionManager manager{&rig.bubble, &registry, nullptr}; // no gateway

    manager.Update(3u);

    EXPECT_EQ(manager.StagedBatch().size(), 1u);   // Step 4 still stages
    EXPECT_TRUE(manager.LastRecords().empty());     // Step 5 applied nothing
    EXPECT_TRUE(manager.LastResult().broughtOnline.empty());
    EXPECT_EQ(manager.LastResult().tick, 3u);
}

// --- Step 5 never confirms: Applied does not advance Pending* to final. ---------
TEST(TransitionApplyStep5, DoesNotConfirmPendingStates)
{
    BubbleRig rig;
    rig.SeedEntity(6);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 6);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    // Even though the Activate was Applied, intent must remain PendingOnline —
    // confirmation to Online is exclusively Step 6.
    EXPECT_EQ(manager.StateOf(EntityId{6}), TransitionState::PendingOnline);
}

// ============================================================================
// Step 6 — Reconciliation via IsOnline() read-back. Engine-free.
// Confirms Pending* -> final; no Apply, no staging, no retry/timeout.
// ============================================================================

// --- PendingOnline confirms to Online when IsOnline() == true. -----------------
TEST(TransitionReconcileStep6, PendingOnlineConfirmsWhenOnlineTrue)
{
    BubbleRig rig;
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 5);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // stage + apply -> PendingOnline
    ASSERT_EQ(manager.StateOf(EntityId{5}), TransitionState::PendingOnline);

    gateway.SetOnline(EntityId{5}, true); // engine now reports online
    manager.Update(2u);                   // reconcile at start of tick 2

    EXPECT_EQ(manager.StateOf(EntityId{5}), TransitionState::Online);
}

// --- PendingOffline confirms to Offline when IsOnline() == false. ---------------
TEST(TransitionReconcileStep6, PendingOfflineConfirmsWhenOnlineFalse)
{
    BubbleRig rig;
    rig.SeedEntity(4);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 4);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // PendingOnline
    rig.PlayerAt(10000.0f, 2u);
    rig.bubble.Update(2u);
    manager.Update(2u); // deactivate -> PendingOffline
    ASSERT_EQ(manager.StateOf(EntityId{4}), TransitionState::PendingOffline);

    gateway.SetOnline(EntityId{4}, false); // engine now reports offline
    manager.Update(3u);                    // reconcile

    EXPECT_EQ(manager.StateOf(EntityId{4}), TransitionState::Offline);
}

// --- nullopt leaves the entity in its existing Pending* state. -----------------
TEST(TransitionReconcileStep6, NulloptLeavesPending)
{
    BubbleRig rig;
    rig.SeedEntity(3);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 3);
    NullAlifeSwitchGateway gateway; // IsOnline() returns nullopt (never seeded)
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // PendingOnline
    manager.Update(2u); // reconcile sees nullopt -> unchanged

    EXPECT_EQ(manager.StateOf(EntityId{3}), TransitionState::PendingOnline);
}

// --- Mismatch: PendingOnline + IsOnline()==false stays PendingOnline. -----------
TEST(TransitionReconcileStep6, PendingOnlineWithFalseStaysPending)
{
    BubbleRig rig;
    rig.SeedEntity(8);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 8);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // PendingOnline
    gateway.SetOnline(EntityId{8}, false); // engine still reports offline
    manager.Update(2u);

    EXPECT_EQ(manager.StateOf(EntityId{8}), TransitionState::PendingOnline);
}

// --- Reconciliation issues no Apply and stages nothing. ------------------------
TEST(TransitionReconcileStep6, ReconciliationIssuesNoApplyOrStaging)
{
    BubbleRig rig;
    rig.SeedEntity(2);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // one Activate applied
    const std::size_t appliedAfterTick1 = gateway.applied.size();
    ASSERT_EQ(appliedAfterTick1, 1u);

    // Tick 2: bubble state unchanged (no new edges). Reconcile confirms online,
    // but must not stage or apply anything new.
    gateway.SetOnline(EntityId{2}, true);
    manager.Update(2u);

    EXPECT_EQ(manager.StateOf(EntityId{2}), TransitionState::Online);
    EXPECT_TRUE(manager.StagedBatch().empty());           // nothing staged
    EXPECT_EQ(gateway.applied.size(), appliedAfterTick1); // no new Apply commands
}

// --- Deterministic: multiple Pending* entities all confirm (ascending pass). ----
TEST(TransitionReconcileStep6, MultiplePendingConfirmDeterministically)
{
    BubbleRig rig;
    rig.SeedEntity(9);
    rig.SeedEntity(2);
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 5);
    Register(registry, 9);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u); // all three PendingOnline
    gateway.SetOnline(EntityId{2}, true);
    gateway.SetOnline(EntityId{5}, true);
    gateway.SetOnline(EntityId{9}, true);
    manager.Update(2u);

    EXPECT_EQ(manager.StateOf(EntityId{2}), TransitionState::Online);
    EXPECT_EQ(manager.StateOf(EntityId{5}), TransitionState::Online);
    EXPECT_EQ(manager.StateOf(EntityId{9}), TransitionState::Online);
    EXPECT_TRUE(manager.ValidateConsistency().IsHealthy());
}

// --- Null gateway: reconciliation is a deterministic no-op. ---------------------
TEST(TransitionReconcileStep6, NullGatewayReconcileIsNoOp)
{
    BubbleRig rig;
    rig.SeedEntity(7);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 7);
    TransitionManager manager{&rig.bubble, &registry, nullptr}; // no gateway

    manager.Update(1u); // stages, marks PendingOnline (no apply)
    manager.Update(2u); // reconcile no-op (no gateway)

    EXPECT_EQ(manager.StateOf(EntityId{7}), TransitionState::PendingOnline);
}

// ============================================================================
// Step 7 — Diagnostics + extended ValidateConsistency. Read-only, deterministic.
// ============================================================================

// --- Statistics: empty manager reports all zero. -------------------------------
TEST(TransitionDiagnosticsStep7, StatisticsEmpty)
{
    TransitionManager manager;
    const TransitionStatistics s = manager.Statistics();
    EXPECT_EQ(s.online, 0u);
    EXPECT_EQ(s.pendingOnline, 0u);
    EXPECT_EQ(s.pendingOffline, 0u);
    EXPECT_EQ(s.offlineTracked, 0u);
    EXPECT_EQ(s.appliedThisTick, 0u);
    EXPECT_EQ(s.skippedThisTick, 0u);
    EXPECT_EQ(s.failedThisTick, 0u);
}

// --- Statistics: intent tallies + this tick's applied outcomes. ----------------
TEST(TransitionDiagnosticsStep7, StatisticsCountsPendingAndApplied)
{
    BubbleRig rig;
    rig.SeedEntity(2);
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 5);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    const TransitionStatistics s = manager.Statistics();
    EXPECT_EQ(s.pendingOnline, 2u);       // both staged -> PendingOnline
    EXPECT_EQ(s.online, 0u);
    EXPECT_EQ(s.appliedThisTick, 2u);     // both Applied this tick
    EXPECT_EQ(s.skippedThisTick, 0u);
    EXPECT_EQ(s.failedThisTick, 0u);
}

// --- Statistics: skipped and failed outcome tallies. ---------------------------
TEST(TransitionDiagnosticsStep7, StatisticsSkippedAndFailed)
{
    BubbleRig rig1;
    rig1.SeedEntity(3);
    rig1.PlayerAt(0.0f, 1u);
    rig1.bubble.Update(1u);
    EntityRegistry reg1;
    Register(reg1, 3);
    NullAlifeSwitchGateway gwSkip; // default AlreadyInState -> skipped
    TransitionManager m1{&rig1.bubble, &reg1, &gwSkip};
    m1.Update(1u);
    EXPECT_EQ(m1.Statistics().skippedThisTick, 1u);
    EXPECT_EQ(m1.Statistics().appliedThisTick, 0u);

    BubbleRig rig2;
    rig2.SeedEntity(4);
    rig2.PlayerAt(0.0f, 1u);
    rig2.bubble.Update(1u);
    EntityRegistry reg2;
    Register(reg2, 4);
    NullAlifeSwitchGateway gwFail;
    gwFail.defaultOutcome = TransitionOutcome::Failed;
    TransitionManager m2{&rig2.bubble, &reg2, &gwFail};
    m2.Update(1u);
    EXPECT_EQ(m2.Statistics().failedThisTick, 1u);
}

// --- DescribeState / DumpPending are non-empty and deterministic. --------------
TEST(TransitionDiagnosticsStep7, DescribeAndDumpDeterministic)
{
    BubbleRig rig;
    rig.SeedEntity(5);
    rig.SeedEntity(2);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 5);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);

    const std::string describe = manager.DescribeState();
    EXPECT_NE(describe.find("tick=1"), std::string::npos);
    EXPECT_NE(describe.find("pendingOnline=2"), std::string::npos);
    EXPECT_EQ(describe, manager.DescribeState()); // deterministic / read-only

    // DumpPending lists both entities ascending.
    const std::string dump = manager.DumpPending();
    const auto p2 = dump.find(" 2=PendingOnline");
    const auto p5 = dump.find(" 5=PendingOnline");
    EXPECT_NE(p2, std::string::npos);
    EXPECT_NE(p5, std::string::npos);
    EXPECT_LT(p2, p5); // ascending order
}

// --- Diagnostics do not mutate state. ------------------------------------------
TEST(TransitionDiagnosticsStep7, DiagnosticsAreReadOnly)
{
    BubbleRig rig;
    rig.SeedEntity(6);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 6);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);
    const TransitionState before = manager.StateOf(EntityId{6});
    const std::size_t trackedBefore = manager.TrackedCount();

    (void)manager.Statistics();
    (void)manager.DescribeState();
    (void)manager.DumpPending();
    (void)manager.ValidateConsistency();

    EXPECT_EQ(manager.StateOf(EntityId{6}), before);
    EXPECT_EQ(manager.TrackedCount(), trackedBefore);
}

// --- ValidateConsistency healthy across a full activate/confirm cycle. ---------
TEST(TransitionDiagnosticsStep7, ValidateConsistencyHealthyAcrossCycle)
{
    BubbleRig rig;
    rig.SeedEntity(2);
    rig.SeedEntity(9);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 2);
    Register(registry, 9);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManager manager{&rig.bubble, &registry, &gateway};

    manager.Update(1u);
    EXPECT_TRUE(manager.ValidateConsistency().IsHealthy()); // staged+records+result consistent

    gateway.SetOnline(EntityId{2}, true);
    gateway.SetOnline(EntityId{9}, true);
    manager.Update(2u); // reconcile confirms online; new staged batch empty
    const TransitionConsistencyReport report = manager.ValidateConsistency();
    EXPECT_TRUE(report.IsHealthy());
    EXPECT_TRUE(report.store.IsHealthy());
    EXPECT_TRUE(report.statesValid);
    EXPECT_TRUE(report.stagedBatchOrdered);
    EXPECT_TRUE(report.stagedIntentConsistent);
    EXPECT_TRUE(report.recordsMatchBatch);
    EXPECT_TRUE(report.resultConsistent);
    EXPECT_EQ(manager.StateOf(EntityId{2}), TransitionState::Online);
}

// ============================================================================
// Step 8 — TransitionManagerService (IService + ITickable). Engine-free.
// Lifecycle + per-frame tick only; NO dispatcher subscription, NO Bootstrap.
// ============================================================================

// --- Identity and dependency contract (Architecture §14). ----------------------
TEST(TransitionServiceStep8, NameAndDependencies)
{
    TransitionManagerService service; // all deps null — valid construction
    EXPECT_EQ(service.Name(), std::string_view{"TransitionManager"});

    const std::vector<std::string> deps = service.Dependencies();
    ASSERT_EQ(deps.size(), 3u);
    EXPECT_EQ(deps[0], "World");
    EXPECT_EQ(deps[1], "EntityRegistry");
    EXPECT_EQ(deps[2], "BubbleManager");
}

// --- Initialize succeeds; Shutdown is safe and idempotent. ---------------------
TEST(TransitionServiceStep8, InitializeAndShutdown)
{
    TransitionManagerService service;
    EXPECT_TRUE(service.Initialize().HasValue());
    service.Shutdown();
    service.Shutdown(); // idempotent, no crash
    SUCCEED();
}

// --- Tick advances the manager once per call (monotonic tick). -----------------
TEST(TransitionServiceStep8, TickAdvancesMonotonically)
{
    TransitionManagerService service; // null deps -> Update is a no-op but ticks
    EXPECT_EQ(service.Manager().LastTick(), 0u);
    service.Tick(0.016);
    service.Tick(0.016);
    service.Tick(0.016);
    EXPECT_EQ(service.Manager().LastTick(), 3u);
    EXPECT_TRUE(service.Manager().StagedBatch().empty()); // null bubble -> no work
}

// --- Service delegates a real pipeline tick to the owned manager. --------------
TEST(TransitionServiceStep8, TickDrivesTheManager)
{
    BubbleRig rig;
    rig.SeedEntity(5);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u); // bubble activation for 5

    EntityRegistry registry;
    Register(registry, 5);
    NullAlifeSwitchGateway gateway;
    gateway.defaultOutcome = TransitionOutcome::Applied;
    TransitionManagerService service{&rig.bubble, &registry, &gateway};

    service.Tick(0.016); // -> manager.Update(1)

    EXPECT_EQ(service.Manager().LastTick(), 1u);
    ASSERT_EQ(service.Manager().StagedBatch().size(), 1u);
    EXPECT_EQ(service.Manager().StagedBatch()[0].id.value, 5u);
    EXPECT_EQ(service.Manager().StateOf(EntityId{5}), TransitionState::PendingOnline);
    EXPECT_TRUE(service.Manager().ValidateConsistency().IsHealthy());
}

// --- IService/ITickable polymorphism works through base pointers. ---------------
TEST(TransitionServiceStep8, UsableThroughBaseInterfaces)
{
    TransitionManagerService service;
    core::IService& asService = service;
    core::ITickable& asTickable = service;

    EXPECT_EQ(asService.Name(), std::string_view{"TransitionManager"});
    EXPECT_TRUE(asService.Initialize().HasValue());
    asTickable.Tick(0.016);
    EXPECT_EQ(service.Manager().LastTick(), 1u);
    asService.Shutdown();
}

// ============================================================================
// Step 9 — Engine gateway seam. The real EngineAlifeSwitchGateway is engine-
// touching (EngineAdapters.cpp) and is verified at runtime by Antigravity; the
// engine-free test build links the null adapter. These tests cover the parts
// observable without the engine: the additive tick-order constant and the
// factory contract (One Engine Boundary — test build uses the null adapter).
// ============================================================================

// --- Additive tick-order constant is correctly placed. -------------------------
TEST(TransitionEngineGatewayStep9, TickOrderConstantPlacement)
{
    EXPECT_EQ(core::tick_order::kAlifeTransition, 350u);
    EXPECT_LT(core::tick_order::kBubble, core::tick_order::kAlifeTransition);
    EXPECT_LT(core::tick_order::kAlifeTransition, core::tick_order::kReplication);
}

// --- The gateway factory yields a usable IAlifeSwitchGateway. -------------------
// In the engine-free test build this resolves to the null adapter (One Engine
// Boundary preserved: no engine TU linked). The manager depends only on the seam.
TEST(TransitionEngineGatewayStep9, FactoryProducesUsableGateway)
{
    std::unique_ptr<IAlifeSwitchGateway> gateway =
        adapters::CreateEngineAlifeSwitchGateway();
    ASSERT_NE(gateway, nullptr);

    const std::vector<TransitionOutcome> outcomes =
        gateway->Apply({{EntityId{1}, TransitionKind::Activate},
                        {EntityId{2}, TransitionKind::Deactivate}});
    EXPECT_EQ(outcomes.size(), 2u);                 // one outcome per command, order preserved
    EXPECT_FALSE(gateway->IsOnline(EntityId{1}).has_value()); // unknown in the null build
}

// --- The manager drives a factory-produced gateway through the seam. -----------
TEST(TransitionEngineGatewayStep9, ManagerUsesFactoryGatewayThroughSeam)
{
    BubbleRig rig;
    rig.SeedEntity(3);
    rig.PlayerAt(0.0f, 1u);
    rig.bubble.Update(1u);

    EntityRegistry registry;
    Register(registry, 3);
    std::unique_ptr<IAlifeSwitchGateway> gateway =
        adapters::CreateEngineAlifeSwitchGateway();
    TransitionManager manager{&rig.bubble, &registry, gateway.get()};

    manager.Update(1u);

    ASSERT_EQ(manager.StagedBatch().size(), 1u);
    EXPECT_EQ(manager.StagedBatch()[0].id.value, 3u);
    EXPECT_EQ(manager.StateOf(EntityId{3}), TransitionState::PendingOnline);
    EXPECT_TRUE(manager.ValidateConsistency().IsHealthy());
}
