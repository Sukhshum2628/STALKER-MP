// STALKER-MP — Entity view types (Sprint-002, §7.5)
//
// Read-only snapshot of an entity as seen through IEntitySource. This is
// NOT the entity model (Sprint-003 owns identity/registry semantics); it
// is the minimal value type world queries return today.

#pragma once

#include <cstdint>
#include <string>

namespace stalkermp::world
{
    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    [[nodiscard]] inline float DistanceSquared(const Vec3& a, const Vec3& b) noexcept
    {
        const float dx = a.x - b.x;
        const float dy = a.y - b.y;
        const float dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    }

    // Generic entity identifier. Wide enough for engine object ids (u16)
    // and future registry ids; semantics belong to Sprint-003.
    struct EntityId
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool operator==(const EntityId& other) const noexcept { return value == other.value; }
        [[nodiscard]] constexpr bool operator!=(const EntityId& other) const noexcept { return value != other.value; }
    };

    struct EntityView
    {
        EntityId id{};
        std::string name;
        Vec3 position{};
    };
} // namespace stalkermp::world
