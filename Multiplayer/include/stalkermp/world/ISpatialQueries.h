// STALKER-MP — Spatial query interface (Sprint-002, §7.6)
//
// Read-only spatial lookups. The initial implementation is a linear scan
// over IEntitySource (per spec: "may initially wrap existing engine
// functionality"); spatial partitioning is an optimization for later,
// measured first (02_Engine.md §20.2).

#pragma once

#include <optional>
#include <vector>

#include "stalkermp/world/EntityView.h"

namespace stalkermp::world
{
    class ISpatialQueries
    {
    public:
        virtual ~ISpatialQueries() = default;

        // Entities within radius of center (sphere semantics; boundary
        // inclusive). Radius must be >= 0.
        [[nodiscard]] virtual std::vector<EntityView> QueryRadius(const Vec3& center, float radius) const = 0;

        // Entities inside the axis-aligned box [min, max] (inclusive).
        [[nodiscard]] virtual std::vector<EntityView> QueryBox(const Vec3& minimum, const Vec3& maximum) const = 0;

        // Position of an entity, if it exists.
        [[nodiscard]] virtual std::optional<Vec3> PositionOf(EntityId id) const = 0;

        // Entity closest to center; ties resolve to the lowest id.
        [[nodiscard]] virtual std::optional<EntityView> NearestEntity(const Vec3& center) const = 0;
    };
} // namespace stalkermp::world
