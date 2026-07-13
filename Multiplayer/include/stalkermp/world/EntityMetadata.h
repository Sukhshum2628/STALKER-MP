// STALKER-MP — Entity metadata value types (Sprint-003, §7.10; §7.6; §7.11)
//
// The registry-owned metadata for a persistent entity. Metadata only: behavior
// lives elsewhere (04_World.md §12; Sprint-003 §7.10). These are pure value
// types — no engine headers (invariant I2 / One Engine Boundary), no engine
// pointers (C2), no exceptions and no throwing STL (ADR-007 / I10). std::string
// is used for the section name exactly as the Sprint-002 EntityView uses it for
// the entity name (verified engine-ABI-clean).
//
// This step introduces types only. No registry logic, lookup containers,
// synchronization, or events.

#pragma once

#include <cstdint>
#include <string>

namespace stalkermp::world
{
    // Category axis used by lookup (§7.4 FindByType) and statistics (§7.11).
    // Named values correspond to the counted statistics; Unknown is the catch-all
    // for persistent entities that are none of the named categories (doors,
    // containers, artifacts, story objects, ...).
    enum class EntityCategory : std::uint8_t
    {
        Unknown = 0,
        Npc,
        Monster,
        Item,
        Vehicle,
        Player,
    };

    [[nodiscard]] constexpr const char* EntityCategoryName(const EntityCategory category) noexcept
    {
        switch (category)
        {
        case EntityCategory::Unknown: return "Unknown";
        case EntityCategory::Npc:     return "Npc";
        case EntityCategory::Monster: return "Monster";
        case EntityCategory::Item:    return "Item";
        case EntityCategory::Vehicle: return "Vehicle";
        case EntityCategory::Player:  return "Player";
        }
        return "Unknown";
    }

    // Registration lifecycle state (§7.6). This is the REGISTRATION state, not the
    // ALife online/offline state (05_ALife.md §12.3, owned by Sprint-004/005).
    // Construct → Register → Initialize → Active → PendingRemoval → Unregister.
    enum class EntityRegistrationState : std::uint8_t
    {
        Constructed = 0,
        Registered,
        Initialized,
        Active,
        PendingRemoval,
        Unregistered,
    };

    [[nodiscard]] constexpr const char* EntityRegistrationStateName(const EntityRegistrationState state) noexcept
    {
        switch (state)
        {
        case EntityRegistrationState::Constructed:    return "Constructed";
        case EntityRegistrationState::Registered:     return "Registered";
        case EntityRegistrationState::Initialized:    return "Initialized";
        case EntityRegistrationState::Active:         return "Active";
        case EntityRegistrationState::PendingRemoval: return "PendingRemoval";
        case EntityRegistrationState::Unregistered:   return "Unregistered";
        }
        return "Unregistered";
    }

    // How the entity came to exist (§7.10 "Spawn Source"). "Dynamic Spawn Count"
    // (§7.11) is derived from spawnSource == Dynamic.
    enum class EntitySpawnSource : std::uint8_t
    {
        Unknown = 0,
        EngineWorld, // present in the loaded world / ALife
        Dynamic,     // spawned at runtime
        Restored,    // registered from a pre-assigned identity (RestoreEntity, §7.3)
    };

    [[nodiscard]] constexpr const char* EntitySpawnSourceName(const EntitySpawnSource source) noexcept
    {
        switch (source)
        {
        case EntitySpawnSource::Unknown:     return "Unknown";
        case EntitySpawnSource::EngineWorld: return "EngineWorld";
        case EntitySpawnSource::Dynamic:     return "Dynamic";
        case EntitySpawnSource::Restored:    return "Restored";
        }
        return "Unknown";
    }

    // Opaque per-entity flag bits (§7.10 "Flags"). Bit semantics are assigned by
    // later work; Sprint-003 defines only the storage and the empty value.
    using EntityFlags = std::uint32_t;
    inline constexpr EntityFlags kNoEntityFlags = 0;

    // Simulation tick index at creation (§7.10 "Creation Tick"), matching the
    // World tick index width (Sprint-002 WorldContext).
    using EntityTick = std::uint64_t;

    // Registry-owned metadata record for one entity (§7.10). Pure value type;
    // holds no identity (identity lives on the record's handle) and no engine
    // pointer.
    struct EntityMetadata
    {
        EntityCategory category = EntityCategory::Unknown;
        std::string section;                          // engine section name (e.g. "stalker")
        EntityFlags flags = kNoEntityFlags;
        EntityTick creationTick = 0;
        EntitySpawnSource spawnSource = EntitySpawnSource::Unknown;
        EntityRegistrationState state = EntityRegistrationState::Constructed;
    };
} // namespace stalkermp::world
