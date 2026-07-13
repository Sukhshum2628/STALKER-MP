#include <gtest/gtest.h>

#include <string>

#include "stalkermp/core/Config.h"
#include "stalkermp/world/SimulationClock.h"
#include "stalkermp/world/WorldConfiguration.h"
#include "stalkermp/world/WorldManager.h"
#include "stalkermp/world/WorldModule.h"
#include "stalkermp/world/WorldTime.h"

using namespace stalkermp;
using namespace stalkermp::world;

namespace
{
    WorldManager MakeStartedManager(WorldConfiguration configuration = {})
    {
        WorldManager manager(configuration);
        EXPECT_TRUE(manager.Initialize().HasValue());
        EXPECT_TRUE(manager.Start().HasValue());
        return manager;
    }
} // namespace

// ------------------------------------------------------------------ Clock

TEST(SimulationClock, DeterministicAdvancement)
{
    SimulationClock a(1.0);
    SimulationClock b(1.0);
    const double deltas[] = {0.016, 0.033, 0.0, 0.1};

    for (const double dt : deltas)
    {
        a.Advance(dt);
        b.Advance(dt);
    }

    EXPECT_EQ(a.SimulationSeconds(), b.SimulationSeconds());
    EXPECT_EQ(a.Tick(), static_cast<std::uint64_t>(4));
    EXPECT_EQ(a.SimulationSeconds(), 0.016 + 0.033 + 0.0 + 0.1);
}

TEST(SimulationClock, SpeedScalesTime)
{
    SimulationClock clock(2.0);
    clock.Advance(1.0);
    EXPECT_EQ(clock.SimulationSeconds(), 2.0);
    EXPECT_EQ(clock.Speed(), 2.0);
}

TEST(SimulationClock, PausedTickAdvancesTickOnly)
{
    SimulationClock clock(1.0);
    clock.Advance(1.0);
    clock.AdvancePausedTick();
    EXPECT_EQ(clock.Tick(), static_cast<std::uint64_t>(2));
    EXPECT_EQ(clock.SimulationSeconds(), 1.0);
}

// -------------------------------------------------------------- WorldTime

TEST(WorldTime, DerivesTimeOfDay)
{
    const double dayLength = 100.0; // 100 sim-seconds per day.

    EXPECT_EQ(DeriveTimeOfDay(0.0, dayLength).dayFraction, 0.0);
    EXPECT_EQ(DeriveTimeOfDay(50.0, dayLength).dayFraction, 0.5);
    EXPECT_EQ(DeriveTimeOfDay(150.0, dayLength).dayFraction, 0.5); // Wraps.

    const WorldTimeOfDay noon = DeriveTimeOfDay(50.0, dayLength);
    EXPECT_EQ(noon.Hour(), 12u);
    EXPECT_EQ(noon.Minute(), 0u);

    const WorldTimeOfDay quarter = DeriveTimeOfDay(25.0, dayLength);
    EXPECT_EQ(quarter.Hour(), 6u);
}

// ---------------------------------------------------------- Configuration

TEST(WorldConfiguration, DefaultsWhenSectionMissing)
{
    const auto empty = core::config::ParseText("", "test");
    ASSERT_TRUE(empty.HasValue());

    const auto result = WorldConfiguration::FromConfig(empty.Value());
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.Value().simulationSpeed, 1.0);
    EXPECT_EQ(result.Value().dayLengthMinutes, 90.0);
    EXPECT_FALSE(result.Value().debugLogging);
}

TEST(WorldConfiguration, ParsesValues)
{
    const auto parsed = core::config::ParseText(
        "[world]\n"
        "simulation_speed = 2.5\n"
        "day_length_minutes = 120\n"
        "debug_logging = true\n",
        "test");
    ASSERT_TRUE(parsed.HasValue());

    const auto result = WorldConfiguration::FromConfig(parsed.Value());
    ASSERT_TRUE(result.HasValue());
    EXPECT_EQ(result.Value().simulationSpeed, 2.5);
    EXPECT_EQ(result.Value().dayLengthMinutes, 120.0);
    EXPECT_TRUE(result.Value().debugLogging);
    EXPECT_EQ(result.Value().DayLengthSeconds(), 7200.0);
}

TEST(WorldConfiguration, RejectsInvalidValues)
{
    for (const char* text : {"[world]\nsimulation_speed = 0\n",
                             "[world]\nsimulation_speed = fast\n",
                             "[world]\nday_length_minutes = -5\n",
                             "[world]\ndebug_logging = maybe\n"})
    {
        const auto parsed = core::config::ParseText(text, "test");
        ASSERT_TRUE(parsed.HasValue());
        const auto result = WorldConfiguration::FromConfig(parsed.Value());
        ASSERT_FALSE(result.HasValue());
        EXPECT_TRUE(result.GetError().Code() == core::ErrorCode::InvalidArgument);
    }
}

// -------------------------------------------------------------- Lifecycle

TEST(WorldManager, LifecycleHappyPath)
{
    WorldManager manager((WorldConfiguration()));
    EXPECT_TRUE(manager.State() == WorldLifecycleState::Created);

    ASSERT_TRUE(manager.Initialize().HasValue());
    EXPECT_TRUE(manager.State() == WorldLifecycleState::Initialized);

    ASSERT_TRUE(manager.Start().HasValue());
    EXPECT_TRUE(manager.State() == WorldLifecycleState::Running);

    ASSERT_TRUE(manager.Pause().HasValue());
    EXPECT_TRUE(manager.State() == WorldLifecycleState::Paused);

    ASSERT_TRUE(manager.Resume().HasValue());
    EXPECT_TRUE(manager.State() == WorldLifecycleState::Running);

    manager.Shutdown();
    EXPECT_TRUE(manager.State() == WorldLifecycleState::ShutDown);
    manager.Shutdown(); // Idempotent.
    EXPECT_TRUE(manager.State() == WorldLifecycleState::ShutDown);
}

TEST(WorldManager, RejectsIllegalTransitions)
{
    WorldManager manager((WorldConfiguration()));

    // Pause before initialization.
    EXPECT_FALSE(manager.Pause().HasValue());
    // Resume before running.
    EXPECT_FALSE(manager.Resume().HasValue());

    ASSERT_TRUE(manager.Initialize().HasValue());
    // Double initialization.
    EXPECT_FALSE(manager.Initialize().HasValue());
    // Resume from Initialized.
    EXPECT_FALSE(manager.Resume().HasValue());
}

// ------------------------------------------------------------------- Tick

TEST(WorldManager, TickPublishesMonotonicContexts)
{
    WorldManager manager = MakeStartedManager();

    EXPECT_EQ(manager.Context().tick, static_cast<std::uint64_t>(0));

    manager.Tick(0.016);
    const WorldContext first = manager.Context();
    EXPECT_EQ(first.tick, static_cast<std::uint64_t>(1));
    EXPECT_EQ(first.deltaSeconds, 0.016);
    EXPECT_EQ(first.simulationSeconds, 0.016);

    manager.Tick(0.033);
    const WorldContext second = manager.Context();
    EXPECT_EQ(second.tick, static_cast<std::uint64_t>(2));
    EXPECT_EQ(second.simulationSeconds, 0.016 + 0.033);

    // The first snapshot is unaffected by later ticks (value semantics).
    EXPECT_EQ(first.tick, static_cast<std::uint64_t>(1));
    EXPECT_EQ(first.simulationSeconds, 0.016);
}

TEST(WorldManager, PauseFreezesSimulationTime)
{
    WorldManager manager = MakeStartedManager();

    manager.Tick(1.0);
    ASSERT_TRUE(manager.Pause().HasValue());

    manager.Tick(1.0); // Paused tick.
    const WorldContext paused = manager.Context();
    EXPECT_EQ(paused.simulationSeconds, 1.0);    // Frozen.
    EXPECT_EQ(paused.tick, static_cast<std::uint64_t>(2)); // Still fresh.

    ASSERT_TRUE(manager.Resume().HasValue());
    manager.Tick(0.5);
    EXPECT_EQ(manager.Context().simulationSeconds, 1.5);
}

TEST(WorldManager, SimulationSpeedAffectsContext)
{
    WorldConfiguration configuration;
    configuration.simulationSpeed = 3.0;
    WorldManager manager = MakeStartedManager(configuration);

    manager.Tick(1.0);
    EXPECT_EQ(manager.Context().simulationSeconds, 3.0);
}

TEST(WorldManager, TimeOfDayDerivedFromConfiguredDayLength)
{
    WorldConfiguration configuration;
    configuration.dayLengthMinutes = 1.0; // 60-second day.
    WorldManager manager = MakeStartedManager(configuration);

    manager.Tick(30.0); // Half a day.
    const WorldContext context = manager.Context();
    EXPECT_EQ(context.timeOfDay.Hour(), 12u);
}

// ----------------------------------------------------------------- Module

TEST(WorldModule, IdentityAndVersion)
{
    WorldModule module;
    EXPECT_EQ(module.Name(), std::string_view("World"));
    EXPECT_TRUE(module.ModuleVersion() == (core::SemanticVersion{0, 1, 0}));
    EXPECT_TRUE(module.Initialize().HasValue());
    module.Shutdown();
}
