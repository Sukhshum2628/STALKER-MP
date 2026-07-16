// STALKER-MP — Replication subsystem tests (Sprint-009)
//
// Step 1: replication value types, enumerations, POD structures, and const char*
//         name helpers. Engine-free and OS-free. Types only — no configuration,
//         update, registry, interest, delta, classifier, queue, packet, worker,
//         manager, or diagnostics.

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "stalkermp/replication/ReplicationTypes.h"

using namespace stalkermp;

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
