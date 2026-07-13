#include "stalkermp/world/WorldQueryService.h"

#include "stalkermp/core/Assert.h"
#include "stalkermp/core/Log.h"
#include "stalkermp/common/StringUtil.h"

namespace stalkermp::world
{
    WorldQueryService::WorldQueryService(const IEntitySource& source)
        : m_source(source)
    {
    }

    core::Expected<void> WorldQueryService::Initialize()
    {
        if (core::IsLogAvailable())
        {
            core::Log().Info("World", common::Format("Query service ready ({} entities visible)",
                                                     m_source.Count()));
        }
        return core::Success();
    }

    void WorldQueryService::Shutdown() noexcept
    {
        if (core::IsLogAvailable())
        {
            core::Log().Info("World",
                             common::Format("Query service shut down ({} entity queries, {} spatial queries)",
                                            m_statistics.entityQueries, m_statistics.spatialQueries));
        }
    }

    // --- IWorldQueries -------------------------------------------------------

    bool WorldQueryService::EntityExists(const EntityId id) const
    {
        ++m_statistics.entityQueries;
        return m_source.FindById(id).has_value();
    }

    std::optional<EntityView> WorldQueryService::GetEntity(const EntityId id) const
    {
        ++m_statistics.entityQueries;
        return m_source.FindById(id);
    }

    std::optional<EntityView> WorldQueryService::FindByID(const EntityId id) const
    {
        return GetEntity(id);
    }

    std::optional<EntityView> WorldQueryService::FindEntity(const std::string_view name) const
    {
        ++m_statistics.entityQueries;
        return m_source.FindByName(name);
    }

    std::vector<EntityView>
    WorldQueryService::GetEntities(const std::function<bool(const EntityView&)>& predicate) const
    {
        ++m_statistics.entityQueries;
        std::vector<EntityView> result;
        m_source.Enumerate([&](const EntityView& entity) {
            if (!predicate || predicate(entity))
            {
                result.push_back(entity);
            }
        });
        return result;
    }

    // --- ISpatialQueries -----------------------------------------------------

    std::vector<EntityView> WorldQueryService::QueryRadius(const Vec3& center, const float radius) const
    {
        SMP_ASSERT_MSG(radius >= 0.0f, "QueryRadius: negative radius");
        ++m_statistics.spatialQueries;

        std::vector<EntityView> result;
        const float radiusSquared = radius * radius;
        m_source.Enumerate([&](const EntityView& entity) {
            if (DistanceSquared(entity.position, center) <= radiusSquared)
            {
                result.push_back(entity);
            }
        });
        return result;
    }

    std::vector<EntityView> WorldQueryService::QueryBox(const Vec3& minimum, const Vec3& maximum) const
    {
        ++m_statistics.spatialQueries;

        std::vector<EntityView> result;
        m_source.Enumerate([&](const EntityView& entity) {
            const Vec3& p = entity.position;
            if (p.x >= minimum.x && p.x <= maximum.x &&
                p.y >= minimum.y && p.y <= maximum.y &&
                p.z >= minimum.z && p.z <= maximum.z)
            {
                result.push_back(entity);
            }
        });
        return result;
    }

    std::optional<Vec3> WorldQueryService::PositionOf(const EntityId id) const
    {
        ++m_statistics.spatialQueries;
        const auto entity = m_source.FindById(id);
        return entity ? std::optional<Vec3>(entity->position) : std::nullopt;
    }

    std::optional<EntityView> WorldQueryService::NearestEntity(const Vec3& center) const
    {
        ++m_statistics.spatialQueries;

        std::optional<EntityView> nearest;
        float nearestDistance = 0.0f;
        m_source.Enumerate([&](const EntityView& entity) {
            const float distance = DistanceSquared(entity.position, center);
            const bool closer = !nearest || distance < nearestDistance ||
                                (distance == nearestDistance && entity.id.value < nearest->id.value);
            if (closer)
            {
                nearest = entity;
                nearestDistance = distance;
            }
        });
        return nearest;
    }
} // namespace stalkermp::world
