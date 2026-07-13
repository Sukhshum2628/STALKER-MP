#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "stalkermp/world/BubbleConfiguration.h"
#include "stalkermp/world/BubbleTypes.h"
#include "stalkermp/world/BubbleManager.h"
#include "stalkermp/world/LocalPlayerPositionSource.h"
#include "stalkermp/world/ISpatialQueries.h"
#include "stalkermp/world/BubbleManagerService.h"
#include "stalkermp/core/Config.h"

using namespace stalkermp;
using namespace stalkermp::world;

TEST(BubbleTests, FoundationTypes)
{
    // PlayerId checks
    PlayerId p1{1};
    PlayerId p2{2};
    PlayerId p1_dup{1};
    EXPECT_EQ(p1, p1_dup);
    EXPECT_NE(p1, p2);

    // PlayerPosition checks
    PlayerPosition pos;
    pos.id = p1;
    pos.position = Vec3{10.0f, 20.0f, 30.0f};
    pos.lastUpdateTick = 100u;
    EXPECT_EQ(pos.id, p1);
    EXPECT_FLOAT_EQ(pos.position.x, 10.0f);

    // BubbleMembership checks
    EXPECT_STREQ(BubbleMembershipName(BubbleMembership::Outside), "Outside");
    EXPECT_STREQ(BubbleMembershipName(BubbleMembership::Inside), "Inside");
    EXPECT_STREQ(BubbleMembershipName(BubbleMembership::PendingActivation), "PendingActivation");
    EXPECT_STREQ(BubbleMembershipName(BubbleMembership::PendingDeactivation), "PendingDeactivation");

    // EntityActivationTransitionName checks
    EXPECT_STREQ(EntityActivationTransitionName(EntityActivationTransition::Entered), "Entered");
    EXPECT_STREQ(EntityActivationTransitionName(EntityActivationTransition::Exited), "Exited");

    // BubbleState checks
    BubbleState state;
    state.activePlayerCount = 0;
    EXPECT_FALSE(state.IsActive());
    state.activePlayerCount = 1;
    EXPECT_TRUE(state.IsActive());
    EXPECT_EQ(state.onlineEntityCount, 0u);
    EXPECT_EQ(state.updateTick, 0u);
}

TEST(BubbleTests, DefaultConfiguration)
{
    core::ConfigStore config;
    auto parseRes = BubbleConfiguration::FromConfig(config);
    ASSERT_TRUE(parseRes.HasValue());

    const auto& bubble = parseRes.Value();
    EXPECT_DOUBLE_EQ(bubble.simulationRadius, 150.0);
    EXPECT_DOUBLE_EQ(bubble.activationMargin, 10.0);
    EXPECT_DOUBLE_EQ(bubble.deactivationMargin, 30.0);
    EXPECT_DOUBLE_EQ(bubble.maximumRadius, 500.0);
    EXPECT_EQ(bubble.debugFlags, 0u);

    // Radius formulas:
    // ActivationRadius = min(150 + 10, 500) = 160
    // DeactivationRadius = min(150 + 30, 500) = 180
    EXPECT_DOUBLE_EQ(bubble.ActivationRadius(), 160.0);
    EXPECT_DOUBLE_EQ(bubble.DeactivationRadius(), 180.0);
    EXPECT_TRUE(bubble.ActivationRadius() <= bubble.DeactivationRadius());
}

TEST(BubbleTests, ConfigurationParsingValid)
{
    auto configRes = core::config::ParseText(
        "[bubble]\n"
        "simulation_radius = 200.0\n"
        "activation_margin = 15.0\n"
        "deactivation_margin = 40.0\n"
        "maximum_radius = 600.0\n"
        "debug_flags = 5\n",
        "test"
    );
    ASSERT_TRUE(configRes.HasValue());
    const auto& config = configRes.Value();

    auto parseRes = BubbleConfiguration::FromConfig(config);
    ASSERT_TRUE(parseRes.HasValue());

    const auto& bubble = parseRes.Value();
    EXPECT_DOUBLE_EQ(bubble.simulationRadius, 200.0);
    EXPECT_DOUBLE_EQ(bubble.activationMargin, 15.0);
    EXPECT_DOUBLE_EQ(bubble.deactivationMargin, 40.0);
    EXPECT_DOUBLE_EQ(bubble.maximumRadius, 600.0);
    EXPECT_EQ(bubble.debugFlags, 5u);

    EXPECT_DOUBLE_EQ(bubble.ActivationRadius(), 215.0);
    EXPECT_DOUBLE_EQ(bubble.DeactivationRadius(), 240.0);
}

TEST(BubbleTests, ConfigurationClamping)
{
    auto configRes = core::config::ParseText(
        "[bubble]\n"
        "simulation_radius = 200.0\n"
        "activation_margin = 100.0\n"
        "deactivation_margin = 250.0\n"
        "maximum_radius = 250.0\n",
        "test"
    );
    ASSERT_TRUE(configRes.HasValue());
    const auto& config = configRes.Value();

    auto parseRes = BubbleConfiguration::FromConfig(config);
    ASSERT_TRUE(parseRes.HasValue());

    const auto& bubble = parseRes.Value();
    // ActivationRadius = min(200 + 100, 250) = 250
    // DeactivationRadius = min(200 + 250, 250) = 250
    EXPECT_DOUBLE_EQ(bubble.ActivationRadius(), 250.0);
    EXPECT_DOUBLE_EQ(bubble.DeactivationRadius(), 250.0);
}

TEST(BubbleTests, ConfigurationParsingInvalid)
{
    // Reject non-positive simulation_radius
    {
        auto configRes = core::config::ParseText("[bubble]\nsimulation_radius = 0.0\n", "test");
        ASSERT_TRUE(configRes.HasValue());
        EXPECT_FALSE(BubbleConfiguration::FromConfig(configRes.Value()).HasValue());
    }

    // Reject negative activation_margin
    {
        auto configRes = core::config::ParseText("[bubble]\nactivation_margin = -1.0\n", "test");
        ASSERT_TRUE(configRes.HasValue());
        EXPECT_FALSE(BubbleConfiguration::FromConfig(configRes.Value()).HasValue());
    }

    // Reject negative deactivation_margin
    {
        auto configRes = core::config::ParseText("[bubble]\ndeactivation_margin = -1.0\n", "test");
        ASSERT_TRUE(configRes.HasValue());
        EXPECT_FALSE(BubbleConfiguration::FromConfig(configRes.Value()).HasValue());
    }

    // Reject non-positive maximum_radius
    {
        auto configRes = core::config::ParseText("[bubble]\nmaximum_radius = -10.0\n", "test");
        ASSERT_TRUE(configRes.HasValue());
        EXPECT_FALSE(BubbleConfiguration::FromConfig(configRes.Value()).HasValue());
    }

    // Reject negative debug_flags
    {
        auto configRes = core::config::ParseText("[bubble]\ndebug_flags = -1\n", "test");
        ASSERT_TRUE(configRes.HasValue());
        EXPECT_FALSE(BubbleConfiguration::FromConfig(configRes.Value()).HasValue());
    }

    // Reject deactivation_margin < activation_margin
    {
        auto configRes = core::config::ParseText(
            "[bubble]\n"
            "activation_margin = 30.0\n"
            "deactivation_margin = 10.0\n",
            "test"
        );
        ASSERT_TRUE(configRes.HasValue());
        EXPECT_FALSE(BubbleConfiguration::FromConfig(configRes.Value()).HasValue());
    }
}

TEST(BubbleTests, BubbleManagerBasicQueries)
{
    BubbleConfiguration config;
    config.simulationRadius = 200.0;
    config.activationMargin = 20.0;
    config.deactivationMargin = 40.0;
    config.maximumRadius = 500.0;

    BubbleManager manager(config);

    // Verify stored configuration
    EXPECT_DOUBLE_EQ(manager.Configuration().simulationRadius, 200.0);
    EXPECT_DOUBLE_EQ(manager.Configuration().ActivationRadius(), 220.0);

    // Verify initial/empty BubbleState
    EXPECT_EQ(manager.State().activePlayerCount, 0u);
    EXPECT_EQ(manager.State().onlineEntityCount, 0u);
    EXPECT_EQ(manager.State().updateTick, 0u);
    EXPECT_FALSE(manager.State().IsActive());

    // Verify public query interface safe defaults
    EXPECT_EQ(manager.OnlineEntityCount(), 0u);
    EXPECT_EQ(manager.ActivePlayerCount(), 0u);
    EXPECT_FALSE(manager.IsEntityInside(EntityId{1}));
    EXPECT_FALSE(manager.IsPositionInside(Vec3{0.0f, 0.0f, 0.0f}));
    EXPECT_FALSE(manager.NearestBubble(Vec3{0.0f, 0.0f, 0.0f}).has_value());
    EXPECT_DOUBLE_EQ(manager.ActiveBubbleRadius(), 0.0); // inactive
}

TEST(BubbleTests, LocalPlayerPositionSource)
{
    LocalPlayerPositionSource source;

    EXPECT_EQ(source.Count(), 0u);
    EXPECT_TRUE(source.ActivePlayers().empty());

    // Insert out of order to verify sorted deterministic ActivePlayers() output
    EXPECT_TRUE(source.SetPlayer(PlayerId{5}, Vec3{50.0f, 50.0f, 50.0f}, 10u).HasValue());
    EXPECT_TRUE(source.SetPlayer(PlayerId{2}, Vec3{20.0f, 20.0f, 20.0f}, 10u).HasValue());
    EXPECT_TRUE(source.SetPlayer(PlayerId{8}, Vec3{80.0f, 80.0f, 80.0f}, 10u).HasValue());

    EXPECT_EQ(source.Count(), 3u);

    auto players = source.ActivePlayers();
    ASSERT_EQ(players.size(), 3u);

    // Verify sorted PlayerId order (B9)
    EXPECT_EQ(players[0].id.value, 2u);
    EXPECT_EQ(players[1].id.value, 5u);
    EXPECT_EQ(players[2].id.value, 8u);

    // Verify values
    EXPECT_FLOAT_EQ(players[0].position.x, 20.0f);
    EXPECT_EQ(players[0].lastUpdateTick, 10u);

    // Update existing player
    EXPECT_TRUE(source.SetPlayer(PlayerId{5}, Vec3{55.0f, 55.0f, 55.0f}, 11u).HasValue());
    EXPECT_EQ(source.Count(), 3u);

    players = source.ActivePlayers();
    EXPECT_EQ(players[1].id.value, 5u);
    EXPECT_FLOAT_EQ(players[1].position.x, 55.0f);
    EXPECT_EQ(players[1].lastUpdateTick, 11u);

    // Null PlayerId must be rejected
    EXPECT_FALSE(source.SetPlayer(PlayerId{0}, Vec3{0.0f, 0.0f, 0.0f}, 10u).HasValue());

    // Remove player
    EXPECT_TRUE(source.RemovePlayer(PlayerId{5}).HasValue());
    EXPECT_EQ(source.Count(), 2u);

    players = source.ActivePlayers();
    EXPECT_EQ(players[0].id.value, 2u);
    EXPECT_EQ(players[1].id.value, 8u);

    // Remove unknown player returns NotFound
    EXPECT_FALSE(source.RemovePlayer(PlayerId{10}).HasValue());
    EXPECT_EQ(source.RemovePlayer(PlayerId{10}).GetError().Code(), core::ErrorCode::NotFound);

    // Clear
    source.Clear();
    EXPECT_EQ(source.Count(), 0u);
    EXPECT_TRUE(source.ActivePlayers().empty());
}

class FakeSpatialQueries final : public ISpatialQueries
{
public:
    std::vector<EntityView> QueryRadius(const Vec3& /*center*/, float /*radius*/) const override { return {}; }
    std::vector<EntityView> QueryBox(const Vec3& /*minimum*/, const Vec3& /*maximum*/) const override { return {}; }
    std::optional<Vec3> PositionOf(EntityId /*id*/) const override { return std::nullopt; }
    std::optional<EntityView> NearestEntity(const Vec3& /*center*/) const override { return std::nullopt; }
};

TEST(BubbleTests, SpatialQueriesIntegration)
{
    BubbleConfiguration config;
    FakeSpatialQueries spatial;
    
    // Verifies constructor accepts spatial queries seam
    BubbleManager manager(config, &spatial);

    EXPECT_EQ(manager.OnlineEntityCount(), 0u);
}

class TestSpatialQueries final : public ISpatialQueries
{
public:
    struct Entry
    {
        EntityId id;
        Vec3 position;
    };
    std::vector<Entry> entities;

    std::vector<EntityView> QueryRadius(const Vec3& center, float radius) const override
    {
        std::vector<EntityView> result;
        const float r2 = radius * radius;
        for (const auto& entry : entities)
        {
            const float dx = entry.position.x - center.x;
            const float dy = entry.position.y - center.y;
            const float dz = entry.position.z - center.z;
            const float dist2 = dx*dx + dy*dy + dz*dz;
            if (dist2 <= r2)
            {
                EntityView view;
                view.id = entry.id;
                view.position = entry.position;
                result.push_back(view);
            }
        }
        return result;
    }

    std::vector<EntityView> QueryBox(const Vec3&, const Vec3&) const override { return {}; }
    std::optional<Vec3> PositionOf(EntityId id) const override
    {
        for (const auto& entry : entities)
        {
            if (entry.id == id) return entry.position;
        }
        return std::nullopt;
    }
    std::optional<EntityView> NearestEntity(const Vec3&) const override { return std::nullopt; }
};

TEST(BubbleTests, ActivationAndHysteresis)
{
    BubbleConfiguration config;
    config.simulationRadius = 100.0;
    config.activationMargin = 10.0; // R_on = 110
    config.deactivationMargin = 30.0; // R_off = 130
    config.maximumRadius = 500.0;

    TestSpatialQueries spatial;
    LocalPlayerPositionSource playerSource;

    // Entity 1: at x=105 (inside R_on = 110)
    // Entity 2: at x=120 (inside R_off = 130, but outside R_on = 110)
    // Entity 3: at x=140 (outside R_off = 130)
    spatial.entities = {
        { EntityId{1}, Vec3{105.0f, 0.0f, 0.0f} },
        { EntityId{2}, Vec3{120.0f, 0.0f, 0.0f} },
        { EntityId{3}, Vec3{140.0f, 0.0f, 0.0f} }
    };

    BubbleManager manager(config, &spatial, &playerSource);

    // Scenario 1: No players => online set empty
    manager.Update(1u);
    EXPECT_EQ(manager.OnlineEntityCount(), 0u);
    EXPECT_EQ(manager.State().activePlayerCount, 0u);
    EXPECT_EQ(manager.State().updateTick, 1u);
    EXPECT_EQ(manager.MembershipOf(EntityId{1}), BubbleMembership::Outside);

    // Scenario 2: Add one player at x=0
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    manager.Update(2u);

    // Entity 1 (105) is inside R_on (110) => activates
    // Entity 2 (120) is outside R_on (110) => stays offline
    // Entity 3 (140) is outside R_off (130) => stays offline
    EXPECT_EQ(manager.OnlineEntityCount(), 1u);
    EXPECT_TRUE(manager.IsEntityInside(EntityId{1}));
    EXPECT_FALSE(manager.IsEntityInside(EntityId{2}));
    EXPECT_FALSE(manager.IsEntityInside(EntityId{3}));

    EXPECT_EQ(manager.MembershipOf(EntityId{1}), BubbleMembership::PendingActivation);
    EXPECT_EQ(manager.MembershipOf(EntityId{2}), BubbleMembership::Outside);

    // Tick 3: Move player to x=10 (Entity 2 is now at distance 110, which is exactly R_on)
    // Entity 2 should activate now because its distance 110 <= R_on (110).
    // Entity 1 (distance 95) remains active.
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{10.0f, 0.0f, 0.0f}, 2u).HasValue());
    manager.Update(3u);

    EXPECT_EQ(manager.OnlineEntityCount(), 2u);
    EXPECT_TRUE(manager.IsEntityInside(EntityId{1}));
    EXPECT_TRUE(manager.IsEntityInside(EntityId{2}));
    EXPECT_EQ(manager.MembershipOf(EntityId{1}), BubbleMembership::Inside);
    EXPECT_EQ(manager.MembershipOf(EntityId{2}), BubbleMembership::PendingActivation);

    // Tick 4: Move player back to x=0.
    // Entity 2 is now at distance 120 (within hysteresis band [110, 130]).
    // Since it was previously online (active at tick 3), it must remain online!
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 3u).HasValue());
    manager.Update(4u);

    EXPECT_EQ(manager.OnlineEntityCount(), 2u);
    EXPECT_TRUE(manager.IsEntityInside(EntityId{1}));
    EXPECT_TRUE(manager.IsEntityInside(EntityId{2}));
    EXPECT_EQ(manager.MembershipOf(EntityId{2}), BubbleMembership::Inside);

    // Tick 5: Move player to x=-15.
    // Entity 2 is now at distance 135 (outside R_off = 130) => deactivates.
    // Entity 1 is at distance 120 (within hysteresis band). Since it was online, it stays online.
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{-15.0f, 0.0f, 0.0f}, 4u).HasValue());
    manager.Update(5u);

    EXPECT_EQ(manager.OnlineEntityCount(), 1u);
    EXPECT_TRUE(manager.IsEntityInside(EntityId{1}));
    EXPECT_FALSE(manager.IsEntityInside(EntityId{2}));
    EXPECT_EQ(manager.MembershipOf(EntityId{2}), BubbleMembership::PendingDeactivation);
    EXPECT_EQ(manager.MembershipOf(EntityId{1}), BubbleMembership::Inside);
}

TEST(BubbleTests, Transitions)
{
    BubbleConfiguration config;
    config.simulationRadius = 100.0;
    config.activationMargin = 0.0;
    config.deactivationMargin = 0.0; // no hysteresis dead-band for simpler testing

    TestSpatialQueries spatial;
    LocalPlayerPositionSource playerSource;

    spatial.entities = {
        { EntityId{1}, Vec3{50.0f, 0.0f, 0.0f} },
        { EntityId{2}, Vec3{150.0f, 0.0f, 0.0f} },
        { EntityId{3}, Vec3{250.0f, 0.0f, 0.0f} }
    };

    BubbleManager manager(config, &spatial, &playerSource);

    // Tick 1: No players -> no transitions
    manager.Update(1u);
    EXPECT_TRUE(manager.Activations().empty());
    EXPECT_TRUE(manager.Deactivations().empty());

    // Tick 2: Player at x=0 -> Entity 1 (dist 50 <= 100) activations={1}, deactivations={}
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    manager.Update(2u);
    ASSERT_EQ(manager.Activations().size(), 1u);
    EXPECT_EQ(manager.Activations()[0].value, 1u);
    EXPECT_TRUE(manager.Deactivations().empty());

    // Tick 3: Move player to x=100 -> Entity 1 (dist 50 <= 100) and Entity 2 (dist 50 <= 100)
    // activations={2}, deactivations={}
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{100.0f, 0.0f, 0.0f}, 2u).HasValue());
    manager.Update(3u);
    ASSERT_EQ(manager.Activations().size(), 1u);
    EXPECT_EQ(manager.Activations()[0].value, 2u);
    EXPECT_TRUE(manager.Deactivations().empty());

    // Tick 4: Move player to x=200 -> Entity 2 (dist 50) and Entity 3 (dist 50) online. Entity 1 (dist 150) offline.
    // activations={3}, deactivations={1}
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{200.0f, 0.0f, 0.0f}, 3u).HasValue());
    manager.Update(4u);
    ASSERT_EQ(manager.Activations().size(), 1u);
    EXPECT_EQ(manager.Activations()[0].value, 3u);
    ASSERT_EQ(manager.Deactivations().size(), 1u);
    EXPECT_EQ(manager.Deactivations()[0].value, 1u);

    // Tick 5: Move player to x=200 (no movement) -> no transitions
    manager.Update(5u);
    EXPECT_TRUE(manager.Activations().empty());
    EXPECT_TRUE(manager.Deactivations().empty());

    // Tick 6: Add overlapping player at x=210 -> no new activations or duplicates
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{2}, Vec3{210.0f, 0.0f, 0.0f}, 4u).HasValue());
    manager.Update(6u);
    EXPECT_TRUE(manager.Activations().empty());
    EXPECT_TRUE(manager.Deactivations().empty());
}

TEST(BubbleTests, ServiceWrapper)
{
    BubbleConfiguration config;
    config.debugFlags = 0; // do not trigger logging on Initialize() to avoid uninitialized logger crash

    FakeSpatialQueries spatial;
    LocalPlayerPositionSource playerSource;

    BubbleManagerService service(config, &spatial, &playerSource);

    EXPECT_EQ(service.Name(), "BubbleManager");
    ASSERT_EQ(service.Dependencies().size(), 2u);
    EXPECT_EQ(service.Dependencies()[0], "World");
    EXPECT_EQ(service.Dependencies()[1], "EntityRegistry");

    // Initialize/Shutdown cycle
    EXPECT_TRUE(service.Initialize().HasValue());
    EXPECT_DOUBLE_EQ(service.Configuration().simulationRadius, 150.0);
    EXPECT_EQ(service.Manager().OnlineEntityCount(), 0u);
    service.Shutdown();
}

TEST(BubbleTests, Diagnostics)
{
    BubbleConfiguration config;
    config.simulationRadius = 100.0;
    config.activationMargin = 10.0; // R_on = 110
    config.deactivationMargin = 30.0; // R_off = 130

    TestSpatialQueries spatial;
    LocalPlayerPositionSource playerSource;

    spatial.entities = {
        { EntityId{1}, Vec3{105.0f, 0.0f, 0.0f} },
        { EntityId{2}, Vec3{120.0f, 0.0f, 0.0f} }
    };

    BubbleManager manager(config, &spatial, &playerSource);

    // Initial state consistency scan
    {
        auto report = manager.ValidateConsistency();
        EXPECT_TRUE(report.IsHealthy());
        EXPECT_TRUE(report.onlineSorted);
        EXPECT_TRUE(report.previousSorted);
        EXPECT_TRUE(report.radiiValid);
        EXPECT_TRUE(report.transitionsConsistent);
    }

    // Tick 1: Player at 0 -> Entity 1 activates
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    manager.Update(1u);

    // Check diagnostics
    EXPECT_EQ(manager.ActivePlayers().size(), 1u);
    EXPECT_EQ(manager.ActivePlayers()[0].id.value, 1u);

    auto online = manager.OnlineEntities();
    ASSERT_EQ(online.size(), 1u);
    EXPECT_EQ(online[0].value, 1u);

    std::string desc = manager.DescribeState();
    EXPECT_TRUE(desc.find("tick=1") != std::string::npos);
    EXPECT_TRUE(desc.find("players=1") != std::string::npos);
    EXPECT_TRUE(desc.find("online=1") != std::string::npos);

    std::string onlineDump = manager.DumpOnline();
    EXPECT_EQ(onlineDump, "Online entities (1): 1");

    std::string transitionDump = manager.DumpTransitions();
    EXPECT_EQ(transitionDump, "Activations (1): 1 | Deactivations (0):");

    // Consistency check on active state
    {
        auto report = manager.ValidateConsistency();
        EXPECT_TRUE(report.IsHealthy());
    }
}

TEST(BubbleTests, ServiceTickDrivesUpdate)
{
    BubbleConfiguration config;
    config.simulationRadius = 100.0;
    config.activationMargin = 0.0;
    config.deactivationMargin = 0.0;
    config.debugFlags = 0; // avoid touching the module logger on Initialize()

    TestSpatialQueries spatial;
    spatial.entities = {
        { EntityId{1}, Vec3{50.0f, 0.0f, 0.0f} },
        { EntityId{2}, Vec3{300.0f, 0.0f, 0.0f} }
    };
    LocalPlayerPositionSource playerSource;

    BubbleManagerService service(config, &spatial, &playerSource);
    ASSERT_TRUE(service.Initialize().HasValue());

    // No players yet: a frame tick leaves the online set empty but advances the
    // monotonic tick counter (Step 8 Tick -> Update wiring).
    service.Tick(0.016);
    EXPECT_EQ(service.Manager().OnlineEntityCount(), 0u);
    const auto tickAfterFirst = service.Manager().State().updateTick;
    EXPECT_GT(tickAfterFirst, 0u);

    // Add a player near entity 1; the next tick brings it online and advances the tick.
    ASSERT_TRUE(playerSource.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    service.Tick(0.016);
    EXPECT_EQ(service.Manager().OnlineEntityCount(), 1u);
    EXPECT_TRUE(service.Manager().IsEntityInside(EntityId{1}));
    EXPECT_GT(service.Manager().State().updateTick, tickAfterFirst); // monotonic

    service.Shutdown();
}

TEST(BubbleTests, DeterministicEvaluation)
{
    BubbleConfiguration config;
    config.simulationRadius = 100.0;
    config.activationMargin = 10.0;
    config.deactivationMargin = 30.0;

    TestSpatialQueries spatial;
    spatial.entities = {
        { EntityId{7}, Vec3{40.0f, 0.0f, 0.0f} },
        { EntityId{3}, Vec3{60.0f, 0.0f, 0.0f} },
        { EntityId{9}, Vec3{200.0f, 0.0f, 0.0f} },
        { EntityId{1}, Vec3{115.0f, 0.0f, 0.0f} }
    };

    // Two independent runs of an identical scenario must produce identical output.
    LocalPlayerPositionSource srcA;
    ASSERT_TRUE(srcA.SetPlayer(PlayerId{2}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    ASSERT_TRUE(srcA.SetPlayer(PlayerId{1}, Vec3{50.0f, 0.0f, 0.0f}, 1u).HasValue());
    BubbleManager a(config, &spatial, &srcA);
    a.Update(1u);

    LocalPlayerPositionSource srcB;
    ASSERT_TRUE(srcB.SetPlayer(PlayerId{2}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    ASSERT_TRUE(srcB.SetPlayer(PlayerId{1}, Vec3{50.0f, 0.0f, 0.0f}, 1u).HasValue());
    BubbleManager b(config, &spatial, &srcB);
    b.Update(1u);

    EXPECT_EQ(a.OnlineEntityCount(), b.OnlineEntityCount());
    EXPECT_EQ(a.DumpOnline(), b.DumpOnline());           // identical ascending order (B9)
    EXPECT_EQ(a.DumpTransitions(), b.DumpTransitions());
    EXPECT_EQ(a.DescribeState(), b.DescribeState());
}

TEST(BubbleTests, MultiPlayerUnionAndDedup)
{
    BubbleConfiguration config;
    config.simulationRadius = 100.0;
    config.activationMargin = 0.0;
    config.deactivationMargin = 0.0;

    TestSpatialQueries spatial;
    spatial.entities = {
        { EntityId{1}, Vec3{0.0f, 0.0f, 0.0f} },   // near player 1
        { EntityId{2}, Vec3{500.0f, 0.0f, 0.0f} }, // near player 2
        { EntityId{3}, Vec3{250.0f, 0.0f, 0.0f} }  // between; covered by neither at first
    };
    LocalPlayerPositionSource src;
    ASSERT_TRUE(src.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());
    ASSERT_TRUE(src.SetPlayer(PlayerId{2}, Vec3{500.0f, 0.0f, 0.0f}, 1u).HasValue());

    BubbleManager m(config, &spatial, &src);
    m.Update(1u);

    // Separated players -> disjoint union {1,2}; entity 3 uncovered.
    EXPECT_EQ(m.OnlineEntityCount(), 2u);
    EXPECT_TRUE(m.IsEntityInside(EntityId{1}));
    EXPECT_TRUE(m.IsEntityInside(EntityId{2}));
    EXPECT_FALSE(m.IsEntityInside(EntityId{3}));
    EXPECT_EQ(m.DumpOnline(), "Online entities (2): 1 2"); // ascending union

    // Overlap dedup: both players now cover entity 3 (at 250) within radius 100.
    ASSERT_TRUE(src.SetPlayer(PlayerId{1}, Vec3{200.0f, 0.0f, 0.0f}, 2u).HasValue());
    ASSERT_TRUE(src.SetPlayer(PlayerId{2}, Vec3{300.0f, 0.0f, 0.0f}, 2u).HasValue());
    m.Update(2u);

    // Entities 1 and 2 are now uncovered; entity 3 is inside BOTH spheres but
    // counted exactly once.
    EXPECT_EQ(m.OnlineEntityCount(), 1u);
    EXPECT_TRUE(m.IsEntityInside(EntityId{3}));
    EXPECT_TRUE(m.ValidateConsistency().IsHealthy()); // sorted+unique => no duplicate
}

TEST(BubbleTests, EmptyWorldNoEntities)
{
    BubbleConfiguration config;
    TestSpatialQueries spatial; // no entities
    LocalPlayerPositionSource src;
    ASSERT_TRUE(src.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());

    BubbleManager m(config, &spatial, &src);
    m.Update(1u);

    EXPECT_EQ(m.OnlineEntityCount(), 0u);
    EXPECT_EQ(m.State().activePlayerCount, 1u); // a player exists; the world has no entities
    EXPECT_TRUE(m.Activations().empty());
    EXPECT_TRUE(m.Deactivations().empty());
    EXPECT_TRUE(m.ValidateConsistency().IsHealthy());
}

TEST(BubbleTests, StressManyEntitiesDeterministic)
{
    BubbleConfiguration config;
    config.simulationRadius = 1000.0;
    config.activationMargin = 0.0;
    config.deactivationMargin = 0.0;
    config.maximumRadius = 100000.0;

    TestSpatialQueries spatial;
    constexpr int kCount = 500;
    for (int i = 1; i <= kCount; ++i)
    {
        spatial.entities.push_back(
            { EntityId{static_cast<std::uint32_t>(i)}, Vec3{static_cast<float>(i), 0.0f, 0.0f} });
    }

    LocalPlayerPositionSource src;
    // Player at x=0, radius 1000 covers entities 1..500 (max distance 500).
    ASSERT_TRUE(src.SetPlayer(PlayerId{1}, Vec3{0.0f, 0.0f, 0.0f}, 1u).HasValue());

    BubbleManager m(config, &spatial, &src);
    m.Update(1u);

    EXPECT_EQ(m.OnlineEntityCount(), static_cast<std::size_t>(kCount));
    EXPECT_EQ(m.Activations().size(), static_cast<std::size_t>(kCount));
    EXPECT_TRUE(m.ValidateConsistency().IsHealthy());

    // Online ids are ascending 1..kCount (deterministic, B9).
    const auto online = m.OnlineEntities();
    ASSERT_EQ(online.size(), static_cast<std::size_t>(kCount));
    for (int i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(online[static_cast<std::size_t>(i)].value, static_cast<std::uint32_t>(i + 1));
    }

    // Move the player far away -> every entity deactivates in one update.
    ASSERT_TRUE(src.SetPlayer(PlayerId{1}, Vec3{100000.0f, 0.0f, 0.0f}, 2u).HasValue());
    m.Update(2u);
    EXPECT_EQ(m.OnlineEntityCount(), 0u);
    EXPECT_EQ(m.Deactivations().size(), static_cast<std::size_t>(kCount));
    EXPECT_TRUE(m.ValidateConsistency().IsHealthy());
}







