// STALKER-MP — Replication reliability & priority classification (Sprint-009, Step 7)
//
// A pure, total, deterministic classifier mapping each ReplicationChangeKind to a
// reliability class, a priority band, and a channel. It encodes VERBATIM the
// complete, frozen classification table in the Sprint-009 Implementation Plan
// (§7.A — "Frozen classification table"), which completes the design tables §7.8
// (priority) and §7.9 (reliability) so every kind is assigned exactly once. This
// classifier invents no defaults: every value below is the authoritative §7.A
// value. Pure constexpr, value-only, stateless — no queues, delta, packets,
// networking, threading, or tick/service. Header-only (a pure constexpr mapping
// belongs in the header).
//
// §7.A summary (authoritative source is the plan):
//   Reliable   : EntitySpawn, EntityRemove, Inventory, QuestUpdate, PlayerJoin,
//                PlayerLeave, Combat, Damage.
//   Unreliable : None, Player, NearbyNpc, Animation, AmbientObject, WeatherUpdate,
//                DistantEntity, Position, Rotation, Velocity, Camera.
//   Priority High   : Player, Combat, Damage, EntitySpawn, EntityRemove,
//                     PlayerJoin, PlayerLeave.
//   Priority Medium : NearbyNpc, Inventory, Animation, Position, Rotation,
//                     Velocity, QuestUpdate.
//   Priority Low    : None, AmbientObject, WeatherUpdate, DistantEntity, Camera.
//   Channel : derived — Reliable -> Reliable, Unreliable -> Unreliable
//             (Control reserved, assigned to no data kind).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/replication/ReplicationTypes.h"

namespace stalkermp::replication
{
    // What kind of change a replication record represents. Fixed std::uint8_t;
    // reserved 0 = None; new kinds are appended, never renumbered.
    enum class ReplicationChangeKind : std::uint8_t
    {
        None = 0,
        // §7.8 priority categories
        Player,        // High
        Combat,        // High
        Damage,        // High
        EntitySpawn,   // High + Reliable
        EntityRemove,  // High (Entity Destruction) + Reliable (Entity Removal)
        NearbyNpc,     // Medium
        Inventory,     // Medium + Reliable
        Animation,     // Medium + Unreliable
        AmbientObject, // Low
        WeatherUpdate, // Low
        DistantEntity, // Low
        // §7.9 reliability categories not named in §7.8
        Position,      // Unreliable
        Rotation,      // Unreliable
        Velocity,      // Unreliable
        Camera,        // Unreliable
        QuestUpdate,   // Reliable
        PlayerJoin,    // Reliable
        PlayerLeave,   // Reliable
    };

    [[nodiscard]] constexpr const char* ReplicationChangeKindName(const ReplicationChangeKind k) noexcept
    {
        switch (k)
        {
        case ReplicationChangeKind::None:          return "None";
        case ReplicationChangeKind::Player:        return "Player";
        case ReplicationChangeKind::Combat:        return "Combat";
        case ReplicationChangeKind::Damage:        return "Damage";
        case ReplicationChangeKind::EntitySpawn:   return "EntitySpawn";
        case ReplicationChangeKind::EntityRemove:  return "EntityRemove";
        case ReplicationChangeKind::NearbyNpc:     return "NearbyNpc";
        case ReplicationChangeKind::Inventory:     return "Inventory";
        case ReplicationChangeKind::Animation:     return "Animation";
        case ReplicationChangeKind::AmbientObject: return "AmbientObject";
        case ReplicationChangeKind::WeatherUpdate: return "WeatherUpdate";
        case ReplicationChangeKind::DistantEntity: return "DistantEntity";
        case ReplicationChangeKind::Position:      return "Position";
        case ReplicationChangeKind::Rotation:      return "Rotation";
        case ReplicationChangeKind::Velocity:      return "Velocity";
        case ReplicationChangeKind::Camera:        return "Camera";
        case ReplicationChangeKind::QuestUpdate:   return "QuestUpdate";
        case ReplicationChangeKind::PlayerJoin:    return "PlayerJoin";
        case ReplicationChangeKind::PlayerLeave:   return "PlayerLeave";
        }
        return "Unknown";
    }

    // Reliability per the frozen §7.A table (authoritative; no invented default).
    [[nodiscard]] constexpr ReplicationReliability ClassifyReliability(const ReplicationChangeKind k) noexcept
    {
        switch (k)
        {
        // §7.A Reliable set.
        case ReplicationChangeKind::EntitySpawn:
        case ReplicationChangeKind::EntityRemove:
        case ReplicationChangeKind::Inventory:
        case ReplicationChangeKind::QuestUpdate:
        case ReplicationChangeKind::PlayerJoin:
        case ReplicationChangeKind::PlayerLeave:
        case ReplicationChangeKind::Combat:
        case ReplicationChangeKind::Damage:
            return ReplicationReliability::Reliable;
        // §7.A Unreliable set.
        case ReplicationChangeKind::Position:
        case ReplicationChangeKind::Rotation:
        case ReplicationChangeKind::Velocity:
        case ReplicationChangeKind::Animation:
        case ReplicationChangeKind::Camera:
        case ReplicationChangeKind::Player:
        case ReplicationChangeKind::NearbyNpc:
        case ReplicationChangeKind::AmbientObject:
        case ReplicationChangeKind::WeatherUpdate:
        case ReplicationChangeKind::DistantEntity:
        case ReplicationChangeKind::None:
            return ReplicationReliability::Unreliable;
        }
        return ReplicationReliability::Unreliable;
    }

    // Priority per the frozen §7.A table (authoritative; no invented default).
    [[nodiscard]] constexpr ReplicationPriority ClassifyPriority(const ReplicationChangeKind k) noexcept
    {
        switch (k)
        {
        // §7.A High.
        case ReplicationChangeKind::Player:
        case ReplicationChangeKind::Combat:
        case ReplicationChangeKind::Damage:
        case ReplicationChangeKind::EntitySpawn:
        case ReplicationChangeKind::EntityRemove:
        case ReplicationChangeKind::PlayerJoin:
        case ReplicationChangeKind::PlayerLeave:
            return ReplicationPriority::High;
        // §7.A Medium.
        case ReplicationChangeKind::NearbyNpc:
        case ReplicationChangeKind::Inventory:
        case ReplicationChangeKind::Animation:
        case ReplicationChangeKind::Position:
        case ReplicationChangeKind::Rotation:
        case ReplicationChangeKind::Velocity:
        case ReplicationChangeKind::QuestUpdate:
            return ReplicationPriority::Medium;
        // §7.A Low.
        case ReplicationChangeKind::AmbientObject:
        case ReplicationChangeKind::WeatherUpdate:
        case ReplicationChangeKind::DistantEntity:
        case ReplicationChangeKind::Camera:
        case ReplicationChangeKind::None:
            return ReplicationPriority::Low;
        }
        return ReplicationPriority::Low;
    }

    // Channel derives from reliability (Control is reserved for future control msgs).
    [[nodiscard]] constexpr ReplicationChannel ClassifyChannel(const ReplicationChangeKind k) noexcept
    {
        return ClassifyReliability(k) == ReplicationReliability::Reliable ? ReplicationChannel::Reliable
                                                                          : ReplicationChannel::Unreliable;
    }
} // namespace stalkermp::replication
