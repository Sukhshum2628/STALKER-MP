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
#include "stalkermp/replication/BubbleInterestPolicy.h"
#include "stalkermp/replication/DeltaEncoder.h"
#include "stalkermp/replication/IBubbleMembershipSource.h"
#include "stalkermp/net/ProtocolConstants.h"
#include "stalkermp/net/ReplicationMessageIds.h"
#include "stalkermp/replication/ReplicationClassifier.h"
#include "stalkermp/replication/ReplicationPacketBuilder.h"
#include "stalkermp/replication/ReplicationQueues.h"
#include "stalkermp/replication/ReplicationClientRegistry.h"
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationTypes.h"
#include "stalkermp/replication/ReplicationUpdate.h"
#include "stalkermp/snapshot/SimulationSnapshot.h"

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

// ============================================================================
// Step 4 — ReplicationClientRegistry (per-client baselines)
// ============================================================================

namespace
{
    world::PlayerPosition Focus(std::uint32_t playerId, float x)
    {
        world::PlayerPosition f{};
        f.id = world::PlayerId{playerId};
        f.position = world::Vec3{x, 0.0f, 0.0f};
        return f;
    }
} // namespace

// --- Lifecycle: register / find (by id + connection) / unregister -------------
TEST(ReplicationClientRegistryStep4, RegisterFindUnregister)
{
    replication::ReplicationClientRegistry registry{8};
    EXPECT_EQ(registry.Count(), 0u);
    EXPECT_EQ(registry.Capacity(), 8u);

    const auto a = registry.Register(net::ConnectionId{5}, Focus(1, 1.0f));
    ASSERT_TRUE(a.HasValue());
    const auto b = registry.Register(net::ConnectionId{9}, Focus(2, 2.0f));
    ASSERT_TRUE(b.HasValue());
    EXPECT_EQ(registry.Count(), 2u);

    // Ascending, non-reused ids starting at 1.
    EXPECT_EQ(a.Value().value, 1u);
    EXPECT_EQ(b.Value().value, 2u);

    const replication::ClientRecord* byId = registry.FindById(a.Value());
    ASSERT_NE(byId, nullptr);
    EXPECT_EQ(byId->connection.value, 5u);
    const replication::ClientRecord* byConn = registry.FindByConnection(net::ConnectionId{9});
    ASSERT_NE(byConn, nullptr);
    EXPECT_EQ(byConn->id.value, 2u);
    EXPECT_EQ(registry.FindById(replication::ClientId{999}), nullptr);

    // Duplicate connection is rejected (value outcome).
    EXPECT_FALSE(registry.Register(net::ConnectionId{5}, Focus(3, 3.0f)).HasValue());

    // Unregister; the id is never reused.
    ASSERT_TRUE(registry.Unregister(a.Value()).HasValue());
    EXPECT_EQ(registry.Count(), 1u);
    EXPECT_EQ(registry.FindById(a.Value()), nullptr);
    EXPECT_FALSE(registry.Unregister(a.Value()).HasValue()); // already gone
    const auto c = registry.Register(net::ConnectionId{5}, Focus(3, 3.0f));
    ASSERT_TRUE(c.HasValue());
    EXPECT_EQ(c.Value().value, 3u); // NOT reusing id 1
}

// --- Capacity bounded by maxClients (value outcome) ---------------------------
TEST(ReplicationClientRegistryStep4, CapacityBounded)
{
    replication::ReplicationClientRegistry registry{2};
    ASSERT_TRUE(registry.Register(net::ConnectionId{1}, Focus(1, 0)).HasValue());
    ASSERT_TRUE(registry.Register(net::ConnectionId{2}, Focus(2, 0)).HasValue());
    EXPECT_TRUE(registry.Full());
    EXPECT_FALSE(registry.Register(net::ConnectionId{3}, Focus(3, 0)).HasValue()); // over capacity
    EXPECT_EQ(registry.Count(), 2u);
}

// --- Deterministic ascending order + focus update -----------------------------
TEST(ReplicationClientRegistryStep4, AscendingOrderAndFocus)
{
    replication::ReplicationClientRegistry registry{8};
    (void)registry.Register(net::ConnectionId{7}, Focus(1, 1.0f));
    (void)registry.Register(net::ConnectionId{4}, Focus(2, 2.0f));
    (void)registry.Register(net::ConnectionId{9}, Focus(3, 3.0f));

    const auto active = registry.ActiveClients();
    ASSERT_EQ(active.size(), 3u);
    for (std::size_t i = 1; i < active.size(); ++i)
    {
        EXPECT_LT(active[i - 1].id.value, active[i].id.value); // ascending
    }

    ASSERT_TRUE(registry.UpdateFocus(replication::ClientId{2}, Focus(2, 42.0f)).HasValue());
    EXPECT_FLOAT_EQ(registry.FindById(replication::ClientId{2})->focus.position.x, 42.0f);
    EXPECT_FALSE(registry.UpdateFocus(replication::ClientId{999}, Focus(9, 0)).HasValue());
}

// --- Monotonic acknowledgement updates ----------------------------------------
TEST(ReplicationClientRegistryStep4, MonotonicAck)
{
    replication::ReplicationClientRegistry registry{4};
    const auto id = registry.Register(net::ConnectionId{1}, Focus(1, 0));
    ASSERT_TRUE(id.HasValue());

    ASSERT_TRUE(registry.RecordAck(id.Value(), 10, 100).HasValue());
    EXPECT_EQ(registry.FindById(id.Value())->lastAckedReplicationId, 10u);
    EXPECT_EQ(registry.FindById(id.Value())->lastAckedSnapshotTick, 100u);

    // Newer ack advances the baseline.
    ASSERT_TRUE(registry.RecordAck(id.Value(), 15, 150).HasValue());
    EXPECT_EQ(registry.FindById(id.Value())->lastAckedReplicationId, 15u);

    // Older/equal acks are ignored (benign; baseline unchanged).
    ASSERT_TRUE(registry.RecordAck(id.Value(), 15, 999).HasValue());
    ASSERT_TRUE(registry.RecordAck(id.Value(), 5, 999).HasValue());
    EXPECT_EQ(registry.FindById(id.Value())->lastAckedReplicationId, 15u);
    EXPECT_EQ(registry.FindById(id.Value())->lastAckedSnapshotTick, 150u);

    EXPECT_FALSE(registry.RecordAck(replication::ClientId{999}, 1, 1).HasValue()); // unknown
}

// --- Consistency validation ---------------------------------------------------
TEST(ReplicationClientRegistryStep4, ConsistencyValid)
{
    replication::ReplicationClientRegistry registry{8};
    EXPECT_TRUE(registry.ValidateConsistency()); // empty is consistent
    (void)registry.Register(net::ConnectionId{1}, Focus(1, 0));
    (void)registry.Register(net::ConnectionId{2}, Focus(2, 0));
    (void)registry.Register(net::ConnectionId{3}, Focus(3, 0));
    ASSERT_TRUE(registry.Unregister(replication::ClientId{2}).HasValue());
    EXPECT_TRUE(registry.ValidateConsistency()); // still ascending + unique after erase
    EXPECT_EQ(registry.Count(), 2u);
}

// ============================================================================
// Step 5 — Interest seam + BubbleInterestPolicy
// ============================================================================

namespace
{
    // Fake bubble membership: the configured ids are Inside; all others Outside.
    class FakeMembership final : public replication::IBubbleMembershipSource
    {
    public:
        void SetInside(std::uint32_t id) { m_inside.push_back(id); }
        [[nodiscard]] world::BubbleMembership MembershipOf(world::EntityId id) const noexcept override
        {
            for (const std::uint32_t inside : m_inside)
            {
                if (inside == id.value)
                {
                    return world::BubbleMembership::Inside;
                }
            }
            return world::BubbleMembership::Outside;
        }

    private:
        std::vector<std::uint32_t> m_inside;
    };

    // Build a finalized snapshot with entities id=1..count, each at position x=id.
    void BuildSnapshot(snapshot::SimulationSnapshot& snap, std::uint32_t count)
    {
        snap.BeginBuild(snapshot::SnapshotId{1}, 1, 1);
        for (std::uint32_t i = 1; i <= count; ++i)
        {
            snapshot::EntitySnapshot e{};
            e.id = world::EntityId{i};
            e.position = world::Vec3{static_cast<float>(i), 0.0f, 0.0f};
            (void)snap.AddEntity(e);
        }
        (void)snap.Finalize();
    }

    replication::ClientRecord ClientAt(float x)
    {
        replication::ClientRecord c{};
        c.id = replication::ClientId{1};
        c.focus.position = world::Vec3{x, 0.0f, 0.0f};
        return c;
    }
} // namespace

// --- Membership-driven relevance: only in-region entities selected ------------
TEST(InterestStep5, MembershipDrivenSelection)
{
    snapshot::SimulationSnapshot snap;
    BuildSnapshot(snap, 5);
    FakeMembership membership;
    membership.SetInside(2);
    membership.SetInside(4);

    replication::BubbleInterestPolicy policy{membership, /*interestRadiusMeters*/ 0}; // membership only
    std::vector<world::EntityId> out;
    policy.SelectRelevant(ClientAt(1000.0f), snap, out); // focus far away -> radius irrelevant

    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].value, 2u);
    EXPECT_EQ(out[1].value, 4u);
}

// --- Ascending, unique output -------------------------------------------------
TEST(InterestStep5, AscendingUniqueOutput)
{
    snapshot::SimulationSnapshot snap;
    BuildSnapshot(snap, 6);
    FakeMembership membership;
    for (std::uint32_t i = 1; i <= 6; ++i) membership.SetInside(i); // all relevant

    replication::BubbleInterestPolicy policy{membership, 0};
    std::vector<world::EntityId> out;
    policy.SelectRelevant(ClientAt(0.0f), snap, out);

    ASSERT_EQ(out.size(), 6u);
    for (std::size_t i = 1; i < out.size(); ++i)
    {
        EXPECT_LT(out[i - 1].value, out[i].value); // strictly ascending, unique
    }
}

// --- Radius policy: entities within interestRadiusMeters of focus are relevant -
TEST(InterestStep5, RadiusPolicy)
{
    snapshot::SimulationSnapshot snap;
    BuildSnapshot(snap, 5); // entities at x = 1..5
    FakeMembership membership; // none Inside

    // Focus at x=1, radius 2 -> entities at x in [1..3] (ids 1,2,3) are within.
    replication::BubbleInterestPolicy policy{membership, /*radius*/ 2};
    std::vector<world::EntityId> out;
    policy.SelectRelevant(ClientAt(1.0f), snap, out);

    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].value, 1u);
    EXPECT_EQ(out[1].value, 2u);
    EXPECT_EQ(out[2].value, 3u);
}

// --- Append-only + deterministic selection ------------------------------------
TEST(InterestStep5, AppendOnlyAndDeterministic)
{
    snapshot::SimulationSnapshot snap;
    BuildSnapshot(snap, 4);
    FakeMembership membership;
    membership.SetInside(2);
    membership.SetInside(3);
    replication::BubbleInterestPolicy policy{membership, 0};

    // Append-only: a pre-existing entry is preserved.
    std::vector<world::EntityId> out;
    out.push_back(world::EntityId{100});
    policy.SelectRelevant(ClientAt(0.0f), snap, out);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].value, 100u); // preserved
    EXPECT_EQ(out[1].value, 2u);
    EXPECT_EQ(out[2].value, 3u);

    // Deterministic: two fresh selections produce identical results.
    std::vector<world::EntityId> a;
    std::vector<world::EntityId> b;
    policy.SelectRelevant(ClientAt(0.0f), snap, a);
    policy.SelectRelevant(ClientAt(0.0f), snap, b);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        EXPECT_EQ(a[i].value, b[i].value);
    }
}

// ============================================================================
// Step 6 — Deterministic delta generation
// ============================================================================

namespace
{
    replication::EntityReplicationState ES(std::uint32_t id, float x, std::uint32_t flags, std::uint32_t version)
    {
        replication::EntityReplicationState e{};
        e.id = world::EntityId{id};
        e.position = world::Vec3{x, 0.0f, 0.0f};
        e.stateFlags = flags;
        e.version = version;
        return e;
    }
    replication::PlayerReplicationState PS(std::uint32_t id, float x, std::uint32_t authority, std::uint32_t version)
    {
        replication::PlayerReplicationState p{};
        p.id = player::PlayerId{id};
        p.position = world::Vec3{x, 0.0f, 0.0f};
        p.authorityFlags = authority;
        p.version = version;
        return p;
    }
} // namespace

// --- Added / changed / removed classification; unchanged omitted --------------
TEST(DeltaStep6, AddedChangedRemovedUnchanged)
{
    const std::vector<replication::EntityReplicationState> baseline{
        ES(1, 1.0f, 10, 3), ES(2, 2.0f, 20, 4), ES(3, 3.0f, 30, 5)};
    const std::vector<replication::EntityReplicationState> current{
        ES(2, 2.5f, 20, 0), // changed (position differs)
        ES(3, 3.0f, 30, 0), // unchanged (tracked fields equal)
        ES(4, 4.0f, 40, 0)};// added

    const auto r = replication::EncodeEntityDelta(baseline, current, /*maxEntitiesPerUpdate*/ 1024);
    ASSERT_TRUE(r.HasValue());
    const auto& cs = r.Value();

    ASSERT_EQ(cs.added.size(), 1u);
    EXPECT_EQ(cs.added[0].id.value, 4u);
    EXPECT_EQ(cs.added[0].version, 1u); // added stamped version 1

    ASSERT_EQ(cs.changed.size(), 1u);
    EXPECT_EQ(cs.changed[0].id.value, 2u);
    EXPECT_EQ(cs.changed[0].version, 5u); // baseline 4 -> bumped to 5

    ASSERT_EQ(cs.removed.size(), 1u);
    EXPECT_EQ(cs.removed[0].value, 1u); // in baseline, absent in current

    // Entity 3 unchanged => omitted from added/changed.
}

// --- Ascending, deterministic output ------------------------------------------
TEST(DeltaStep6, AscendingAndDeterministic)
{
    const std::vector<replication::EntityReplicationState> baseline{ES(2, 2.0f, 20, 1)};
    const std::vector<replication::EntityReplicationState> current{
        ES(1, 1.0f, 10, 0), ES(3, 3.0f, 30, 0), ES(5, 5.0f, 50, 0)};

    const auto r1 = replication::EncodeEntityDelta(baseline, current, 1024);
    const auto r2 = replication::EncodeEntityDelta(baseline, current, 1024);
    ASSERT_TRUE(r1.HasValue());
    ASSERT_TRUE(r2.HasValue());
    ASSERT_EQ(r1.Value().added.size(), 3u);
    for (std::size_t i = 1; i < r1.Value().added.size(); ++i)
    {
        EXPECT_LT(r1.Value().added[i - 1].id.value, r1.Value().added[i].id.value); // ascending
    }
    ASSERT_EQ(r1.Value().added.size(), r2.Value().added.size());
    for (std::size_t i = 0; i < r1.Value().added.size(); ++i)
    {
        EXPECT_EQ(r1.Value().added[i].id.value, r2.Value().added[i].id.value); // deterministic
    }
    EXPECT_EQ(r1.Value().removed.size(), 1u); // id 2 removed
    EXPECT_EQ(r1.Value().removed[0].value, 2u);
}

// --- Overflow handled via value outcome ---------------------------------------
TEST(DeltaStep6, OverflowValueOutcome)
{
    const std::vector<replication::EntityReplicationState> baseline{};
    const std::vector<replication::EntityReplicationState> current{
        ES(1, 1.0f, 10, 0), ES(2, 2.0f, 20, 0)}; // 2 added

    EXPECT_FALSE(replication::EncodeEntityDelta(baseline, current, /*max*/ 1).HasValue()); // 2 > 1
    EXPECT_TRUE(replication::EncodeEntityDelta(baseline, current, /*max*/ 2).HasValue());  // 2 <= 2
}

// --- Player delta: added + tracked-field changes ------------------------------
TEST(DeltaStep6, PlayerDelta)
{
    const std::vector<replication::PlayerReplicationState> baseline{PS(1, 1.0f, 0, 2), PS(2, 2.0f, 0, 3)};
    const std::vector<replication::PlayerReplicationState> current{
        PS(1, 1.0f, 0, 0), // unchanged
        PS(2, 2.0f, 7, 0), // changed (authorityFlags differ)
        PS(3, 3.0f, 0, 0)};// added

    const auto changed = replication::EncodePlayerDelta(baseline, current);
    ASSERT_EQ(changed.size(), 2u);
    EXPECT_EQ(changed[0].id.value, 2u);
    EXPECT_EQ(changed[0].version, 4u); // baseline 3 -> 4
    EXPECT_EQ(changed[1].id.value, 3u);
    EXPECT_EQ(changed[1].version, 1u); // added
}

// --- NextEntityBaseline: version carry/bump; removed dropped -------------------
TEST(DeltaStep6, NextBaseline)
{
    const std::vector<replication::EntityReplicationState> baseline{
        ES(1, 1.0f, 10, 3), ES(2, 2.0f, 20, 4)};
    const std::vector<replication::EntityReplicationState> current{
        ES(2, 2.5f, 20, 0), // changed
        ES(5, 5.0f, 50, 0)};// added; id 1 removed

    const auto next = replication::NextEntityBaseline(baseline, current);
    ASSERT_EQ(next.size(), 2u);
    EXPECT_EQ(next[0].id.value, 2u);
    EXPECT_EQ(next[0].version, 5u); // changed: 4 -> 5
    EXPECT_EQ(next[1].id.value, 5u);
    EXPECT_EQ(next[1].version, 1u); // added
    // id 1 (removed) is dropped from the next baseline.
}

// ============================================================================
// Step 7 — Reliability & priority classification
// ============================================================================

// --- Frozen Reliable / Unreliable sets (§7.9) ---------------------------------
TEST(ClassifierStep7, ReliabilityMappingFrozen)
{
    using K = replication::ReplicationChangeKind;
    using R = replication::ReplicationReliability;
    // §7.9 Reliable set.
    EXPECT_EQ(replication::ClassifyReliability(K::EntitySpawn), R::Reliable);
    EXPECT_EQ(replication::ClassifyReliability(K::EntityRemove), R::Reliable);
    EXPECT_EQ(replication::ClassifyReliability(K::Inventory), R::Reliable);
    EXPECT_EQ(replication::ClassifyReliability(K::QuestUpdate), R::Reliable);
    EXPECT_EQ(replication::ClassifyReliability(K::PlayerJoin), R::Reliable);
    EXPECT_EQ(replication::ClassifyReliability(K::PlayerLeave), R::Reliable);
    // §7.9 Unreliable set.
    EXPECT_EQ(replication::ClassifyReliability(K::Position), R::Unreliable);
    EXPECT_EQ(replication::ClassifyReliability(K::Rotation), R::Unreliable);
    EXPECT_EQ(replication::ClassifyReliability(K::Velocity), R::Unreliable);
    EXPECT_EQ(replication::ClassifyReliability(K::Animation), R::Unreliable);
    EXPECT_EQ(replication::ClassifyReliability(K::Camera), R::Unreliable);
}

// --- Frozen priority bands (§7.8) ---------------------------------------------
TEST(ClassifierStep7, PriorityMappingFrozen)
{
    using K = replication::ReplicationChangeKind;
    using P = replication::ReplicationPriority;
    // §7.8 High.
    EXPECT_EQ(replication::ClassifyPriority(K::Player), P::High);
    EXPECT_EQ(replication::ClassifyPriority(K::Combat), P::High);
    EXPECT_EQ(replication::ClassifyPriority(K::Damage), P::High);
    EXPECT_EQ(replication::ClassifyPriority(K::EntitySpawn), P::High);
    EXPECT_EQ(replication::ClassifyPriority(K::EntityRemove), P::High);
    // §7.8 Medium.
    EXPECT_EQ(replication::ClassifyPriority(K::NearbyNpc), P::Medium);
    EXPECT_EQ(replication::ClassifyPriority(K::Inventory), P::Medium);
    EXPECT_EQ(replication::ClassifyPriority(K::Animation), P::Medium);
    // §7.8 Low.
    EXPECT_EQ(replication::ClassifyPriority(K::AmbientObject), P::Low);
    EXPECT_EQ(replication::ClassifyPriority(K::WeatherUpdate), P::Low);
    EXPECT_EQ(replication::ClassifyPriority(K::DistantEntity), P::Low);
}

// --- Channel derives from reliability -----------------------------------------
TEST(ClassifierStep7, ChannelDerivesFromReliability)
{
    using K = replication::ReplicationChangeKind;
    using C = replication::ReplicationChannel;
    EXPECT_EQ(replication::ClassifyChannel(K::EntitySpawn), C::Reliable);
    EXPECT_EQ(replication::ClassifyChannel(K::Inventory), C::Reliable);
    EXPECT_EQ(replication::ClassifyChannel(K::Position), C::Unreliable);
    EXPECT_EQ(replication::ClassifyChannel(K::Animation), C::Unreliable);
}

// --- Total mapping + deterministic (constexpr) + Name() totality --------------
TEST(ClassifierStep7, TotalDeterministicAndNames)
{
    using K = replication::ReplicationChangeKind;
    // Compile-time totality/determinism spot checks (constexpr).
    static_assert(replication::ClassifyReliability(K::EntitySpawn) == replication::ReplicationReliability::Reliable);
    static_assert(replication::ClassifyPriority(K::Player) == replication::ReplicationPriority::High);
    static_assert(replication::ClassifyChannel(K::Position) == replication::ReplicationChannel::Unreliable);

    // Every kind classifies without hitting the fallthrough default; Name total.
    for (std::uint16_t v = 0; v <= 18; ++v)
    {
        const auto k = static_cast<K>(v);
        (void)replication::ClassifyReliability(k);
        (void)replication::ClassifyPriority(k);
        (void)replication::ClassifyChannel(k);
        EXPECT_STRNE(replication::ReplicationChangeKindName(k), "Unknown"); // 0..18 are all defined
    }
    EXPECT_STREQ(replication::ReplicationChangeKindName(static_cast<K>(200)), "Unknown");
    EXPECT_STREQ(replication::ReplicationChangeKindName(K::EntityRemove), "EntityRemove");
}

// --- Priority ordering is total (High > Medium > Low) -------------------------
TEST(ClassifierStep7, PriorityOrderingTotal)
{
    using P = replication::ReplicationPriority;
    EXPECT_GT(static_cast<int>(P::High), static_cast<int>(P::Medium));
    EXPECT_GT(static_cast<int>(P::Medium), static_cast<int>(P::Low));
}

// ============================================================================
// Step 8 — Replication queues
// ============================================================================

namespace
{
    replication::QueuedRecord QR(replication::ReplicationChangeKind kind, std::uint32_t id,
                                 replication::ReplicationReliability reliability)
    {
        replication::QueuedRecord r{};
        r.kind = kind;
        r.entity.id = world::EntityId{id};
        r.reliability = reliability;
        return r;
    }
    replication::ReplicationConfiguration QueueCfg(std::uint32_t reliableDepth, std::uint32_t unreliableDepth)
    {
        replication::ReplicationConfiguration c{};
        c.reliableQueueDepth = reliableDepth;
        c.unreliableQueueDepth = unreliableDepth;
        return c;
    }
} // namespace

// --- FIFO ordering, capacity, and overflow value outcome ----------------------
TEST(QueuesStep8, FifoCapacityOverflow)
{
    replication::FixedRecordQueue queue{3};
    EXPECT_EQ(queue.Capacity(), 3u);
    EXPECT_TRUE(queue.Empty());

    EXPECT_EQ(queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 1,
                               replication::ReplicationReliability::Reliable)),
              replication::ReplicationOutcome::Ok);
    EXPECT_EQ(queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 2,
                               replication::ReplicationReliability::Reliable)),
              replication::ReplicationOutcome::Ok);
    EXPECT_EQ(queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 3,
                               replication::ReplicationReliability::Reliable)),
              replication::ReplicationOutcome::Ok);
    EXPECT_TRUE(queue.Full());
    EXPECT_EQ(queue.Size(), 3u);

    // Overflow is a value outcome; the queue is unchanged.
    EXPECT_EQ(queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 4,
                               replication::ReplicationReliability::Reliable)),
              replication::ReplicationOutcome::Overflow);
    EXPECT_EQ(queue.Size(), 3u);

    // FIFO dequeue order: 1, 2, 3.
    EXPECT_EQ(queue.Dequeue()->entity.id.value, 1u);
    EXPECT_EQ(queue.Dequeue()->entity.id.value, 2u);
    EXPECT_EQ(queue.Dequeue()->entity.id.value, 3u);
    EXPECT_TRUE(queue.Empty());
    EXPECT_FALSE(queue.Dequeue().has_value()); // empty -> nullopt
}

// --- Ring reuse: no allocation, indices wrap ----------------------------------
TEST(QueuesStep8, RingReuse)
{
    replication::FixedRecordQueue queue{2};
    (void)queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 1, replication::ReplicationReliability::Reliable));
    (void)queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 2, replication::ReplicationReliability::Reliable));
    EXPECT_EQ(queue.Dequeue()->entity.id.value, 1u);
    // Re-enqueue wraps into the freed slot; capacity unchanged.
    EXPECT_EQ(queue.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 3, replication::ReplicationReliability::Reliable)),
              replication::ReplicationOutcome::Ok);
    EXPECT_EQ(queue.Capacity(), 2u);
    EXPECT_EQ(queue.Dequeue()->entity.id.value, 2u);
    EXPECT_EQ(queue.Dequeue()->entity.id.value, 3u);
}

// --- Independent reliable / unreliable routing --------------------------------
TEST(QueuesStep8, IndependentRouting)
{
    replication::ReplicationQueues queues{QueueCfg(8, 8)};
    EXPECT_TRUE(queues.Empty());

    EXPECT_EQ(queues.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, 1,
                                replication::ReplicationReliability::Reliable)),
              replication::ReplicationOutcome::Ok);
    EXPECT_EQ(queues.Enqueue(QR(replication::ReplicationChangeKind::Position, 2,
                                replication::ReplicationReliability::Unreliable)),
              replication::ReplicationOutcome::Ok);

    EXPECT_EQ(queues.Reliable().Size(), 1u);   // reliable record routed here
    EXPECT_EQ(queues.Unreliable().Size(), 1u); // unreliable record routed here
    EXPECT_EQ(queues.Size(), 2u);
    EXPECT_FALSE(queues.Empty());

    queues.Clear();
    EXPECT_TRUE(queues.Empty());
    EXPECT_EQ(queues.Reliable().Size(), 0u);
    EXPECT_EQ(queues.Unreliable().Size(), 0u);
}

// --- Change-set routing + budget enforcement (deterministic) ------------------
TEST(QueuesStep8, ChangeSetRoutingAndBudget)
{
    replication::ReplicationChangeSet changes;
    changes.added.push_back(ES(1, 1.0f, 10, 0));   // EntitySpawn -> reliable
    changes.added.push_back(ES(2, 2.0f, 20, 0));   // EntitySpawn -> reliable
    changes.changed.push_back(ES(3, 3.0f, 30, 0)); // Position    -> unreliable
    changes.removed.push_back(world::EntityId{9}); // EntityRemove-> reliable

    // Ample capacity: everything is routed; Ok.
    replication::ReplicationQueues ok{QueueCfg(8, 8)};
    EXPECT_EQ(ok.EnqueueChangeSet(changes), replication::ReplicationOutcome::Ok);
    EXPECT_EQ(ok.Reliable().Size(), 3u);   // 2 added + 1 removed
    EXPECT_EQ(ok.Unreliable().Size(), 1u); // 1 changed

    // Deterministic order within reliable queue: removed(9), added(1), added(2).
    EXPECT_EQ(ok.Reliable().Dequeue()->entity.id.value, 9u);
    EXPECT_EQ(ok.Reliable().Dequeue()->entity.id.value, 1u);
    EXPECT_EQ(ok.Reliable().Dequeue()->entity.id.value, 2u);

    // Tight reliable budget (1) overflows; value outcome, unreliable still fills.
    replication::ReplicationQueues tight{QueueCfg(1, 8)};
    EXPECT_EQ(tight.EnqueueChangeSet(changes), replication::ReplicationOutcome::Overflow);
    EXPECT_EQ(tight.Reliable().Size(), 1u);   // only the first reliable record fit
    EXPECT_EQ(tight.Unreliable().Size(), 1u); // unreliable unaffected (independent)
}

// ============================================================================
// Step 9 — Replication packet assembly (additive wire ids)
// ============================================================================

namespace
{
    replication::ReplicationConfiguration PacketCfg(std::uint32_t byteBudget, std::uint32_t maxRecords)
    {
        replication::ReplicationConfiguration c{};
        c.bandwidthBudgetBytesPerTick = byteBudget;
        c.maxEntitiesPerUpdate = maxRecords;
        return c;
    }
    void FillQueue(replication::FixedRecordQueue& q, std::uint32_t count)
    {
        for (std::uint32_t i = 1; i <= count; ++i)
        {
            (void)q.Enqueue(QR(replication::ReplicationChangeKind::EntitySpawn, i,
                               replication::ReplicationReliability::Reliable));
        }
    }
    constexpr std::size_t kHdr = replication::ReplicationPacketBuilder::kHeaderBytes;
    constexpr std::size_t kRec = replication::ReplicationPacketBuilder::kRecordBytes;
    constexpr std::size_t kChk = replication::ReplicationPacketBuilder::kChecksumBytes;
} // namespace

// --- Deterministic layout + size + additive DATA-range id ---------------------
TEST(PacketStep9, DeterministicLayoutAndId)
{
    replication::ReplicationPacketBuilder builder{PacketCfg(/*budget*/ 0, /*maxRecords*/ 64)};

    replication::FixedRecordQueue qa{8};
    replication::FixedRecordQueue qb{8};
    FillQueue(qa, 3);
    FillQueue(qb, 3);

    const auto pa = builder.BuildPacket(qa, replication::ReplicationReliability::Reliable,
                                        replication::ClientId{5}, /*tick*/ 100);
    const auto pb = builder.BuildPacket(qb, replication::ReplicationReliability::Reliable,
                                        replication::ClientId{5}, 100);
    ASSERT_TRUE(pa.HasValue());
    ASSERT_TRUE(pb.HasValue());

    EXPECT_EQ(pa.Value().recordCount, 3u);
    EXPECT_EQ(pa.Value().PacketSize(), kHdr + 3u * kRec + kChk);
    EXPECT_FALSE(pa.Value().Empty());
    // Additive DATA-range id (ADR-010); distinct from Sprint-007 player block.
    EXPECT_EQ(pa.Value().messageId.value, stalkermp::net::kMsgReplicationUpdate.value);
    EXPECT_EQ(pa.Value().messageId.value, 0x0200u);
    EXPECT_TRUE(stalkermp::net::IsDataId(pa.Value().messageId));

    // Identical input -> identical bytes (deterministic, byte-for-byte).
    ASSERT_EQ(pa.Value().bytes.size(), pb.Value().bytes.size());
    EXPECT_TRUE(pa.Value().bytes == pb.Value().bytes);
}

// --- Little-endian header layout ----------------------------------------------
TEST(PacketStep9, LittleEndianHeader)
{
    replication::ReplicationPacketBuilder builder{PacketCfg(0, 64)};
    replication::FixedRecordQueue q{4};
    FillQueue(q, 2);
    const auto p = builder.BuildPacket(q, replication::ReplicationReliability::Reliable,
                                       replication::ClientId{0x11223344}, /*tick*/ 0);
    ASSERT_TRUE(p.HasValue());
    const auto& b = p.Value().bytes;
    ASSERT_GE(b.size(), kHdr);
    // messageId 0x0200 little-endian: low byte 0x00, high byte 0x02.
    EXPECT_EQ(b[0], 0x00u);
    EXPECT_EQ(b[1], 0x02u);
    // clientId low 32 bits little-endian: 0x44,0x33,0x22,0x11.
    EXPECT_EQ(b[2], 0x44u);
    EXPECT_EQ(b[3], 0x33u);
    EXPECT_EQ(b[4], 0x22u);
    EXPECT_EQ(b[5], 0x11u);
    // reliability byte (Reliable = 1) at offset 18 (2+8+8), count (2) LE at 19..20.
    EXPECT_EQ(b[18], 1u);
    EXPECT_EQ(b[19], 2u); // recordCount low byte
    EXPECT_EQ(b[20], 0u); // recordCount high byte
}

// --- Budget enforcement: overflow value outcome + partial drain ---------------
TEST(PacketStep9, BudgetOverflowAndLimit)
{
    // Budget below an empty frame (header+checksum) -> Overflow value outcome.
    {
        replication::ReplicationPacketBuilder builder{PacketCfg(/*budget*/ 10, /*maxRecords*/ 64)};
        replication::FixedRecordQueue q{4};
        FillQueue(q, 2);
        EXPECT_FALSE(builder.BuildPacket(q, replication::ReplicationReliability::Reliable,
                                         replication::ClientId{1}, 0)
                         .HasValue());
        EXPECT_EQ(q.Size(), 2u); // no partial drain on overflow
    }
    // Budget fitting exactly one record; the rest remain in the queue.
    {
        const std::uint32_t budget = static_cast<std::uint32_t>(kHdr + kRec + kChk);
        replication::ReplicationPacketBuilder builder{PacketCfg(budget, /*maxRecords*/ 64)};
        replication::FixedRecordQueue q{4};
        FillQueue(q, 3);
        const auto p = builder.BuildPacket(q, replication::ReplicationReliability::Reliable,
                                           replication::ClientId{1}, 0);
        ASSERT_TRUE(p.HasValue());
        EXPECT_EQ(p.Value().recordCount, 1u);
        EXPECT_EQ(p.Value().PacketSize(), budget);
        EXPECT_EQ(q.Size(), 2u); // remaining records left for the next packet
    }
    // maxRecords cap applies even with unbounded byte budget.
    {
        replication::ReplicationPacketBuilder builder{PacketCfg(/*budget*/ 0, /*maxRecords*/ 2)};
        replication::FixedRecordQueue q{8};
        FillQueue(q, 5);
        const auto p = builder.BuildPacket(q, replication::ReplicationReliability::Reliable,
                                           replication::ClientId{1}, 0);
        ASSERT_TRUE(p.HasValue());
        EXPECT_EQ(p.Value().recordCount, 2u);
        EXPECT_EQ(q.Size(), 3u);
    }
}

// --- Empty queue: a valid empty frame -----------------------------------------
TEST(PacketStep9, EmptyPacket)
{
    replication::ReplicationPacketBuilder builder{PacketCfg(0, 64)};
    replication::FixedRecordQueue q{4}; // empty
    const auto p = builder.BuildPacket(q, replication::ReplicationReliability::Unreliable,
                                       replication::ClientId{2}, 0);
    ASSERT_TRUE(p.HasValue());
    EXPECT_TRUE(p.Value().Empty());
    EXPECT_EQ(p.Value().recordCount, 0u);
    EXPECT_EQ(p.Value().PacketSize(), kHdr + kChk); // header + checksum only
    EXPECT_EQ(p.Value().reliability, replication::ReplicationReliability::Unreliable);
}
