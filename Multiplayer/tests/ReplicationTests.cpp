// STALKER-MP — Replication subsystem tests (Sprint-009)
//
// Step 1: replication value types, enumerations, POD structures, and const char*
//         name helpers. Engine-free and OS-free. Types only — no configuration,
//         update, registry, interest, delta, classifier, queue, packet, worker,
//         manager, or diagnostics.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/core/Config.h"
#include "stalkermp/replication/IReplicationView.h"
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationTypes.h"
#include "stalkermp/replication/ReplicationUpdate.h"

using namespace stalkermp;

namespace
{
    replication::EntityReplicationState RE(std::uint32_t id)
    {
        replication::EntityReplicationState e{};
        e.id = world::EntityId{id};
        e.stateFlags = id * 10u;
        return e;
    }
    replication::PlayerReplicationState RP(std::uint32_t id, std::uint32_t entity)
    {
        replication::PlayerReplicationState p{};
        p.id = player::PlayerId{id};
        p.entity = world::EntityId{entity};
        return p;
    }
} // namespace

// --- Enum layout: fixed std::uint8_t underlying type (deterministic ABI) -----
TEST(ReplicationTypesStep1, EnumsHaveUint8UnderlyingType)
{
    static_assert(std::is_same_v<std::underlying_type_t<replication::ReplicationReliability>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<replication::ReplicationPriority>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<replication::ReplicationChannel>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<replication::ReplicationState>, std::uint8_t>);
    static_assert(std::is_same_v<std::underlying_type_t<replication::ReplicationOutcome>, std::uint8_t>);
    SUCCEED();
}

// --- Identity: none sentinel + equality ---------------------------------------
TEST(ReplicationTypesStep1, IdentityNoneAndEquality)
{
    constexpr replication::ClientId noneClient{};
    EXPECT_EQ(noneClient.value, 0u);
    EXPECT_TRUE(noneClient.IsNone());
    constexpr replication::ClientId a{7};
    constexpr replication::ClientId b{7};
    constexpr replication::ClientId c{8};
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a.IsNone());

    constexpr replication::ReplicationId noneRepl{};
    EXPECT_TRUE(noneRepl.IsNone());
    constexpr replication::ReplicationId r1{3};
    constexpr replication::ReplicationId r2{3};
    constexpr replication::ReplicationId r3{4};
    EXPECT_TRUE(r1 == r2);
    EXPECT_TRUE(r1 != r3);
}

// --- POD-style aggregates: trivially copyable + documented defaults -----------
TEST(ReplicationTypesStep1, PodDefaults)
{
    static_assert(std::is_trivially_copyable_v<replication::EntityReplicationState>);
    static_assert(std::is_trivially_copyable_v<replication::PlayerReplicationState>);
    static_assert(std::is_trivially_copyable_v<replication::ReplicationMetadata>);
    static_assert(std::is_trivially_copyable_v<replication::ReplicationStatistics>);
    static_assert(std::is_trivially_copyable_v<replication::ReplicationConsistency>);

    const replication::EntityReplicationState e{};
    EXPECT_EQ(e.id.value, 0u);
    EXPECT_EQ(e.stateFlags, 0u);
    EXPECT_EQ(e.version, 0u);
    EXPECT_EQ(e.priority, replication::ReplicationPriority::Low);
    EXPECT_EQ(e.reliability, replication::ReplicationReliability::Unreliable);

    const replication::PlayerReplicationState p{};
    EXPECT_EQ(p.id.value, 0u);
    EXPECT_EQ(p.entity.value, 0u);
    EXPECT_EQ(p.connectionState, player::PlayerConnectionState::Connected);
    EXPECT_EQ(p.authorityFlags, 0u);

    const replication::ReplicationMetadata m{};
    EXPECT_EQ(m.id.value, 0u);
    EXPECT_EQ(m.client.value, 0u);
    EXPECT_EQ(m.sourceSnapshotId, 0u);
    EXPECT_EQ(m.sourceSnapshotTick, 0u);
    EXPECT_EQ(m.entityCount, 0u);
    EXPECT_EQ(m.playerCount, 0u);
    EXPECT_EQ(m.byteCount, 0u);
    EXPECT_EQ(m.reliability, replication::ReplicationReliability::Unreliable);
    EXPECT_EQ(m.state, replication::ReplicationState::Pending);
    EXPECT_EQ(m.timestampWallClock, 0u);

    const replication::ReplicationStatistics s{};
    EXPECT_EQ(s.updatesBuilt, 0u);
    EXPECT_EQ(s.updatesSent, 0u);
    EXPECT_EQ(s.updatesDropped, 0u);
    EXPECT_EQ(s.bytesSent, 0u);
    EXPECT_EQ(s.activeClients, 0u);
}

// --- Consistency: healthy by default; any false clears health -----------------
TEST(ReplicationTypesStep1, ConsistencyHealth)
{
    replication::ReplicationConsistency c{};
    EXPECT_TRUE(c.IsHealthy());
    c.idMonotonic = false;
    EXPECT_FALSE(c.IsHealthy());
    c = replication::ReplicationConsistency{};
    c.noLiveObjectCaptured = false;
    EXPECT_FALSE(c.IsHealthy());
    c = replication::ReplicationConsistency{};
    c.noSimulationMutation = false;
    EXPECT_FALSE(c.IsHealthy());
}

// --- Name() helpers are total (incl. out-of-range sentinel) -------------------
TEST(ReplicationTypesStep1, ReliabilityAndPriorityNamesTotal)
{
    EXPECT_STREQ(replication::ReplicationReliabilityName(replication::ReplicationReliability::Unreliable), "Unreliable");
    EXPECT_STREQ(replication::ReplicationReliabilityName(replication::ReplicationReliability::Reliable), "Reliable");
    EXPECT_STREQ(replication::ReplicationReliabilityName(static_cast<replication::ReplicationReliability>(200)), "Unknown");

    EXPECT_STREQ(replication::ReplicationPriorityName(replication::ReplicationPriority::Low), "Low");
    EXPECT_STREQ(replication::ReplicationPriorityName(replication::ReplicationPriority::Medium), "Medium");
    EXPECT_STREQ(replication::ReplicationPriorityName(replication::ReplicationPriority::High), "High");
    EXPECT_STREQ(replication::ReplicationPriorityName(static_cast<replication::ReplicationPriority>(200)), "Unknown");
}

TEST(ReplicationTypesStep1, ChannelStateOutcomeNamesTotal)
{
    EXPECT_STREQ(replication::ReplicationChannelName(replication::ReplicationChannel::Unreliable), "Unreliable");
    EXPECT_STREQ(replication::ReplicationChannelName(replication::ReplicationChannel::Reliable), "Reliable");
    EXPECT_STREQ(replication::ReplicationChannelName(replication::ReplicationChannel::Control), "Control");
    EXPECT_STREQ(replication::ReplicationChannelName(static_cast<replication::ReplicationChannel>(200)), "Unknown");

    EXPECT_STREQ(replication::ReplicationStateName(replication::ReplicationState::Pending), "Pending");
    EXPECT_STREQ(replication::ReplicationStateName(replication::ReplicationState::Built), "Built");
    EXPECT_STREQ(replication::ReplicationStateName(replication::ReplicationState::Queued), "Queued");
    EXPECT_STREQ(replication::ReplicationStateName(replication::ReplicationState::Sent), "Sent");
    EXPECT_STREQ(replication::ReplicationStateName(replication::ReplicationState::Acknowledged), "Acknowledged");
    EXPECT_STREQ(replication::ReplicationStateName(replication::ReplicationState::Dropped), "Dropped");
    EXPECT_STREQ(replication::ReplicationStateName(static_cast<replication::ReplicationState>(200)), "Unknown");

    EXPECT_STREQ(replication::ReplicationOutcomeName(replication::ReplicationOutcome::Ok), "Ok");
    EXPECT_STREQ(replication::ReplicationOutcomeName(replication::ReplicationOutcome::ClientUnknown), "ClientUnknown");
    EXPECT_STREQ(replication::ReplicationOutcomeName(replication::ReplicationOutcome::SnapshotUnavailable), "SnapshotUnavailable");
    EXPECT_STREQ(replication::ReplicationOutcomeName(replication::ReplicationOutcome::Overflow), "Overflow");
    EXPECT_STREQ(replication::ReplicationOutcomeName(replication::ReplicationOutcome::NotRelevant), "NotRelevant");
    EXPECT_STREQ(replication::ReplicationOutcomeName(static_cast<replication::ReplicationOutcome>(200)), "Unknown");
}

// ============================================================================
// Step 2 — ReplicationConfiguration::FromConfig
// ============================================================================

// --- Missing [replication] section => all documented defaults -----------------
TEST(ReplicationConfigStep2, DefaultsWhenSectionAbsent)
{
    core::ConfigStore store;
    const auto r = replication::ReplicationConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.maxClients, 64u);
    EXPECT_EQ(c.maxEntitiesPerUpdate, 1024u);
    EXPECT_EQ(c.maxPlayersPerUpdate, 64u);
    EXPECT_EQ(c.reliableQueueDepth, 256u);
    EXPECT_EQ(c.unreliableQueueDepth, 512u);
    EXPECT_EQ(c.retryLimit, 5u);
    EXPECT_EQ(c.bandwidthBudgetBytesPerTick, 65536u);
    EXPECT_EQ(c.interestRadiusMeters, 150u);
    EXPECT_EQ(c.updateBudgetTicks, 0u);
    EXPECT_EQ(c.version, 1u);
}

// --- Each field parses a valid supplied value (override) ----------------------
TEST(ReplicationConfigStep2, OverridesParsed)
{
    core::ConfigStore store;
    store.Set("replication", "max_clients", "128");
    store.Set("replication", "max_entities_per_update", "2048");
    store.Set("replication", "max_players_per_update", "32");
    store.Set("replication", "reliable_queue_depth", "300");
    store.Set("replication", "unreliable_queue_depth", "600");
    store.Set("replication", "retry_limit", "8");
    store.Set("replication", "bandwidth_budget_bytes_per_tick", "131072");
    store.Set("replication", "interest_radius_meters", "200");
    store.Set("replication", "update_budget_ticks", "3");
    store.Set("replication", "version", "2");
    const auto r = replication::ReplicationConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    const auto& c = r.Value();
    EXPECT_EQ(c.maxClients, 128u);
    EXPECT_EQ(c.maxEntitiesPerUpdate, 2048u);
    EXPECT_EQ(c.maxPlayersPerUpdate, 32u);
    EXPECT_EQ(c.reliableQueueDepth, 300u);
    EXPECT_EQ(c.unreliableQueueDepth, 600u);
    EXPECT_EQ(c.retryLimit, 8u);
    EXPECT_EQ(c.bandwidthBudgetBytesPerTick, 131072u);
    EXPECT_EQ(c.interestRadiusMeters, 200u);
    EXPECT_EQ(c.updateBudgetTicks, 3u);
    EXPECT_EQ(c.version, 2u);
}

// --- Advisory/unbounded fields accept 0 ---------------------------------------
TEST(ReplicationConfigStep2, AdvisoryFieldsAcceptZero)
{
    core::ConfigStore store;
    store.Set("replication", "bandwidth_budget_bytes_per_tick", "0");
    store.Set("replication", "interest_radius_meters", "0");
    store.Set("replication", "update_budget_ticks", "0");
    const auto r = replication::ReplicationConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().bandwidthBudgetBytesPerTick, 0u);
    EXPECT_EQ(r.Value().interestRadiusMeters, 0u);
    EXPECT_EQ(r.Value().updateBudgetTicks, 0u);
}

// --- Invalid per-field values are rejected (value outcome) --------------------
TEST(ReplicationConfigStep2, InvalidValuesRejected)
{
    {
        core::ConfigStore store;
        store.Set("replication", "max_clients", "0"); // below min 1
        EXPECT_FALSE(replication::ReplicationConfiguration::FromConfig(store).HasValue());
    }
    {
        core::ConfigStore store;
        store.Set("replication", "version", "0"); // below min 1
        EXPECT_FALSE(replication::ReplicationConfiguration::FromConfig(store).HasValue());
    }
    {
        core::ConfigStore store;
        store.Set("replication", "reliable_queue_depth", "0"); // below min 1
        EXPECT_FALSE(replication::ReplicationConfiguration::FromConfig(store).HasValue());
    }
    {
        core::ConfigStore store;
        store.Set("replication", "retry_limit", "-1"); // below min 1
        EXPECT_FALSE(replication::ReplicationConfiguration::FromConfig(store).HasValue());
    }
    {
        core::ConfigStore store;
        store.Set("replication", "max_entities_per_update", "0"); // below min 1
        EXPECT_FALSE(replication::ReplicationConfiguration::FromConfig(store).HasValue());
    }
}

// --- Boundary minimums (== 1) are accepted ------------------------------------
TEST(ReplicationConfigStep2, BoundaryMinimumsAccepted)
{
    core::ConfigStore store;
    store.Set("replication", "max_clients", "1");
    store.Set("replication", "max_entities_per_update", "1");
    store.Set("replication", "max_players_per_update", "1");
    store.Set("replication", "reliable_queue_depth", "1");
    store.Set("replication", "unreliable_queue_depth", "1");
    store.Set("replication", "retry_limit", "1");
    store.Set("replication", "version", "1");
    const auto r = replication::ReplicationConfiguration::FromConfig(store);
    ASSERT_TRUE(r.HasValue());
    EXPECT_EQ(r.Value().maxClients, 1u);
    EXPECT_EQ(r.Value().retryLimit, 1u);
    EXPECT_EQ(r.Value().version, 1u);
}

// ============================================================================
// Step 3 — Immutable ReplicationUpdate + IReplicationView
// ============================================================================

// --- Build -> Finalize: content captured, counts stamped, state Built ---------
TEST(ReplicationUpdateStep3, BuildAndFinalize)
{
    replication::ReplicationUpdate update;
    update.BeginBuild(replication::ReplicationId{42}, replication::ClientId{7},
                      /*sourceSnapshotId*/ 1000, /*sourceSnapshotTick*/ 500);
    EXPECT_EQ(update.State(), replication::ReplicationState::Pending);
    EXPECT_FALSE(update.IsFinalized());

    ASSERT_TRUE(update.AddEntity(RE(1)).HasValue());
    ASSERT_TRUE(update.AddEntity(RE(5)).HasValue());
    ASSERT_TRUE(update.AddPlayer(RP(1, 1)).HasValue());
    ASSERT_TRUE(update.SetReliability(replication::ReplicationReliability::Reliable).HasValue());

    ASSERT_TRUE(update.Finalize().HasValue());
    EXPECT_EQ(update.State(), replication::ReplicationState::Built);
    EXPECT_TRUE(update.IsFinalized());

    // Consumer surface (const-only) reflects the captured content.
    const replication::IReplicationView& view = update;
    EXPECT_EQ(view.Metadata().id.value, 42u);
    EXPECT_EQ(view.Metadata().client.value, 7u);
    EXPECT_EQ(view.Metadata().sourceSnapshotId, 1000u);
    EXPECT_EQ(view.Metadata().sourceSnapshotTick, 500u);
    EXPECT_EQ(view.Metadata().entityCount, 2u);
    EXPECT_EQ(view.Metadata().playerCount, 1u);
    EXPECT_EQ(view.Metadata().reliability, replication::ReplicationReliability::Reliable);
    ASSERT_EQ(view.Entities().size(), 2u);
    EXPECT_EQ(view.Entities()[0].id.value, 1u);
    EXPECT_EQ(view.Entities()[1].id.value, 5u);
    ASSERT_EQ(view.Players().size(), 1u);
    EXPECT_EQ(view.Players()[0].id.value, 1u);
}

// --- Deterministic ordering: non-ascending / duplicate / zero rejected --------
TEST(ReplicationUpdateStep3, AscendingUniqueEnforced)
{
    replication::ReplicationUpdate update;
    update.BeginBuild(replication::ReplicationId{1}, replication::ClientId{1}, 1, 1);
    ASSERT_TRUE(update.AddEntity(RE(3)).HasValue());
    EXPECT_FALSE(update.AddEntity(RE(3)).HasValue()); // duplicate
    EXPECT_FALSE(update.AddEntity(RE(2)).HasValue()); // out of order
    EXPECT_FALSE(update.AddEntity(RE(0)).HasValue()); // none id
    ASSERT_TRUE(update.AddEntity(RE(4)).HasValue());  // ascending ok

    ASSERT_TRUE(update.AddPlayer(RP(2, 20)).HasValue());
    EXPECT_FALSE(update.AddPlayer(RP(2, 21)).HasValue()); // duplicate
    EXPECT_FALSE(update.AddPlayer(RP(1, 22)).HasValue()); // out of order
    EXPECT_FALSE(update.AddPlayer(RP(0, 23)).HasValue()); // none id

    ASSERT_EQ(update.Entities().size(), 2u);
    ASSERT_EQ(update.Players().size(), 1u);
}

// --- Immutability: mutating a Finalized update is rejected (E-G1-R) ------------
TEST(ReplicationUpdateStep3, ImmutableAfterFinalize)
{
    replication::ReplicationUpdate update;
    update.BeginBuild(replication::ReplicationId{1}, replication::ClientId{1}, 1, 1);
    ASSERT_TRUE(update.AddEntity(RE(1)).HasValue());
    ASSERT_TRUE(update.Finalize().HasValue());

    // Every mutating operation now fails; content is unchanged.
    EXPECT_FALSE(update.AddEntity(RE(2)).HasValue());
    EXPECT_FALSE(update.AddPlayer(RP(1, 1)).HasValue());
    EXPECT_FALSE(update.SetReliability(replication::ReplicationReliability::Reliable).HasValue());
    EXPECT_FALSE(update.Finalize().HasValue()); // double-finalize rejected
    EXPECT_EQ(update.Entities().size(), 1u);
    EXPECT_EQ(update.State(), replication::ReplicationState::Built);
}

// --- Deterministic replay: identical build sequence => identical content -------
TEST(ReplicationUpdateStep3, DeterministicBuild)
{
    auto build = [](replication::ReplicationUpdate& u) {
        u.BeginBuild(replication::ReplicationId{7}, replication::ClientId{3}, 500, 250);
        (void)u.AddEntity(RE(2));
        (void)u.AddEntity(RE(9));
        (void)u.AddPlayer(RP(3, 30));
        (void)u.SetReliability(replication::ReplicationReliability::Reliable);
        (void)u.Finalize();
    };
    replication::ReplicationUpdate a;
    replication::ReplicationUpdate b;
    build(a);
    build(b);
    ASSERT_EQ(a.Entities().size(), b.Entities().size());
    for (std::size_t i = 0; i < a.Entities().size(); ++i)
    {
        EXPECT_EQ(a.Entities()[i].id.value, b.Entities()[i].id.value);
        EXPECT_EQ(a.Entities()[i].stateFlags, b.Entities()[i].stateFlags);
    }
    EXPECT_EQ(a.Metadata().id.value, b.Metadata().id.value);
    EXPECT_EQ(a.Metadata().entityCount, b.Metadata().entityCount);
    EXPECT_EQ(a.Metadata().reliability, b.Metadata().reliability);
    // No wall-clock in content identity (timestampWallClock unset).
    EXPECT_EQ(a.Metadata().timestampWallClock, b.Metadata().timestampWallClock);
}
