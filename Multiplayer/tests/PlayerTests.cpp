// STALKER-MP — Player subsystem tests (Sprint-007)
//
// Step 1: value types, enumerations, POD structures, and const char* name
//         helpers (Architecture §9).
// Step 2: PlayerConfiguration::FromConfig (defaults, overrides, validation,
//         cross-field rules, tick-based durations).
// Engine-free and OS-free.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/adapters/PlayerSpawnGateway.h"
#include "stalkermp/core/Config.h"
#include "stalkermp/net/MessageRegistry.h"
#include "stalkermp/net/Session.h"
#include "stalkermp/player/PlayerManagerService.h"
#include "stalkermp/player/IPlayerSpawnGateway.h"
#include "stalkermp/player/PlayerConfiguration.h"
#include "stalkermp/player/PlayerDeltaQueue.h"
#include "stalkermp/player/PlayerLifecycle.h"
#include "stalkermp/player/PlayerDiagnostics.h"
#include "stalkermp/player/PlayerManager.h"
#include "stalkermp/player/NetworkedPlayerPositionSource.h"
#include "stalkermp/player/PlayerRegistry.h"
#include "stalkermp/world/IPlayerPositionSource.h"
#include "stalkermp/player/PlayerSessionObserver.h"
#include "stalkermp/player/PlayerTypes.h"

using namespace stalkermp;

// ============================================================================
// Step 1 — Player value types
// ============================================================================

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(PlayerTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<player::PlayerLifecycleState>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<player::PlayerConnectionState>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<player::DisconnectDisposition>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<player::JoinOutcome>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<player::SpawnOutcome>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<player::ReconnectOutcome>, std::uint8_t>);
    SUCCEED();
}

// --- PlayerId is REUSED from world::PlayerId (not redefined) ------------------
TEST(PlayerTypesStep1, PlayerIdIsWorldPlayerIdAlias)
{
    static_assert(std::is_same_v<player::PlayerId, world::PlayerId>);
    constexpr player::PlayerId none{};
    EXPECT_EQ(none.value, 0u); // 0 = "none"
}

// --- POD records default to the "none"/initial values -------------------------
TEST(PlayerTypesStep1, PlayerRecordDefaults)
{
    const player::PlayerRecord r{};
    EXPECT_EQ(r.id.value, 0u);
    EXPECT_EQ(r.connection.value, 0u);
    EXPECT_EQ(r.sessionMember, 0u);
    EXPECT_EQ(r.entity.value, 0u);
    EXPECT_EQ(r.lifecycle, player::PlayerLifecycleState::Joining);
    EXPECT_EQ(r.connectionState, player::PlayerConnectionState::Connected);
    EXPECT_EQ(r.joinTick, 0u);
    EXPECT_EQ(r.respawnEligibleTick, 0u);
    EXPECT_EQ(r.reconnectToken, 0u);
    EXPECT_EQ(r.lastPosition.id.value, 0u);
    static_assert(std::is_trivially_copyable_v<player::PlayerRecord>);
}

TEST(PlayerTypesStep1, PlayerStatisticsDefaults)
{
    const player::PlayerStatistics s{};
    EXPECT_EQ(s.connected, 0u);
    EXPECT_EQ(s.suspended, 0u);
    EXPECT_EQ(s.dead, 0u);
    EXPECT_EQ(s.respawns, 0u);
    EXPECT_EQ(s.deaths, 0u);
    EXPECT_EQ(s.reconnects, 0u);
    EXPECT_EQ(s.joinTick, 0u);
    EXPECT_EQ(s.averageSessionDurationTicks, 0u);
    static_assert(std::is_trivially_copyable_v<player::PlayerStatistics>);
}

// --- PlayerMappingView equality ----------------------------------------------
TEST(PlayerTypesStep1, PlayerMappingViewEquality)
{
    player::PlayerMappingView a{};
    a.id = player::PlayerId{7};
    a.connection = net::ConnectionId{3};
    a.sessionMember = 42;
    a.entity = world::EntityId{9};

    player::PlayerMappingView b = a;
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    b.entity = world::EntityId{10};
    EXPECT_TRUE(a != b);
    EXPECT_FALSE(a == b);
}

// --- Name() helpers are total: every enumerator -> stable non-null string -----
TEST(PlayerTypesStep1, LifecycleStateNamesTotal)
{
    EXPECT_STREQ(player::PlayerLifecycleStateName(player::PlayerLifecycleState::Joining), "Joining");
    EXPECT_STREQ(player::PlayerLifecycleStateName(player::PlayerLifecycleState::Active), "Active");
    EXPECT_STREQ(player::PlayerLifecycleStateName(player::PlayerLifecycleState::Dead), "Dead");
    EXPECT_STREQ(player::PlayerLifecycleStateName(player::PlayerLifecycleState::AwaitingRespawn), "AwaitingRespawn");
    EXPECT_STREQ(player::PlayerLifecycleStateName(player::PlayerLifecycleState::Suspended), "Suspended");
    EXPECT_STREQ(player::PlayerLifecycleStateName(player::PlayerLifecycleState::Removed), "Removed");
    EXPECT_STREQ(player::PlayerLifecycleStateName(static_cast<player::PlayerLifecycleState>(200)), "Unknown");
}

TEST(PlayerTypesStep1, ConnectionStateNamesTotal)
{
    EXPECT_STREQ(player::PlayerConnectionStateName(player::PlayerConnectionState::Connected), "Connected");
    EXPECT_STREQ(player::PlayerConnectionStateName(player::PlayerConnectionState::Suspended), "Suspended");
    EXPECT_STREQ(player::PlayerConnectionStateName(player::PlayerConnectionState::Reclaimed), "Reclaimed");
    EXPECT_STREQ(player::PlayerConnectionStateName(static_cast<player::PlayerConnectionState>(200)), "Unknown");
}

TEST(PlayerTypesStep1, DisconnectDispositionNamesTotal)
{
    EXPECT_STREQ(player::DisconnectDispositionName(player::DisconnectDisposition::Retain), "Retain");
    EXPECT_STREQ(player::DisconnectDispositionName(player::DisconnectDisposition::Remove), "Remove");
    EXPECT_STREQ(player::DisconnectDispositionName(static_cast<player::DisconnectDisposition>(200)), "Unknown");
}

TEST(PlayerTypesStep1, JoinOutcomeNamesTotal)
{
    EXPECT_STREQ(player::JoinOutcomeName(player::JoinOutcome::Accepted), "Accepted");
    EXPECT_STREQ(player::JoinOutcomeName(player::JoinOutcome::RejectedCapacity), "RejectedCapacity");
    EXPECT_STREQ(player::JoinOutcomeName(player::JoinOutcome::RejectedDuplicate), "RejectedDuplicate");
    EXPECT_STREQ(player::JoinOutcomeName(player::JoinOutcome::RejectedInvalid), "RejectedInvalid");
    EXPECT_STREQ(player::JoinOutcomeName(player::JoinOutcome::RejectedSpawnFailed), "RejectedSpawnFailed");
    EXPECT_STREQ(player::JoinOutcomeName(static_cast<player::JoinOutcome>(200)), "Unknown");
}

TEST(PlayerTypesStep1, SpawnOutcomeNamesTotal)
{
    EXPECT_STREQ(player::SpawnOutcomeName(player::SpawnOutcome::Spawned), "Spawned");
    EXPECT_STREQ(player::SpawnOutcomeName(player::SpawnOutcome::EntityMissing), "EntityMissing");
    EXPECT_STREQ(player::SpawnOutcomeName(player::SpawnOutcome::EngineUnavailable), "EngineUnavailable");
    EXPECT_STREQ(player::SpawnOutcomeName(player::SpawnOutcome::RejectedInvalid), "RejectedInvalid");
    EXPECT_STREQ(player::SpawnOutcomeName(static_cast<player::SpawnOutcome>(200)), "Unknown");
}

TEST(PlayerTypesStep1, ReconnectOutcomeNamesTotal)
{
    EXPECT_STREQ(player::ReconnectOutcomeName(player::ReconnectOutcome::Reclaimed), "Reclaimed");
    EXPECT_STREQ(player::ReconnectOutcomeName(player::ReconnectOutcome::TokenUnknown), "TokenUnknown");
    EXPECT_STREQ(player::ReconnectOutcomeName(player::ReconnectOutcome::AlreadyActive), "AlreadyActive");
    EXPECT_STREQ(player::ReconnectOutcomeName(player::ReconnectOutcome::RejectedInvalid), "RejectedInvalid");
    EXPECT_STREQ(player::ReconnectOutcomeName(static_cast<player::ReconnectOutcome>(200)), "Unknown");
}

// ============================================================================
// Step 2 — PlayerConfiguration::FromConfig
// ============================================================================

// --- Missing [player] section => all documented defaults ----------------------
TEST(PlayerConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = player::PlayerConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.maxPlayers, 32u);
    EXPECT_EQ(c.respawnDelayTicks, 300u);
    EXPECT_EQ(c.reconnectRetentionTicks, 3600u);
    EXPECT_EQ(c.spawnPolicyId, 0u);
}

// --- Each field parses a valid supplied value (override) ----------------------
TEST(PlayerConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("player", "max_players", "64");
    store.Set("player", "respawn_delay_ticks", "150");
    store.Set("player", "reconnect_retention_ticks", "7200");
    store.Set("player", "spawn_policy_id", "3");
    const auto r = player::PlayerConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.maxPlayers, 64u);
    EXPECT_EQ(c.respawnDelayTicks, 150u);
    EXPECT_EQ(c.reconnectRetentionTicks, 7200u);
    EXPECT_EQ(c.spawnPolicyId, 3u);
}

// --- Zero durations are legal (immediate respawn / no retention) --------------
TEST(PlayerConfigStep2, ZeroDurationsAccepted)
{
    core::ConfigStore store;
    store.Set("player", "respawn_delay_ticks", "0");
    store.Set("player", "reconnect_retention_ticks", "0");
    const auto r = player::PlayerConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().respawnDelayTicks, 0u);
    EXPECT_EQ(r.Value().reconnectRetentionTicks, 0u);
}

// --- Invalid value => InvalidArgument, no value -------------------------------
TEST(PlayerConfigStep2, InvalidMaxPlayersRejected)
{
    core::ConfigStore store;
    store.Set("player", "max_players", "0"); // must be >= 1
    const auto r = player::PlayerConfiguration::FromConfig(store);
    EXPECT_FALSE(r.HasValue());
}

TEST(PlayerConfigStep2, NegativeValueRejected)
{
    core::ConfigStore store;
    store.Set("player", "respawn_delay_ticks", "-1"); // below min 0
    const auto r = player::PlayerConfiguration::FromConfig(store);
    EXPECT_FALSE(r.HasValue());
}

// --- Cross-field rule: maxPlayers >= 1 (present section) -----------------------
TEST(PlayerConfigStep2, CrossFieldMaxPlayersAtLeastOne)
{
    core::ConfigStore store;
    store.Set("player", "max_players", "1");
    const auto r = player::PlayerConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().maxPlayers, 1u);
}

// --- Unknown non-zero spawn_policy_id is accepted (deferred resolution) --------
TEST(PlayerConfigStep2, UnknownSpawnPolicyIdAccepted)
{
    core::ConfigStore store;
    store.Set("player", "spawn_policy_id", "99");
    const auto r = player::PlayerConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().spawnPolicyId, 99u);
}

// ============================================================================
// Step 3 — PlayerRegistry storage
// ============================================================================

namespace
{
    player::PlayerRecord MakeRecord(std::uint32_t id)
    {
        player::PlayerRecord r{};
        r.id = player::PlayerId{id};
        return r;
    }
} // namespace

// --- Allocation is ascending and never returns 0 ------------------------------
TEST(PlayerRegistryStep3, AllocateAscendingNonZero)
{
    player::PlayerRegistry reg(8);
    const player::PlayerId a = reg.Allocate();
    const player::PlayerId b = reg.Allocate();
    const player::PlayerId c = reg.Allocate();
    EXPECT_NE(a.value, 0u);
    EXPECT_LT(a.value, b.value);
    EXPECT_LT(b.value, c.value);
}

// --- Allocation is non-reused even after retire -------------------------------
TEST(PlayerRegistryStep3, AllocateNonReusedAfterRetire)
{
    player::PlayerRegistry reg(8);
    const player::PlayerId a = reg.Allocate();
    ASSERT_TRUE(reg.Insert(MakeRecord(a.value)).HasValue());
    reg.Retire(a);
    const player::PlayerId b = reg.Allocate();
    EXPECT_GT(b.value, a.value); // id never reused
}

// --- Insert keeps the vector sorted-unique ascending --------------------------
TEST(PlayerRegistryStep3, InsertMaintainsAscendingOrder)
{
    player::PlayerRegistry reg(8);
    // Insert out of order; storage must remain ascending.
    ASSERT_TRUE(reg.Insert(MakeRecord(5)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeRecord(2)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeRecord(9)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeRecord(1)).HasValue());
    const auto& recs = reg.Records();
    ASSERT_EQ(recs.size(), 4u);
    EXPECT_EQ(recs[0].id.value, 1u);
    EXPECT_EQ(recs[1].id.value, 2u);
    EXPECT_EQ(recs[2].id.value, 5u);
    EXPECT_EQ(recs[3].id.value, 9u);
}

// --- Duplicate-id insert is rejected, no state change -------------------------
TEST(PlayerRegistryStep3, DuplicateInsertRejected)
{
    player::PlayerRegistry reg(8);
    ASSERT_TRUE(reg.Insert(MakeRecord(3)).HasValue());
    EXPECT_FALSE(reg.Insert(MakeRecord(3)).HasValue());
    EXPECT_EQ(reg.size(), 1u);
}

// --- Inserting id 0 (none) is rejected ----------------------------------------
TEST(PlayerRegistryStep3, ZeroIdInsertRejected)
{
    player::PlayerRegistry reg(8);
    EXPECT_FALSE(reg.Insert(MakeRecord(0)).HasValue());
    EXPECT_EQ(reg.size(), 0u);
}

// --- Capacity is enforced; over-capacity rejected with no partial insert ------
TEST(PlayerRegistryStep3, CapacityEnforced)
{
    player::PlayerRegistry reg(2);
    ASSERT_TRUE(reg.Insert(MakeRecord(1)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeRecord(2)).HasValue());
    EXPECT_FALSE(reg.Insert(MakeRecord(3)).HasValue());
    EXPECT_EQ(reg.size(), 2u);
    EXPECT_EQ(reg.capacity(), 2u);
}

// --- Retire removes; retire-absent is a no-op ---------------------------------
TEST(PlayerRegistryStep3, RetireRemovesAndAbsentIsNoOp)
{
    player::PlayerRegistry reg(8);
    ASSERT_TRUE(reg.Insert(MakeRecord(4)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeRecord(7)).HasValue());
    reg.Retire(player::PlayerId{4});
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_EQ(reg.Records()[0].id.value, 7u);
    // Absent id: no-op, no error, no change.
    reg.Retire(player::PlayerId{99});
    reg.Retire(player::PlayerId{0});
    EXPECT_EQ(reg.size(), 1u);
}

// ============================================================================
// Step 4 — Lookup APIs + ValidateConsistency
// ============================================================================

namespace
{
    player::PlayerRecord MakeMapped(std::uint32_t id, std::uint32_t conn, std::uint64_t session,
                                    std::uint32_t entity)
    {
        player::PlayerRecord r{};
        r.id = player::PlayerId{id};
        r.connection = net::ConnectionId{conn};
        r.sessionMember = session;
        r.entity = world::EntityId{entity};
        return r;
    }
} // namespace

// --- All four lookups resolve on a hit ----------------------------------------
TEST(PlayerLookupStep4, FindByEachKeyHit)
{
    player::PlayerRegistry reg(8);
    ASSERT_TRUE(reg.Insert(MakeMapped(2, 20, 200, 2000)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeMapped(5, 50, 500, 5000)).HasValue());

    const auto byId = reg.FindByPlayerId(player::PlayerId{5});
    ASSERT_TRUE(byId.has_value());
    EXPECT_EQ(byId->id.value, 5u);
    EXPECT_EQ(byId->connection.value, 50u);
    EXPECT_EQ(byId->sessionMember, 500u);
    EXPECT_EQ(byId->entity.value, 5000u);

    const auto byConn = reg.FindByConnection(net::ConnectionId{20});
    ASSERT_TRUE(byConn.has_value());
    EXPECT_EQ(byConn->id.value, 2u);

    const auto bySess = reg.FindBySession(500);
    ASSERT_TRUE(bySess.has_value());
    EXPECT_EQ(bySess->id.value, 5u);

    const auto byEnt = reg.FindByEntity(world::EntityId{2000});
    ASSERT_TRUE(byEnt.has_value());
    EXPECT_EQ(byEnt->id.value, 2u);
}

// --- Misses (absent + 0/none) resolve to nullopt ------------------------------
TEST(PlayerLookupStep4, FindMissAndNoneKeys)
{
    player::PlayerRegistry reg(8);
    ASSERT_TRUE(reg.Insert(MakeMapped(1, 10, 100, 1000)).HasValue());

    EXPECT_FALSE(reg.FindByPlayerId(player::PlayerId{99}).has_value());
    EXPECT_FALSE(reg.FindByConnection(net::ConnectionId{99}).has_value());
    EXPECT_FALSE(reg.FindBySession(999).has_value());
    EXPECT_FALSE(reg.FindByEntity(world::EntityId{9999}).has_value());

    // 0/none keys are always unmapped.
    EXPECT_FALSE(reg.FindByPlayerId(player::PlayerId{0}).has_value());
    EXPECT_FALSE(reg.FindByConnection(net::ConnectionId{0}).has_value());
    EXPECT_FALSE(reg.FindBySession(0).has_value());
    EXPECT_FALSE(reg.FindByEntity(world::EntityId{0}).has_value());
}

// --- Accelerators stay consistent across insert/retire churn ------------------
TEST(PlayerLookupStep4, AcceleratorsConsistentAfterChurn)
{
    player::PlayerRegistry reg(8);
    ASSERT_TRUE(reg.Insert(MakeMapped(3, 30, 300, 3000)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeMapped(1, 10, 100, 1000)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeMapped(6, 60, 600, 6000)).HasValue());
    reg.Retire(player::PlayerId{1}); // shifts indices
    ASSERT_TRUE(reg.Insert(MakeMapped(4, 40, 400, 4000)).HasValue());

    EXPECT_TRUE(reg.ValidateConsistency().IsHealthy());
    // Post-churn lookups still resolve to the correct records.
    EXPECT_EQ(reg.FindByConnection(net::ConnectionId{60})->id.value, 6u);
    EXPECT_EQ(reg.FindByEntity(world::EntityId{4000})->id.value, 4u);
    EXPECT_FALSE(reg.FindByConnection(net::ConnectionId{10}).has_value()); // retired
}

// --- Healthy registry reports healthy -----------------------------------------
TEST(PlayerLookupStep4, ValidateConsistencyHealthy)
{
    player::PlayerRegistry reg(8);
    ASSERT_TRUE(reg.Insert(MakeMapped(2, 20, 200, 2000)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeMapped(8, 80, 800, 8000)).HasValue());
    const auto c = reg.ValidateConsistency();
    EXPECT_TRUE(c.sortedUnique);
    EXPECT_TRUE(c.acceleratorsConsistent);
    EXPECT_TRUE(c.mappingBijective);
    EXPECT_TRUE(c.withinCapacity);
    EXPECT_TRUE(c.noZeroId);
    EXPECT_TRUE(c.IsHealthy());
}

// --- Broken bijection (two records share a secondary key) is detected ---------
TEST(PlayerLookupStep4, ValidateConsistencyDetectsBrokenBijection)
{
    player::PlayerRegistry reg(8);
    // Distinct PlayerIds, but the SAME connection value on both records — a
    // bijection violation surfaced through the public API (Insert only enforces
    // PlayerId uniqueness).
    ASSERT_TRUE(reg.Insert(MakeMapped(1, 77, 100, 1000)).HasValue());
    ASSERT_TRUE(reg.Insert(MakeMapped(2, 77, 200, 2000)).HasValue());
    const auto c = reg.ValidateConsistency();
    EXPECT_FALSE(c.mappingBijective);
    EXPECT_FALSE(c.IsHealthy());
}

// ============================================================================
// Step 5 — Lifecycle state machine (pure, stateless)
// ============================================================================

namespace
{
    player::PlayerRecord RecordInState(player::PlayerLifecycleState s, std::uint64_t eligible = 0)
    {
        player::PlayerRecord r{};
        r.id = player::PlayerId{1};
        r.lifecycle = s;
        r.respawnEligibleTick = eligible;
        return r;
    }
} // namespace

// --- Join: Joining -> Active; illegal from any other state --------------------
TEST(PlayerLifecycleStep5, JoinLegalAndIllegal)
{
    using S = player::PlayerLifecycleState;
    const auto ok = player::ApplyJoin(RecordInState(S::Joining), 10);
    ASSERT_TRUE(ok.HasValue());
    EXPECT_EQ(ok.Value(), S::Active);

    for (S s : {S::Active, S::Dead, S::AwaitingRespawn, S::Suspended, S::Removed})
    {
        EXPECT_FALSE(player::ApplyJoin(RecordInState(s), 10).HasValue());
    }
}

// --- Death: Active -> Dead; illegal otherwise ---------------------------------
TEST(PlayerLifecycleStep5, DeathLegalAndIllegal)
{
    using S = player::PlayerLifecycleState;
    const auto ok = player::ApplyDeath(RecordInState(S::Active), 5);
    ASSERT_TRUE(ok.HasValue());
    EXPECT_EQ(ok.Value(), S::Dead);

    for (S s : {S::Joining, S::Dead, S::AwaitingRespawn, S::Suspended, S::Removed})
    {
        EXPECT_FALSE(player::ApplyDeath(RecordInState(s), 5).HasValue());
    }
}

// --- Respawn: waits until eligible, then Active; illegal from non-dead states --
TEST(PlayerLifecycleStep5, RespawnTimingAndLegality)
{
    using S = player::PlayerLifecycleState;
    player::PlayerConfiguration cfg; // respawnDelayTicks default 300

    // Dead, scheduled eligible=100: before -> AwaitingRespawn, at/after -> Active.
    const auto early = player::ApplyRespawn(RecordInState(S::Dead, 100), 50, cfg);
    ASSERT_TRUE(early.HasValue());
    EXPECT_EQ(early.Value(), S::AwaitingRespawn);

    const auto ready = player::ApplyRespawn(RecordInState(S::AwaitingRespawn, 100), 100, cfg);
    ASSERT_TRUE(ready.HasValue());
    EXPECT_EQ(ready.Value(), S::Active);

    const auto after = player::ApplyRespawn(RecordInState(S::Dead, 100), 250, cfg);
    ASSERT_TRUE(after.HasValue());
    EXPECT_EQ(after.Value(), S::Active);

    // Illegal from non-dead/awaiting states.
    for (S s : {S::Joining, S::Active, S::Suspended, S::Removed})
    {
        EXPECT_FALSE(player::ApplyRespawn(RecordInState(s), 100, cfg).HasValue());
    }
}

// --- Unscheduled respawn derives eligibility from config (delay 0 => immediate)-
TEST(PlayerLifecycleStep5, RespawnUnscheduledUsesConfig)
{
    using S = player::PlayerLifecycleState;
    player::PlayerConfiguration cfg;
    cfg.respawnDelayTicks = 0; // immediate
    const auto r = player::ApplyRespawn(RecordInState(S::Dead, 0), 42, cfg);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value(), S::Active);

    cfg.respawnDelayTicks = 20;
    const auto r2 = player::ApplyRespawn(RecordInState(S::Dead, 0), 10, cfg); // eligible=10+20=30 > 10
    ASSERT_TRUE(r2.HasValue());
    EXPECT_EQ(r2.Value(), S::AwaitingRespawn);
}

// --- Suspend(Retain): Active|Dead|AwaitingRespawn -> Suspended ----------------
TEST(PlayerLifecycleStep5, SuspendRetain)
{
    using S = player::PlayerLifecycleState;
    for (S s : {S::Active, S::Dead, S::AwaitingRespawn})
    {
        const auto r = player::ApplySuspend(RecordInState(s), player::DisconnectDisposition::Retain);
        ASSERT_TRUE(r.HasValue());
        EXPECT_EQ(r.Value(), S::Suspended);
    }
    // Retain illegal from Joining/Suspended/Removed.
    for (S s : {S::Joining, S::Suspended, S::Removed})
    {
        EXPECT_FALSE(player::ApplySuspend(RecordInState(s), player::DisconnectDisposition::Retain).HasValue());
    }
}

// --- Suspend(Remove): any -> Removed ------------------------------------------
TEST(PlayerLifecycleStep5, SuspendRemoveAlwaysRemoved)
{
    using S = player::PlayerLifecycleState;
    for (S s : {S::Joining, S::Active, S::Dead, S::AwaitingRespawn, S::Suspended, S::Removed})
    {
        const auto r = player::ApplySuspend(RecordInState(s), player::DisconnectDisposition::Remove);
        ASSERT_TRUE(r.HasValue());
        EXPECT_EQ(r.Value(), S::Removed);
    }
}

// --- Reclaim: Suspended -> Active; illegal otherwise --------------------------
TEST(PlayerLifecycleStep5, ReclaimLegalAndIllegal)
{
    using S = player::PlayerLifecycleState;
    const auto ok = player::ApplyReclaim(RecordInState(S::Suspended), 7);
    ASSERT_TRUE(ok.HasValue());
    EXPECT_EQ(ok.Value(), S::Active);

    for (S s : {S::Joining, S::Active, S::Dead, S::AwaitingRespawn, S::Removed})
    {
        EXPECT_FALSE(player::ApplyReclaim(RecordInState(s), 7).HasValue());
    }
}

// --- Remove: any -> Removed (always legal) ------------------------------------
TEST(PlayerLifecycleStep5, RemoveAlwaysLegal)
{
    using S = player::PlayerLifecycleState;
    for (S s : {S::Joining, S::Active, S::Dead, S::AwaitingRespawn, S::Suspended, S::Removed})
    {
        const auto r = player::ApplyRemove(RecordInState(s));
        ASSERT_TRUE(r.HasValue());
        EXPECT_EQ(r.Value(), S::Removed);
    }
}

// --- ComputeRespawnEligibleTick = deathTick + respawnDelayTicks ---------------
TEST(PlayerLifecycleStep5, ComputeRespawnEligibleTick)
{
    player::PlayerConfiguration cfg;
    cfg.respawnDelayTicks = 250;
    EXPECT_EQ(player::ComputeRespawnEligibleTick(1000, cfg), 1250u);
    cfg.respawnDelayTicks = 0;
    EXPECT_EQ(player::ComputeRespawnEligibleTick(1000, cfg), 1000u);
}

// ============================================================================
// Step 6 — Session integration (observer + delta queue), enqueue-only
// ============================================================================

// --- Join callback enqueues exactly one Joined delta with the right fields -----
TEST(PlayerSessionStep6, JoinEnqueuesDelta)
{
    player::PlayerDeltaQueue queue;
    player::PlayerSessionObserver obs(queue);
    obs.OnMemberJoined(net::ConnectionId{11}, 500);

    ASSERT_EQ(queue.size(), 1u);
    const auto drained = queue.Drain();
    ASSERT_EQ(drained.size(), 1u);
    EXPECT_EQ(drained[0].kind, player::PlayerSessionEventKind::Joined);
    EXPECT_EQ(drained[0].connection.value, 11u);
    EXPECT_EQ(drained[0].tick, 500u);
    EXPECT_EQ(drained[0].reason, net::DisconnectReason::None);
    EXPECT_EQ(drained[0].reconnectToken, 0u);
}

// --- Leave callback enqueues exactly one Left delta with reason + token --------
TEST(PlayerSessionStep6, LeaveEnqueuesDelta)
{
    player::PlayerDeltaQueue queue;
    player::PlayerSessionObserver obs(queue);
    obs.OnMemberLeft(net::ConnectionId{7}, net::DisconnectReason::Timeout, 0xABCDu);

    ASSERT_EQ(queue.size(), 1u);
    const auto drained = queue.Drain();
    ASSERT_EQ(drained.size(), 1u);
    EXPECT_EQ(drained[0].kind, player::PlayerSessionEventKind::Left);
    EXPECT_EQ(drained[0].connection.value, 7u);
    EXPECT_EQ(drained[0].reason, net::DisconnectReason::Timeout);
    EXPECT_EQ(drained[0].reconnectToken, 0xABCDu);
}

// --- Enqueue-only: each callback adds exactly one, mutating nothing else -------
TEST(PlayerSessionStep6, EnqueueOnlyOnePerCallback)
{
    player::PlayerDeltaQueue queue;
    player::PlayerSessionObserver obs(queue);
    EXPECT_EQ(queue.size(), 0u);
    obs.OnMemberJoined(net::ConnectionId{1}, 1);
    EXPECT_EQ(queue.size(), 1u);
    obs.OnMemberLeft(net::ConnectionId{2}, net::DisconnectReason::Graceful, 0);
    EXPECT_EQ(queue.size(), 2u);
    EXPECT_FALSE(queue.empty());
}

// --- Drain returns deterministic order (ascending ConnectionId) and empties ----
TEST(PlayerSessionStep6, DrainDeterministicOrderAndEmpties)
{
    player::PlayerDeltaQueue queue;
    player::PlayerSessionObserver obs(queue);
    // Arrival order deliberately not ascending by connection.
    obs.OnMemberJoined(net::ConnectionId{5}, 10);
    obs.OnMemberJoined(net::ConnectionId{2}, 11);
    obs.OnMemberLeft(net::ConnectionId{9}, net::DisconnectReason::Timeout, 1);
    obs.OnMemberJoined(net::ConnectionId{2}, 12); // same connection as 2nd; stable tie-break

    const auto d = queue.Drain();
    ASSERT_EQ(d.size(), 4u);
    EXPECT_EQ(d[0].connection.value, 2u);
    EXPECT_EQ(d[1].connection.value, 2u);
    // Stable: the earlier-arriving conn-2 (tick 11) precedes the later (tick 12).
    EXPECT_EQ(d[0].tick, 11u);
    EXPECT_EQ(d[1].tick, 12u);
    EXPECT_EQ(d[2].connection.value, 5u);
    EXPECT_EQ(d[3].connection.value, 9u);
    // Drain emptied the queue.
    EXPECT_EQ(queue.size(), 0u);
    EXPECT_TRUE(queue.empty());
}

// --- Driven through a real net::Session (subscription fires enqueue-only) ------
TEST(PlayerSessionStep6, DrivenByRealSession)
{
    player::PlayerDeltaQueue queue;
    player::PlayerSessionObserver obs(queue);

    net::Session session(8);
    session.Subscribe(&obs);

    ASSERT_TRUE(session.Admit(net::ConnectionId{3}, 100).HasValue());
    ASSERT_TRUE(session.Admit(net::ConnectionId{1}, 101).HasValue());
    session.Remove(net::ConnectionId{3}, net::DisconnectReason::Graceful);

    const auto d = queue.Drain();
    ASSERT_EQ(d.size(), 3u);
    // Ascending ConnectionId order after drain: conn 1 (join), conn 3 (join), conn 3 (left).
    EXPECT_EQ(d[0].connection.value, 1u);
    EXPECT_EQ(d[0].kind, player::PlayerSessionEventKind::Joined);
    EXPECT_EQ(d[1].connection.value, 3u);
    EXPECT_EQ(d[1].kind, player::PlayerSessionEventKind::Joined);
    EXPECT_EQ(d[2].connection.value, 3u);
    EXPECT_EQ(d[2].kind, player::PlayerSessionEventKind::Left);
    EXPECT_EQ(d[2].reason, net::DisconnectReason::Graceful);
}

// ============================================================================
// Step 7 — Player manager (transactional join/reclaim, queue consumption)
// ============================================================================

namespace
{
    // Test-only spawn gateway double (NOT Step-9's NullPlayerSpawnGateway). Mints
    // deterministic ascending EntityIds; can be told to fail the next Spawn.
    class FakeSpawnGateway final : public player::IPlayerSpawnGateway
    {
    public:
        [[nodiscard]] core::Expected<world::EntityId> Spawn(const player::PlayerProfile&,
                                                            const world::PlayerPosition&) override
        {
            if (m_failNext)
            {
                m_failNext = false;
                return core::MakeError(core::ErrorCode::IoError, "spawn failed (test)");
            }
            return world::EntityId{m_nextEntity++};
        }
        [[nodiscard]] player::SpawnOutcome Despawn(world::EntityId) override
        {
            ++m_despawns;
            return player::SpawnOutcome::Spawned;
        }
        void FailNextSpawn() { m_failNext = true; }
        [[nodiscard]] std::uint32_t Despawns() const { return m_despawns; }

    private:
        std::uint32_t m_nextEntity = 1000;
        std::uint32_t m_despawns = 0;
        bool m_failNext = false;
    };

    player::PlayerConfiguration MgrConfig(std::uint32_t maxPlayers, std::uint32_t respawnDelay = 100)
    {
        player::PlayerConfiguration c;
        c.maxPlayers = maxPlayers;
        c.respawnDelayTicks = respawnDelay;
        return c;
    }
} // namespace

// --- Join: accepted, registered, Active, position published -------------------
TEST(PlayerManagerStep7, JoinAcceptedAndRegistered)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);

    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);
    EXPECT_EQ(mgr.PlayerCount(), 1u);

    const auto view = mgr.FindByConnection(net::ConnectionId{5});
    ASSERT_TRUE(view.has_value());
    EXPECT_NE(view->entity.value, 0u); // entity materialized by the gateway
    EXPECT_EQ(mgr.ActivePlayerPositions().size(), 1u);
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// --- Host Authority: invalid request rejected ---------------------------------
TEST(PlayerManagerStep7, JoinInvalidConnectionRejected)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{0}, player::PlayerProfile{}, 1), player::JoinOutcome::RejectedInvalid);
    EXPECT_EQ(mgr.PlayerCount(), 0u);
}

// --- Duplicate join on the same connection rejected ---------------------------
TEST(PlayerManagerStep7, DuplicateJoinRejected)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);
    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 2), player::JoinOutcome::RejectedDuplicate);
    EXPECT_EQ(mgr.PlayerCount(), 1u);
}

// --- Capacity enforced --------------------------------------------------------
TEST(PlayerManagerStep7, JoinCapacityEnforced)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(1), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{1}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);
    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{2}, player::PlayerProfile{}, 2), player::JoinOutcome::RejectedCapacity);
    EXPECT_EQ(mgr.PlayerCount(), 1u);
}

// --- Transactional rollback: spawn failure leaves no orphan -------------------
TEST(PlayerManagerStep7, JoinSpawnFailureRollsBack)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    gw.FailNextSpawn();
    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 1), player::JoinOutcome::RejectedSpawnFailed);
    EXPECT_EQ(mgr.PlayerCount(), 0u); // no orphan record/mapping
    EXPECT_FALSE(mgr.FindByConnection(net::ConnectionId{5}).has_value());
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// --- Death then respawn (timing-gated) ----------------------------------------
TEST(PlayerManagerStep7, DeathAndRespawn)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8, /*respawnDelay*/ 50), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);
    const auto id = mgr.FindByConnection(net::ConnectionId{5})->id;

    EXPECT_EQ(mgr.NotifyDeath(id, 200), player::JoinOutcome::Accepted); // eligible = 200+50 = 250
    EXPECT_EQ(mgr.RequestRespawn(id, 240), player::SpawnOutcome::RejectedInvalid); // too early
    EXPECT_EQ(mgr.RequestRespawn(id, 250), player::SpawnOutcome::Spawned);         // eligible
    EXPECT_EQ(mgr.Statistics().deaths, 1u);
    EXPECT_EQ(mgr.Statistics().respawns, 1u);
    EXPECT_EQ(mgr.Statistics().connected, 1u); // back to Active
}

// --- Leave suspends (retain); record persists, connection cleared -------------
TEST(PlayerManagerStep7, LeaveSuspendsAndRetains)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);

    player::PlayerDeltaQueue q;
    player::PlayerSessionDelta left{};
    left.kind = player::PlayerSessionEventKind::Left;
    left.connection = net::ConnectionId{5};
    left.reason = net::DisconnectReason::Timeout;
    left.reconnectToken = 0xBEEF;
    q.Enqueue(left);
    mgr.ApplyPendingDeltas(q, 300);

    EXPECT_EQ(mgr.PlayerCount(), 1u); // retained (not destroyed)
    EXPECT_FALSE(mgr.FindByConnection(net::ConnectionId{5}).has_value()); // connection cleared
    EXPECT_EQ(mgr.Statistics().suspended, 1u);
}

// --- Reclaim delegates to Session::TryReclaim (Sprint-006 stub => TokenUnknown)-
TEST(PlayerManagerStep7, ReclaimDelegatesToSessionSeam)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);

    // Suspend first.
    player::PlayerDeltaQueue q;
    player::PlayerSessionDelta left{};
    left.kind = player::PlayerSessionEventKind::Left;
    left.connection = net::ConnectionId{5};
    left.reconnectToken = 0xBEEF;
    q.Enqueue(left);
    mgr.ApplyPendingDeltas(q, 300);

    // Sprint-006 TryReclaim returns NotFound -> TokenUnknown; no duplicate created.
    EXPECT_EQ(mgr.ApplyReclaim(net::ConnectionId{9}, 0xBEEF, 400), player::ReconnectOutcome::TokenUnknown);
    EXPECT_EQ(mgr.ApplyReclaim(net::ConnectionId{0}, 0xBEEF, 400), player::ReconnectOutcome::RejectedInvalid);
    EXPECT_EQ(mgr.PlayerCount(), 1u); // still exactly one record, no duplicate
}

// --- Deterministic replay: identical delta+tick sequence => identical state ----
TEST(PlayerManagerStep7, DeterministicReplay)
{
    auto run = [](std::vector<world::PlayerPosition>& positions, std::vector<std::uint32_t>& entities) {
        FakeSpawnGateway gw;
        net::Session session(8);
        player::PlayerManager mgr(MgrConfig(8), gw, session);

        player::PlayerDeltaQueue q;
        for (std::uint32_t c : {5u, 2u, 8u})
        {
            player::PlayerSessionDelta d{};
            d.kind = player::PlayerSessionEventKind::Joined;
            d.connection = net::ConnectionId{c};
            d.tick = 100 + c;
            q.Enqueue(d);
        }
        mgr.ApplyPendingDeltas(q, 100);
        positions = mgr.ActivePlayerPositions();
        for (const auto& v : {mgr.FindByConnection(net::ConnectionId{2}), mgr.FindByConnection(net::ConnectionId{5}),
                              mgr.FindByConnection(net::ConnectionId{8})})
        {
            entities.push_back(v.has_value() ? v->entity.value : 0u);
        }
    };

    std::vector<world::PlayerPosition> p1, p2;
    std::vector<std::uint32_t> e1, e2;
    run(p1, e1);
    run(p2, e2);

    ASSERT_EQ(p1.size(), 3u);
    ASSERT_EQ(p1.size(), p2.size());
    EXPECT_EQ(e1, e2); // identical entity registrations across runs
    for (std::size_t i = 0; i < p1.size(); ++i)
    {
        EXPECT_EQ(p1[i].id.value, p2[i].id.value);
        EXPECT_EQ(p1[i].lastUpdateTick, p2[i].lastUpdateTick);
    }
}

// ============================================================================
// Step 8 — Networked position source (implements world::IPlayerPositionSource)
// ============================================================================

// --- Source mirrors the manager's active positions, ascending PlayerId --------
TEST(PositionSourceStep8, MirrorsManagerAscending)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::NetworkedPlayerPositionSource source(mgr);

    // Join out of connection order; positions come back ascending by PlayerId.
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{9}, player::PlayerProfile{}, 10), player::JoinOutcome::Accepted);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{4}, player::PlayerProfile{}, 11), player::JoinOutcome::Accepted);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{7}, player::PlayerProfile{}, 12), player::JoinOutcome::Accepted);

    const std::vector<world::PlayerPosition> snap = source.ActivePlayers();
    ASSERT_EQ(snap.size(), 3u);
    // PlayerIds are allocated ascending (1,2,3) regardless of connection order.
    EXPECT_EQ(snap[0].id.value, 1u);
    EXPECT_EQ(snap[1].id.value, 2u);
    EXPECT_EQ(snap[2].id.value, 3u);
    // Matches the manager's own snapshot exactly.
    EXPECT_EQ(snap.size(), mgr.ActivePlayerPositions().size());
}

// --- Usable polymorphically through the frozen world interface ----------------
TEST(PositionSourceStep8, UsableThroughInterface)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::NetworkedPlayerPositionSource source(mgr);

    world::IPlayerPositionSource& iface = source; // drop-in for the Bubble Manager
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{2}, player::PlayerProfile{}, 5), player::JoinOutcome::Accepted);
    const auto snap = iface.ActivePlayers();
    ASSERT_EQ(snap.size(), 1u);
    EXPECT_EQ(snap[0].id.value, 1u);
    EXPECT_EQ(snap[0].lastUpdateTick, 5u);
}

// --- Empty state => empty snapshot --------------------------------------------
TEST(PositionSourceStep8, EmptyWhenNoActivePlayers)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::NetworkedPlayerPositionSource source(mgr);
    EXPECT_TRUE(source.ActivePlayers().empty());
}

// --- Lifecycle filtering: suspended/dead players are excluded ------------------
TEST(PositionSourceStep8, FiltersNonActiveByLifecycle)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::NetworkedPlayerPositionSource source(mgr);

    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{1}, player::PlayerProfile{}, 10), player::JoinOutcome::Accepted);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{2}, player::PlayerProfile{}, 11), player::JoinOutcome::Accepted);
    EXPECT_EQ(source.ActivePlayers().size(), 2u);

    // Suspend player on connection 1 (leave) -> excluded from the active snapshot.
    player::PlayerDeltaQueue q;
    player::PlayerSessionDelta left{};
    left.kind = player::PlayerSessionEventKind::Left;
    left.connection = net::ConnectionId{1};
    q.Enqueue(left);
    mgr.ApplyPendingDeltas(q, 300);
    EXPECT_EQ(source.ActivePlayers().size(), 1u);

    // Kill player 2 -> Dead -> excluded too.
    const auto id2 = mgr.FindByConnection(net::ConnectionId{2})->id;
    ASSERT_EQ(mgr.NotifyDeath(id2, 320), player::JoinOutcome::Accepted);
    EXPECT_TRUE(source.ActivePlayers().empty());
}

// ============================================================================
// Step 9 — Null spawn gateway (test-build factory; engine-free, deterministic)
// ============================================================================

// --- Factory yields a usable, engine-free gateway -----------------------------
TEST(NullSpawnGatewayStep9, FactoryProducesGateway)
{
    std::unique_ptr<player::IPlayerSpawnGateway> gw = adapters::CreatePlayerSpawnGateway();
    ASSERT_NE(gw, nullptr);
}

// --- Spawn mints deterministic ascending, non-zero EntityIds ------------------
TEST(NullSpawnGatewayStep9, SpawnAscendingNonZero)
{
    std::unique_ptr<player::IPlayerSpawnGateway> gw = adapters::CreatePlayerSpawnGateway();
    const auto a = gw->Spawn(player::PlayerProfile{}, world::PlayerPosition{});
    const auto b = gw->Spawn(player::PlayerProfile{}, world::PlayerPosition{});
    ASSERT_TRUE(a.HasValue());
    ASSERT_TRUE(b.HasValue());
    EXPECT_NE(a.Value().value, 0u);
    EXPECT_LT(a.Value().value, b.Value().value); // strictly ascending

    // Determinism: a fresh gateway repeats the same id sequence.
    std::unique_ptr<player::IPlayerSpawnGateway> gw2 = adapters::CreatePlayerSpawnGateway();
    const auto a2 = gw2->Spawn(player::PlayerProfile{}, world::PlayerPosition{});
    ASSERT_TRUE(a2.HasValue());
    EXPECT_EQ(a.Value().value, a2.Value().value);
}

// --- Despawn is a benign value outcome ----------------------------------------
TEST(NullSpawnGatewayStep9, DespawnBenign)
{
    std::unique_ptr<player::IPlayerSpawnGateway> gw = adapters::CreatePlayerSpawnGateway();
    const auto e = gw->Spawn(player::PlayerProfile{}, world::PlayerPosition{});
    ASSERT_TRUE(e.HasValue());
    EXPECT_EQ(gw->Despawn(e.Value()), player::SpawnOutcome::Spawned);
    EXPECT_EQ(gw->Despawn(world::EntityId{999}), player::SpawnOutcome::Spawned); // unknown id: benign
}

// --- PlayerManager runs a full join against the null (via the factory) --------
TEST(NullSpawnGatewayStep9, ManagerRunsAgainstNull)
{
    std::unique_ptr<player::IPlayerSpawnGateway> gw = adapters::CreatePlayerSpawnGateway();
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), *gw, session);
    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);
    EXPECT_EQ(mgr.PlayerCount(), 1u);
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// ============================================================================
// Step 12 — Player diagnostics (read-only)
// ============================================================================

// --- Statistics reflect the manager's live tallies ---------------------------
TEST(PlayerDiagnosticsStep12, StatisticsAndState)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8, /*respawnDelay*/ 50), gw, session);
    player::PlayerDiagnostics diag(mgr);

    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{6}, player::PlayerProfile{}, 101), player::JoinOutcome::Accepted);
    const auto id5 = mgr.FindByConnection(net::ConnectionId{5})->id;
    ASSERT_EQ(mgr.NotifyDeath(id5, 200), player::JoinOutcome::Accepted);

    const auto s = diag.Statistics();
    EXPECT_EQ(s.connected, 1u); // conn 6 active
    EXPECT_EQ(s.dead, 1u);      // conn 5 dead
    EXPECT_EQ(s.deaths, 1u);
    // DescribeState is a non-empty read-only summary mentioning the counts.
    const std::string state = diag.DescribeState();
    EXPECT_NE(state.find("connected 1"), std::string::npos);
    EXPECT_NE(state.find("dead 1"), std::string::npos);
    EXPECT_NE(state.find("healthy"), std::string::npos);
}

// --- DescribePlayer: present shows lifecycle; absent shows a stable line -------
TEST(PlayerDiagnosticsStep12, DescribePlayerPresentAndAbsent)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::PlayerDiagnostics diag(mgr);

    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{9}, player::PlayerProfile{}, 10), player::JoinOutcome::Accepted);
    const auto id = mgr.FindByConnection(net::ConnectionId{9})->id;

    const std::string present = diag.DescribePlayer(id);
    EXPECT_NE(present.find("lifecycle=Active"), std::string::npos);
    EXPECT_NE(present.find("conn=9"), std::string::npos);

    EXPECT_NE(diag.DescribePlayer(player::PlayerId{999}).find("absent"), std::string::npos);
}

// --- DumpPlayers lists players ascending by PlayerId ---------------------------
TEST(PlayerDiagnosticsStep12, DumpPlayersAscending)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::PlayerDiagnostics diag(mgr);

    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{4}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{2}, player::PlayerProfile{}, 2), player::JoinOutcome::Accepted);

    const std::string dump = diag.DumpPlayers();
    EXPECT_NE(dump.find("2 players"), std::string::npos);
    // player 1 (first allocated) appears before player 2 in the ascending dump.
    const auto p1 = dump.find("player 1 ");
    const auto p2 = dump.find("player 2 ");
    ASSERT_NE(p1, std::string::npos);
    ASSERT_NE(p2, std::string::npos);
    EXPECT_LT(p1, p2);
}

// --- ValidateConsistency healthy; determinism (same twice) --------------------
TEST(PlayerDiagnosticsStep12, ConsistencyAndDeterminism)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::PlayerDiagnostics diag(mgr);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{3}, player::PlayerProfile{}, 5), player::JoinOutcome::Accepted);

    EXPECT_TRUE(diag.ValidateConsistency().IsHealthy());
    EXPECT_TRUE(diag.PositionFeedAgrees());
    // Deterministic/replay-stable: identical calls yield identical output.
    EXPECT_EQ(diag.DumpPlayers(), diag.DumpPlayers());
    EXPECT_EQ(diag.DescribeState(), diag.DescribeState());
}

// --- Diagnostics are read-only: player count/consistency unchanged by calls ----
TEST(PlayerDiagnosticsStep12, ReadOnly)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    player::PlayerDiagnostics diag(mgr);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{1}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);

    const std::size_t before = mgr.PlayerCount();
    (void)diag.Statistics();
    (void)diag.DescribeState();
    (void)diag.DescribePlayer(player::PlayerId{1});
    (void)diag.DumpPlayers();
    (void)diag.ValidateConsistency();
    (void)diag.PositionFeedAgrees();
    EXPECT_EQ(mgr.PlayerCount(), before); // no mutation
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// ============================================================================
// Step 13 — Validation hardening (full negative surface + churn/replay)
// ============================================================================

// --- Duplicate entity prevention: every player has a unique EntityId ----------
TEST(PlayerHardeningStep13, NoDuplicateEntities)
{
    FakeSpawnGateway gw;
    net::Session session(16);
    player::PlayerManager mgr(MgrConfig(16), gw, session);
    for (std::uint32_t c = 1; c <= 8; ++c)
    {
        ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{c}, player::PlayerProfile{}, c), player::JoinOutcome::Accepted);
    }
    // All entity ids distinct (bijection) and the registry stays healthy.
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
    for (std::uint32_t c = 1; c <= 8; ++c)
    {
        const auto v = mgr.FindByConnection(net::ConnectionId{c});
        ASSERT_TRUE(v.has_value());
        // Each connection resolves to a distinct entity (checked via bijection).
        EXPECT_NE(v->entity.value, 0u);
    }
}

// --- Invalid respawn requests are rejected AND leave state unchanged ----------
TEST(PlayerHardeningStep13, InvalidRespawnLeavesStateUnchanged)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8, /*respawnDelay*/ 100), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 100), player::JoinOutcome::Accepted);
    const auto id = mgr.FindByConnection(net::ConnectionId{5})->id;

    // Respawn on an Active player -> RejectedInvalid, no change.
    EXPECT_EQ(mgr.RequestRespawn(id, 150), player::SpawnOutcome::RejectedInvalid);
    EXPECT_EQ(mgr.Statistics().connected, 1u);
    EXPECT_EQ(mgr.Statistics().respawns, 0u);

    // Respawn on an absent id -> RejectedInvalid.
    EXPECT_EQ(mgr.RequestRespawn(player::PlayerId{999}, 150), player::SpawnOutcome::RejectedInvalid);

    // Kill, then a too-early respawn is rejected WITHOUT mutating the record
    // (Step-13 hardening: rejection never changes state).
    ASSERT_EQ(mgr.NotifyDeath(id, 200), player::JoinOutcome::Accepted); // eligible = 300
    EXPECT_EQ(mgr.RequestRespawn(id, 250), player::SpawnOutcome::RejectedInvalid); // too early
    // Still Dead (not silently moved to another state), respawns unchanged.
    EXPECT_EQ(mgr.Statistics().dead, 1u);
    EXPECT_EQ(mgr.Statistics().connected, 0u);
    EXPECT_EQ(mgr.Statistics().respawns, 0u);
    // And a later eligible respawn still succeeds (valid input unaffected).
    EXPECT_EQ(mgr.RequestRespawn(id, 300), player::SpawnOutcome::Spawned);
    EXPECT_EQ(mgr.Statistics().connected, 1u);
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// --- Invalid death requests rejected, no change -------------------------------
TEST(PlayerHardeningStep13, InvalidDeathRejected)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);
    const auto id = mgr.FindByConnection(net::ConnectionId{5})->id;

    EXPECT_EQ(mgr.NotifyDeath(player::PlayerId{999}, 2), player::JoinOutcome::RejectedInvalid); // absent
    ASSERT_EQ(mgr.NotifyDeath(id, 3), player::JoinOutcome::Accepted);                            // Active->Dead
    EXPECT_EQ(mgr.NotifyDeath(id, 4), player::JoinOutcome::RejectedInvalid);                     // already Dead
    EXPECT_EQ(mgr.Statistics().deaths, 1u); // only the one legal death counted
}

// --- Ownership: a bad/absent reconnect token cannot reclaim; no duplicate ------
TEST(PlayerHardeningStep13, ReconnectOwnershipEnforced)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(8), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{5}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);

    // Suspend via a Left delta.
    player::PlayerDeltaQueue q;
    player::PlayerSessionDelta left{};
    left.kind = player::PlayerSessionEventKind::Left;
    left.connection = net::ConnectionId{5};
    left.reconnectToken = 0x1234;
    q.Enqueue(left);
    mgr.ApplyPendingDeltas(q, 10);

    // Wrong/absent token cannot reclaim (Sprint-006 TryReclaim gates it); no
    // duplicate record is ever created by a failed reclaim.
    EXPECT_EQ(mgr.ApplyReclaim(net::ConnectionId{9}, 0xDEAD, 20), player::ReconnectOutcome::TokenUnknown);
    EXPECT_EQ(mgr.ApplyReclaim(net::ConnectionId{9}, 0x1234, 20), player::ReconnectOutcome::TokenUnknown);
    EXPECT_EQ(mgr.PlayerCount(), 1u);
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// --- Capacity (including suspended players) rejects with no state change -------
TEST(PlayerHardeningStep13, CapacityIncludesSuspended)
{
    FakeSpawnGateway gw;
    net::Session session(8);
    player::PlayerManager mgr(MgrConfig(2), gw, session);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{1}, player::PlayerProfile{}, 1), player::JoinOutcome::Accepted);
    ASSERT_EQ(mgr.RequestJoin(net::ConnectionId{2}, player::PlayerProfile{}, 2), player::JoinOutcome::Accepted);

    // Suspend one — it is RETAINED, so capacity is still full.
    player::PlayerDeltaQueue q;
    player::PlayerSessionDelta left{};
    left.kind = player::PlayerSessionEventKind::Left;
    left.connection = net::ConnectionId{1};
    q.Enqueue(left);
    mgr.ApplyPendingDeltas(q, 3);
    EXPECT_EQ(mgr.PlayerCount(), 2u); // suspended still occupies a slot

    const std::size_t before = mgr.PlayerCount();
    EXPECT_EQ(mgr.RequestJoin(net::ConnectionId{3}, player::PlayerProfile{}, 4), player::JoinOutcome::RejectedCapacity);
    EXPECT_EQ(mgr.PlayerCount(), before); // rejection left state unchanged
    EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
}

// --- Many-player churn stress stays healthy and is replay-identical -----------
TEST(PlayerHardeningStep13, ChurnStressReplayIdentical)
{
    auto run = [](std::string& signature) {
        FakeSpawnGateway gw;
        net::Session session(64);
        player::PlayerManager mgr(MgrConfig(64, /*respawnDelay*/ 5), gw, session);
        player::PlayerDiagnostics diag(mgr);

        // Deterministic churn: join 20, kill/respawn some, suspend some.
        for (std::uint32_t c = 1; c <= 20; ++c)
        {
            (void)mgr.RequestJoin(net::ConnectionId{c}, player::PlayerProfile{}, 100 + c);
        }
        for (std::uint32_t c = 1; c <= 20; c += 3) // kill every 3rd
        {
            const auto v = mgr.FindByConnection(net::ConnectionId{c});
            if (v.has_value())
            {
                (void)mgr.NotifyDeath(v->id, 200);
                (void)mgr.RequestRespawn(v->id, 205); // eligible = 200+5
            }
        }
        player::PlayerDeltaQueue q; // suspend every 5th via Left deltas
        for (std::uint32_t c = 5; c <= 20; c += 5)
        {
            player::PlayerSessionDelta d{};
            d.kind = player::PlayerSessionEventKind::Left;
            d.connection = net::ConnectionId{c};
            q.Enqueue(d);
        }
        mgr.ApplyPendingDeltas(q, 300);

        EXPECT_TRUE(mgr.ValidateConsistency().IsHealthy());
        EXPECT_TRUE(diag.PositionFeedAgrees());
        signature = diag.DumpPlayers(); // deterministic snapshot
    };

    std::string sig1;
    std::string sig2;
    run(sig1);
    run(sig2);
    EXPECT_EQ(sig1, sig2); // identical churn sequence -> identical state (replay)
}

// ============================================================================
// Step 14 — Integration (composed null-gateway / Session stack)
// ============================================================================
//
// Drives the REAL composed ingress: net::Session::Admit/Remove fire the enqueue-
// only PlayerSessionObserver -> PlayerDeltaQueue; PlayerManagerService::Tick
// drains + applies at the player tick. The null spawn gateway stands in for the
// engine (the EnginePlayerSpawnGateway path is Antigravity's, Step 10).

namespace
{
    struct ComposedStack
    {
        std::unique_ptr<player::IPlayerSpawnGateway> gateway = adapters::CreatePlayerSpawnGateway();
        net::Session session{8};
        net::MessageRegistry registry;
        player::PlayerManager manager;
        player::PlayerDeltaQueue queue;
        player::PlayerManagerService service;
        player::NetworkedPlayerPositionSource source;

        explicit ComposedStack(std::uint32_t respawnDelay = 50)
            : manager(MgrConfig(8, respawnDelay), *gateway, session),
              service(manager, queue, session, registry), source(manager)
        {
            (void)service.Initialize(); // subscribe observer + register message ids
        }
    };
} // namespace

// --- (1) Player appears in the (entity) registry; (3) session mapping correct --
TEST(PlayerIntegrationStep14, JoinRegistersAndMaps)
{
    ComposedStack st;
    ASSERT_TRUE(st.session.Admit(net::ConnectionId{5}, 100).HasValue());
    st.service.Tick(0.016);

    const auto byConn = st.manager.FindByConnection(net::ConnectionId{5});
    ASSERT_TRUE(byConn.has_value());
    EXPECT_NE(byConn->entity.value, 0u);
    EXPECT_TRUE(st.manager.FindByEntity(byConn->entity).has_value());
    EXPECT_TRUE(st.manager.FindBySession(5).has_value());
    EXPECT_TRUE(st.manager.ValidateConsistency().IsHealthy());
}

// --- (2) Bubble includes the player position (via the source it binds) --------
TEST(PlayerIntegrationStep14, BubbleSeesPlayerPosition)
{
    ComposedStack st;
    ASSERT_TRUE(st.session.Admit(net::ConnectionId{7}, 100).HasValue());
    st.service.Tick(0.016);

    const std::vector<world::PlayerPosition> feed = st.source.ActivePlayers();
    ASSERT_EQ(feed.size(), 1u);
    const auto id = st.manager.FindByConnection(net::ConnectionId{7})->id;
    EXPECT_EQ(feed[0].id.value, id.value);
}

// --- (4) Disconnect preserves the record (suspended, not destroyed) -----------
TEST(PlayerIntegrationStep14, DisconnectPreservesRecord)
{
    ComposedStack st;
    ASSERT_TRUE(st.session.Admit(net::ConnectionId{5}, 100).HasValue());
    st.service.Tick(0.016);
    const auto entityBefore = st.manager.FindByConnection(net::ConnectionId{5})->entity;

    st.session.Remove(net::ConnectionId{5}, net::DisconnectReason::Timeout);
    st.service.Tick(0.016);

    EXPECT_EQ(st.manager.PlayerCount(), 1u);
    EXPECT_FALSE(st.manager.FindByConnection(net::ConnectionId{5}).has_value());
    player::PlayerDiagnostics diag(st.manager);
    EXPECT_NE(diag.DumpPlayers().find("lifecycle=Suspended"), std::string::npos);
    EXPECT_NE(diag.DumpPlayers().find(std::string("entity=") + std::to_string(entityBefore.value)),
              std::string::npos);
}

// --- (5) Reconnect keeps the SAME entity, no duplicate (FR-6) ------------------
// Sprint-006 Session::TryReclaim is a reserved stub (full reclaim: Sprint-012),
// so a reconnect attempt yields TokenUnknown; the retained record keeps its
// entity and NO new entity/player is created.
TEST(PlayerIntegrationStep14, ReconnectKeepsSameEntityNoDuplicate)
{
    ComposedStack st;
    ASSERT_TRUE(st.session.Admit(net::ConnectionId{5}, 100).HasValue());
    st.service.Tick(0.016);
    const auto entityBefore = st.manager.FindByConnection(net::ConnectionId{5})->entity;

    st.session.Remove(net::ConnectionId{5}, net::DisconnectReason::Graceful);
    st.service.Tick(0.016);

    EXPECT_EQ(st.manager.ApplyReclaim(net::ConnectionId{9}, 0xABCD, 300), player::ReconnectOutcome::TokenUnknown);
    EXPECT_EQ(st.manager.PlayerCount(), 1u);

    bool sameEntity = false;
    for (const auto& r : st.manager.Registry().Records())
    {
        if (r.entity.value == entityBefore.value)
        {
            sameEntity = true;
        }
    }
    EXPECT_TRUE(sameEntity);
    EXPECT_TRUE(st.manager.ValidateConsistency().IsHealthy());
}

// --- (6) Full-flow replay identity through the composed stack ------------------
TEST(PlayerIntegrationStep14, FullFlowReplayIdentity)
{
    auto run = [](std::string& signature) {
        ComposedStack st(/*respawnDelay*/ 5);
        (void)st.session.Admit(net::ConnectionId{5}, 100);
        (void)st.session.Admit(net::ConnectionId{6}, 101);
        st.service.Tick(0.016);
        const auto id5 = st.manager.FindByConnection(net::ConnectionId{5})->id;
        (void)st.manager.NotifyDeath(id5, 200);
        (void)st.manager.RequestRespawn(id5, 205);
        st.session.Remove(net::ConnectionId{6}, net::DisconnectReason::Timeout);
        st.service.Tick(0.016);

        EXPECT_TRUE(st.manager.ValidateConsistency().IsHealthy());
        player::PlayerDiagnostics diag(st.manager);
        signature = diag.DumpPlayers();
    };
    std::string s1;
    std::string s2;
    run(s1);
    run(s2);
    EXPECT_EQ(s1, s2);
}
