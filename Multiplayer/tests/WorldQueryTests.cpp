#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "stalkermp/world/EnvironmentService.h"
#include "stalkermp/world/WorldManager.h"
#include "stalkermp/world/WorldQueryService.h"

using namespace stalkermp::world;

namespace
{
    class FakeEntitySource final : public IEntitySource
    {
    public:
        std::vector<EntityView> entities;

        [[nodiscard]] std::size_t Count() const override { return entities.size(); }

        void Enumerate(const std::function<void(const EntityView&)>& visitor) const override
        {
            for (const EntityView& entity : entities)
            {
                visitor(entity);
            }
        }

        [[nodiscard]] std::optional<EntityView> FindById(const EntityId id) const override
        {
            for (const EntityView& entity : entities)
            {
                if (entity.id == id)
                {
                    return entity;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::optional<EntityView> FindByName(const std::string_view name) const override
        {
            for (const EntityView& entity : entities)
            {
                if (entity.name == name)
                {
                    return entity;
                }
            }
            return std::nullopt;
        }
    };

    class FakeEnvironmentSource final : public IEnvironmentSource
    {
    public:
        std::optional<EnvironmentSample> sample;

        [[nodiscard]] std::optional<EnvironmentSample> Sample() const override { return sample; }
    };

    FakeEntitySource MakeThreeEntities()
    {
        FakeEntitySource source;
        source.entities = {
            {EntityId{1}, "stalker_a", Vec3{0.0f, 0.0f, 0.0f}},
            {EntityId{2}, "stalker_b", Vec3{10.0f, 0.0f, 0.0f}},
            {EntityId{3}, "mutant_c", Vec3{0.0f, 5.0f, 0.0f}},
        };
        return source;
    }
} // namespace

// ---------------------------------------------------------- Entity queries

TEST(WorldQueries, LookupByIdAndName)
{
    const FakeEntitySource source = MakeThreeEntities();
    const WorldQueryService queries(source);

    EXPECT_TRUE(queries.EntityExists(EntityId{1}));
    EXPECT_FALSE(queries.EntityExists(EntityId{99}));

    const auto byId = queries.GetEntity(EntityId{2});
    ASSERT_TRUE(byId.has_value());
    EXPECT_EQ(byId->name, std::string("stalker_b"));

    EXPECT_TRUE(queries.FindByID(EntityId{2}).has_value()); // Alias.

    const auto byName = queries.FindEntity("mutant_c");
    ASSERT_TRUE(byName.has_value());
    EXPECT_TRUE(byName->id == EntityId{3});

    EXPECT_FALSE(queries.FindEntity("nobody").has_value());
}

TEST(WorldQueries, GetEntitiesWithAndWithoutPredicate)
{
    const FakeEntitySource source = MakeThreeEntities();
    const WorldQueryService queries(source);

    EXPECT_EQ(queries.GetEntities(nullptr).size(), static_cast<std::size_t>(3));

    const auto stalkers = queries.GetEntities(
        [](const EntityView& entity) { return entity.name.find("stalker") == 0; });
    EXPECT_EQ(stalkers.size(), static_cast<std::size_t>(2));
}

TEST(WorldQueries, EmptySourceYieldsNothing)
{
    const FakeEntitySource source; // Empty.
    const WorldQueryService queries(source);

    EXPECT_TRUE(queries.GetEntities(nullptr).empty());
    EXPECT_FALSE(queries.NearestEntity(Vec3{}).has_value());
    EXPECT_TRUE(queries.QueryRadius(Vec3{}, 1000.0f).empty());
}

// --------------------------------------------------------- Spatial queries

TEST(SpatialQueries, RadiusIsInclusiveAtBoundary)
{
    const FakeEntitySource source = MakeThreeEntities();
    const WorldQueryService queries(source);

    // Entity 2 is exactly 10 units away.
    EXPECT_EQ(queries.QueryRadius(Vec3{0.0f, 0.0f, 0.0f}, 10.0f).size(), static_cast<std::size_t>(3));
    EXPECT_EQ(queries.QueryRadius(Vec3{0.0f, 0.0f, 0.0f}, 9.99f).size(), static_cast<std::size_t>(2));
    EXPECT_EQ(queries.QueryRadius(Vec3{0.0f, 0.0f, 0.0f}, 0.0f).size(), static_cast<std::size_t>(1));
}

TEST(SpatialQueries, BoxIsInclusive)
{
    const FakeEntitySource source = MakeThreeEntities();
    const WorldQueryService queries(source);

    const auto inBox = queries.QueryBox(Vec3{-1.0f, -1.0f, -1.0f}, Vec3{10.0f, 1.0f, 1.0f});
    EXPECT_EQ(inBox.size(), static_cast<std::size_t>(2)); // Entities 1 and 2; 3 is at y=5.
}

TEST(SpatialQueries, NearestEntityAndPositionOf)
{
    const FakeEntitySource source = MakeThreeEntities();
    const WorldQueryService queries(source);

    const auto nearest = queries.NearestEntity(Vec3{1.0f, 0.0f, 0.0f});
    ASSERT_TRUE(nearest.has_value());
    EXPECT_TRUE(nearest->id == EntityId{1});

    const auto position = queries.PositionOf(EntityId{3});
    ASSERT_TRUE(position.has_value());
    EXPECT_EQ(position->y, 5.0f);
    EXPECT_FALSE(queries.PositionOf(EntityId{99}).has_value());
}

TEST(SpatialQueries, NearestTieResolvesToLowestId)
{
    FakeEntitySource source;
    source.entities = {
        {EntityId{7}, "b", Vec3{1.0f, 0.0f, 0.0f}},
        {EntityId{4}, "a", Vec3{-1.0f, 0.0f, 0.0f}},
    };
    const WorldQueryService queries(source);

    const auto nearest = queries.NearestEntity(Vec3{});
    ASSERT_TRUE(nearest.has_value());
    EXPECT_TRUE(nearest->id == EntityId{4});
}

// -------------------------------------------------------------- Statistics

TEST(WorldQueries, CountsQueries)
{
    const FakeEntitySource source = MakeThreeEntities();
    const WorldQueryService queries(source);

    (void)queries.EntityExists(EntityId{1});
    (void)queries.FindEntity("stalker_a");
    (void)queries.QueryRadius(Vec3{}, 1.0f);

    const QueryStatistics stats = queries.Statistics();
    EXPECT_EQ(stats.entityQueries, static_cast<std::uint64_t>(2));
    EXPECT_EQ(stats.spatialQueries, static_cast<std::uint64_t>(1));
}

// ------------------------------------------------------------- Environment

TEST(Environment, WorldSamplesSourceIntoContext)
{
    FakeEnvironmentSource environment;
    environment.sample = EnvironmentSample{"clear", false};

    WorldManager manager((WorldConfiguration()), &environment);
    ASSERT_TRUE(manager.Initialize().HasValue());
    ASSERT_TRUE(manager.Start().HasValue());

    manager.Tick(0.016);
    EXPECT_TRUE(manager.Context().weather == WeatherId::FromName("clear"));
    EXPECT_FALSE(manager.Context().emissionActive);

    // Weather changes propagate on the next tick.
    environment.sample = EnvironmentSample{"storm", true};
    manager.Tick(0.016);
    EXPECT_TRUE(manager.Context().weather == WeatherId::FromName("storm"));
    EXPECT_TRUE(manager.Context().emissionActive);

    // Unavailable environment keeps the last known state.
    environment.sample.reset();
    manager.Tick(0.016);
    EXPECT_TRUE(manager.Context().weather == WeatherId::FromName("storm"));
}

TEST(Environment, ServiceExposesContextState)
{
    FakeEnvironmentSource environment;
    environment.sample = EnvironmentSample{"fog", false};

    WorldManager manager((WorldConfiguration()), &environment);
    ASSERT_TRUE(manager.Initialize().HasValue());
    ASSERT_TRUE(manager.Start().HasValue());
    manager.Tick(0.016);

    EnvironmentService service(manager);
    EXPECT_TRUE(service.Weather() == WeatherId::FromName("fog"));
    EXPECT_FALSE(service.EmissionActive());
    EXPECT_EQ(service.TimeOfDay().Hour(), manager.Context().timeOfDay.Hour());
}

// ------------------------------------------------------------- Diagnostics

TEST(WorldManager, TickStatisticsAccumulate)
{
    WorldManager manager((WorldConfiguration()));
    ASSERT_TRUE(manager.Initialize().HasValue());
    ASSERT_TRUE(manager.Start().HasValue());

    manager.Tick(0.016);
    manager.Tick(0.016);

    const WorldTickStatistics stats = manager.Statistics();
    EXPECT_EQ(stats.ticks, static_cast<std::uint64_t>(2));
    EXPECT_TRUE(stats.lastTickMilliseconds >= 0.0);
    EXPECT_TRUE(stats.maxTickMilliseconds >= stats.lastTickMilliseconds);
}
