#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/world/EntityRegistry.h"
#include "stalkermp/world/EntityRegistryService.h"
#include "stalkermp/world/EntityRegistryConfig.h"

using namespace stalkermp;
using namespace stalkermp::world;

namespace
{
    EntityMetadata MakeTestMetadata(EntityCategory cat = EntityCategory::Npc, std::string section = "stalker")
    {
        EntityMetadata md;
        md.category = cat;
        md.section = std::move(section);
        md.flags = kNoEntityFlags;
        md.creationTick = 10;
        md.spawnSource = EntitySpawnSource::EngineWorld;
        md.state = EntityRegistrationState::Constructed;
        return md;
    }
} // namespace

TEST(EntityRegistryTests, BasicRegistrationAndLookup)
{
    EntityRegistry registry;
    EXPECT_EQ(registry.Size(), 0u);

    EntityId id1{100};
    auto handleResult = registry.RegisterEntity(id1, "NPC_1", MakeTestMetadata(EntityCategory::Npc));
    ASSERT_TRUE(handleResult.HasValue());
    EntityHandle h1 = handleResult.Value();

    EXPECT_EQ(registry.Size(), 1u);
    EXPECT_TRUE(registry.Contains(id1));

    // Lookup by ID
    const EntityRecord* record = registry.FindByEntityId(id1);
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->name, "NPC_1");
    EXPECT_EQ(record->handle, h1);

    // Lookup by Handle
    const EntityRecord* recordByH = registry.FindByHandle(h1);
    ASSERT_NE(recordByH, nullptr);
    EXPECT_EQ(recordByH->name, "NPC_1");

    // Invalid handle checks
    EntityHandle staleHandle{id1, h1.generation + 1};
    EXPECT_EQ(registry.FindByHandle(staleHandle), nullptr);
}

TEST(EntityRegistryTests, ReserveAndRestoreEntity)
{
    EntityRegistry registry;

    // Reserve Entity ID
    auto res1 = registry.ReserveEntity();
    ASSERT_TRUE(res1.HasValue());
    EXPECT_GE(res1.Value().value, 0x10000u);

    auto res2 = registry.ReserveEntity();
    ASSERT_TRUE(res2.HasValue());
    EXPECT_NE(res1.Value(), res2.Value());

    // Restore Entity
    EntityId idRestored{500};
    auto restoreResult = registry.RestoreEntity(idRestored, 3, "Restored_1", MakeTestMetadata(EntityCategory::Monster, "bloodsucker"));
    ASSERT_TRUE(restoreResult.HasValue());
    EntityHandle hRestored = restoreResult.Value();
    EXPECT_EQ(hRestored.id, idRestored);
    EXPECT_EQ(hRestored.generation, 3u);

    // Generation tracking update check - must unregister first before reuse
    (void)registry.UnregisterEntity(hRestored);
    auto regResult = registry.RegisterEntity(idRestored, "New_1", MakeTestMetadata());
    ASSERT_TRUE(regResult.HasValue());
    EXPECT_EQ(regResult.Value().generation, 4u); // Increments from restored 3
}

TEST(EntityRegistryTests, SearchAndIterationAPIs)
{
    EntityRegistry registry;
    (void)registry.RegisterEntity(EntityId{10}, "NPC_A", MakeTestMetadata(EntityCategory::Npc, "stalker"));
    (void)registry.RegisterEntity(EntityId{20}, "NPC_B", MakeTestMetadata(EntityCategory::Monster, "dog"));
    (void)registry.RegisterEntity(EntityId{5}, "NPC_C", MakeTestMetadata(EntityCategory::Npc, "bandit"));

    // Verification of ascending EntityId order
    // Order in registry should be: 5, 10, 20
    std::vector<EntityId> visitedIds;
    registry.ForEach([&visitedIds](const EntityRecord& record) {
        visitedIds.push_back(record.Id());
    });
    ASSERT_EQ(visitedIds.size(), 3u);
    EXPECT_EQ(visitedIds[0].value, 5u);
    EXPECT_EQ(visitedIds[1].value, 10u);
    EXPECT_EQ(visitedIds[2].value, 20u);

    // FindByName
    const auto* found = registry.FindByName("NPC_A");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->Id().value, 10u);

    // FindByType
    auto npcs = registry.FindByType(EntityCategory::Npc);
    ASSERT_EQ(npcs.size(), 2u);
    EXPECT_EQ(npcs[0].id.value, 5u); // 5 comes first in canonical order
    EXPECT_EQ(npcs[1].id.value, 10u);

    // FindBySection
    auto stalkers = registry.FindBySection("stalker");
    ASSERT_EQ(stalkers.size(), 1u);
    EXPECT_EQ(stalkers[0].id.value, 10u);

    // ForEachOfType
    std::vector<EntityId> npcVisited;
    registry.ForEachOfType(EntityCategory::Npc, [&npcVisited](const EntityRecord& rec) {
        npcVisited.push_back(rec.Id());
    });
    ASSERT_EQ(npcVisited.size(), 2u);
    EXPECT_EQ(npcVisited[0].value, 5u);
    EXPECT_EQ(npcVisited[1].value, 10u);
}

TEST(EntityRegistryTests, EventSubscriptions)
{
    EntityRegistry registry;
    std::vector<EntityId> registeredEventIds;
    std::vector<EntityId> unregisteredEventIds;
    std::vector<EntityId> restoredEventIds;

    auto subReg = registry.SubscribeOnRegistered([&registeredEventIds](const EntityRecord& rec) {
        registeredEventIds.push_back(rec.Id());
    });
    ASSERT_TRUE(subReg.HasValue());

    auto subUnreg = registry.SubscribeOnUnregistered([&unregisteredEventIds](const EntityRecord& rec) {
        unregisteredEventIds.push_back(rec.Id());
    });
    ASSERT_TRUE(subUnreg.HasValue());

    auto subRestored = registry.SubscribeOnRestored([&restoredEventIds](const EntityRecord& rec) {
        restoredEventIds.push_back(rec.Id());
    });
    ASSERT_TRUE(subRestored.HasValue());

    // Register triggers event
    auto h1 = registry.RegisterEntity(EntityId{1}, "E1", MakeTestMetadata()).Value();
    ASSERT_EQ(registeredEventIds.size(), 1u);
    EXPECT_EQ(registeredEventIds[0].value, 1u);

    // Restore triggers event
    auto h2 = registry.RestoreEntity(EntityId{2}, 1, "E2", MakeTestMetadata()).Value();
    ASSERT_EQ(restoredEventIds.size(), 1u);
    EXPECT_EQ(restoredEventIds[0].value, 2u);

    // Unregister triggers event
    (void)registry.UnregisterEntity(h1);
    ASSERT_EQ(unregisteredEventIds.size(), 1u);
    EXPECT_EQ(unregisteredEventIds[0].value, 1u);

    // Unsubscribe check
    EXPECT_TRUE(registry.UnsubscribeOnRegistered(subReg.Value()).HasValue());
    (void)registry.RegisterEntity(EntityId{3}, "E3", MakeTestMetadata());
    EXPECT_EQ(registeredEventIds.size(), 1u); // No new event recorded because unsubscribed
}

TEST(EntityRegistryTests, EventNonReentrancy)
{
    EntityRegistry registry;
    bool mutationFailed = false;

    (void)registry.SubscribeOnRegistered([&registry, &mutationFailed](const EntityRecord&) {
        // Attempting to register another entity during event dispatch should fail
        auto result = registry.RegisterEntity(EntityId{2}, "Nested", MakeTestMetadata());
        if (!result.HasValue() && result.GetError().Code() == core::ErrorCode::Internal)
        {
            mutationFailed = true;
        }
    });

    (void)registry.RegisterEntity(EntityId{1}, "Root", MakeTestMetadata());
    EXPECT_TRUE(mutationFailed);
}

TEST(EntityRegistryTests, ComputeStatistics)
{
    EntityRegistry registry;
    
    // Initial stats
    auto stats = registry.ComputeStatistics();
    EXPECT_EQ(stats.entityCount, 0u);
    EXPECT_EQ(stats.npcCount, 0u);
    EXPECT_EQ(stats.monsterCount, 0u);
    EXPECT_EQ(stats.itemCount, 0u);
    EXPECT_EQ(stats.vehicleCount, 0u);
    EXPECT_EQ(stats.playerCount, 0u);
    EXPECT_EQ(stats.dynamicSpawnCount, 0u);
    EXPECT_EQ(stats.destroyedCount, 0u);

    // Register a mix
    auto h1 = registry.RegisterEntity(EntityId{1}, "P1", MakeTestMetadata(EntityCategory::Player)).Value();
    (void)registry.RegisterEntity(EntityId{2}, "M1", MakeTestMetadata(EntityCategory::Monster));
    
    EntityMetadata dynamicMetadata = MakeTestMetadata(EntityCategory::Item);
    dynamicMetadata.spawnSource = EntitySpawnSource::Dynamic;
    (void)registry.RegisterEntity(EntityId{3}, "I1", dynamicMetadata);

    stats = registry.ComputeStatistics();
    EXPECT_EQ(stats.entityCount, 3u);
    EXPECT_EQ(stats.playerCount, 1u);
    EXPECT_EQ(stats.monsterCount, 1u);
    EXPECT_EQ(stats.itemCount, 1u);
    EXPECT_EQ(stats.dynamicSpawnCount, 1u);
    EXPECT_EQ(stats.destroyedCount, 0u);

    // Destroy one
    (void)registry.UnregisterEntity(h1);
    stats = registry.ComputeStatistics();
    EXPECT_EQ(stats.entityCount, 2u);
    EXPECT_EQ(stats.playerCount, 0u);
    EXPECT_EQ(stats.destroyedCount, 1u);

    // Clear resets destroyedCount
    registry.Clear();
    stats = registry.ComputeStatistics();
    EXPECT_EQ(stats.entityCount, 0u);
    EXPECT_EQ(stats.destroyedCount, 0u);
}

TEST(EntityRegistryTests, DebugUtilities)
{
    EntityRegistry registry;
    
    // Classify NullHandle
    EXPECT_EQ(registry.ClassifyHandle(EntityHandle{}), HandleValidity::NullHandle);

    // Register a valid entity
    auto h1 = registry.RegisterEntity(EntityId{1}, "NPC_A", MakeTestMetadata(EntityCategory::Npc)).Value();
    EXPECT_EQ(registry.ClassifyHandle(h1), HandleValidity::Valid);

    // Unknown ID
    EXPECT_EQ(registry.ClassifyHandle(EntityHandle{EntityId{999}, 1}), HandleValidity::UnknownId);

    // Stale Generation
    EXPECT_EQ(registry.ClassifyHandle(EntityHandle{EntityId{1}, 5}), HandleValidity::StaleGeneration);

    // Inspect entity
    auto inspectResult = registry.InspectEntity(h1);
    ASSERT_TRUE(inspectResult.HasValue());
    EXPECT_NE(inspectResult.Value().find("NPC_A"), std::string::npos);

    // Inspect non-existent
    auto inspectFail = registry.InspectEntity(EntityHandle{EntityId{2}, 1});
    EXPECT_FALSE(inspectFail.HasValue());
    EXPECT_EQ(inspectFail.GetError().Code(), core::ErrorCode::NotFound);

    // Dump registry
    std::string dump = registry.DumpRegistry();
    EXPECT_NE(dump.find("NPC_A"), std::string::npos);

    // Validate integrity
    auto report = registry.ValidateIntegrity();
    EXPECT_TRUE(report.invalidRecords.empty());
    EXPECT_TRUE(report.duplicateIds.empty());
    EXPECT_TRUE(report.outOfOrder.empty());
    EXPECT_TRUE(report.generationMismatches.empty());
}

TEST(EntityRegistryTests, ServiceWrapper)
{
    EntityRegistryConfig config;
    config.debugLogging = false;
    EntityRegistryService service(config);

    EXPECT_EQ(service.Name(), "EntityRegistry");
    EXPECT_TRUE(service.Dependencies().empty());

    // Registry initially empty
    EXPECT_EQ(service.Registry().Size(), 0u);

    // Initialize service
    auto initRes = service.Initialize();
    ASSERT_TRUE(initRes.HasValue());

    // Register a test record
    (void)service.Registry().RegisterEntity(EntityId{1}, "Test", MakeTestMetadata());
    EXPECT_EQ(service.Registry().Size(), 1u);

    // Shutdown service clears the registry
    service.Shutdown();
    EXPECT_EQ(service.Registry().Size(), 0u);
}

TEST(EntityRegistryTests, RegistrationLifecycleTransitions)
{
    EntityRegistry registry;

    auto handleResult = registry.RegisterEntity(EntityId{100}, "npc", MakeTestMetadata());
    ASSERT_TRUE(handleResult.HasValue());
    const EntityHandle handle = handleResult.Value();

    // New records enter Registered.
    auto state0 = registry.GetRegistrationState(handle);
    ASSERT_TRUE(state0.HasValue());
    EXPECT_EQ(state0.Value(), EntityRegistrationState::Registered);

    // Legal forward path: Registered -> Initialized -> Active.
    EXPECT_TRUE(registry.SetRegistrationState(handle, EntityRegistrationState::Initialized).HasValue());
    EXPECT_TRUE(registry.SetRegistrationState(handle, EntityRegistrationState::Active).HasValue());

    auto stateActive = registry.GetRegistrationState(handle);
    ASSERT_TRUE(stateActive.HasValue());
    EXPECT_EQ(stateActive.Value(), EntityRegistrationState::Active);

    // Illegal: Active may only advance to PendingRemoval.
    EXPECT_FALSE(registry.SetRegistrationState(handle, EntityRegistrationState::Initialized).HasValue());

    // Legal: Active -> PendingRemoval, which is terminal (no further transition).
    EXPECT_TRUE(registry.SetRegistrationState(handle, EntityRegistrationState::PendingRemoval).HasValue());
    EXPECT_FALSE(registry.SetRegistrationState(handle, EntityRegistrationState::Active).HasValue());

    // Stale handle -> failure; null/invalid handle -> failure.
    const EntityHandle stale{EntityId{100}, handle.generation + 1};
    EXPECT_FALSE(registry.SetRegistrationState(stale, EntityRegistrationState::Initialized).HasValue());
    EXPECT_FALSE(registry.GetRegistrationState(kNullEntityHandle).HasValue());
}

TEST(EntityRegistryTests, ForEachActiveVisitsOnlyActiveInOrder)
{
    EntityRegistry registry;
    ASSERT_TRUE(registry.RegisterEntity(EntityId{30}, "a", MakeTestMetadata()).HasValue());
    ASSERT_TRUE(registry.RegisterEntity(EntityId{10}, "b", MakeTestMetadata()).HasValue());
    ASSERT_TRUE(registry.RegisterEntity(EntityId{20}, "c", MakeTestMetadata()).HasValue());

    auto activate = [&registry](EntityId id) {
        const EntityRecord* record = registry.FindByEntityId(id);
        ASSERT_NE(record, nullptr);
        ASSERT_TRUE(registry.SetRegistrationState(record->handle, EntityRegistrationState::Initialized).HasValue());
        ASSERT_TRUE(registry.SetRegistrationState(record->handle, EntityRegistrationState::Active).HasValue());
    };
    activate(EntityId{30});
    activate(EntityId{10});
    // 20 is left in the Registered state.

    std::vector<std::uint32_t> visited;
    registry.ForEachActive([&visited](const EntityRecord& record) {
        visited.push_back(record.handle.id.value);
    });

    // Only the two Active records, in canonical ascending-EntityId order (I3).
    ASSERT_EQ(visited.size(), 2u);
    EXPECT_EQ(visited[0], 10u);
    EXPECT_EQ(visited[1], 30u);
}

TEST(EntityRegistryTests, StressManyEntitiesDeterministicOrder)
{
    EntityRegistry registry;
    constexpr std::uint32_t kCount = 3000;

    // Register in descending order to exercise sorted insertion.
    for (std::uint32_t i = kCount; i >= 1; --i)
    {
        ASSERT_TRUE(registry.RegisterEntity(EntityId{i}, "e", MakeTestMetadata()).HasValue());
    }
    EXPECT_EQ(registry.Size(), static_cast<std::size_t>(kCount));

    // Canonical iteration is strictly ascending by EntityId (I3).
    std::uint32_t previous = 0;
    bool strictlyAscending = true;
    registry.ForEach([&](const EntityRecord& record) {
        if (record.handle.id.value <= previous)
        {
            strictlyAscending = false;
        }
        previous = record.handle.id.value;
    });
    EXPECT_TRUE(strictlyAscending);

    // Lookup correctness across the range.
    EXPECT_TRUE(registry.Contains(EntityId{1}));
    EXPECT_TRUE(registry.Contains(EntityId{kCount}));
    EXPECT_NE(registry.FindByEntityId(EntityId{kCount / 2}), nullptr);
    EXPECT_EQ(registry.FindByEntityId(EntityId{kCount + 1}), nullptr);

    // Integrity holds at scale.
    EXPECT_TRUE(registry.ValidateIntegrity().IsHealthy());

    // Unregister the even ids; size halves and integrity/order hold.
    for (std::uint32_t i = 2; i <= kCount; i += 2)
    {
        const EntityRecord* record = registry.FindByEntityId(EntityId{i});
        ASSERT_NE(record, nullptr);
        EXPECT_TRUE(registry.UnregisterEntity(record->handle).HasValue());
    }
    EXPECT_EQ(registry.Size(), static_cast<std::size_t>(kCount / 2));
    EXPECT_TRUE(registry.ValidateIntegrity().IsHealthy());
}
