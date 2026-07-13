// STALKER-MP — Player subsystem value types (Sprint-007, Step 1)
//
// The engine-free / OS-free vocabulary for the player-lifecycle subsystem
// (Architecture §9). This file introduces TYPES ONLY — value structs,
// enumerations, and const char* name helpers. No registry, no configuration,
// no lifecycle logic, no services, no networking, no engine adapters.
//
// ADR-007: no engine headers (One Engine Boundary), no OS headers (One Platform
// Boundary), no exceptions, no RTTI, no iostream. Pure value types.
//
// Identity is REUSED, never redefined: PlayerId is world::PlayerId (Sprint-004).
// PlayerRecord references world::EntityId (Sprint-003), world::PlayerPosition
// (Sprint-004), and net::ConnectionId / net::DisconnectReason (Sprint-006) by
// value only. Value 0 is the "none" sentinel for every id (frozen contracts).

#pragma once

#include <cstdint>

#include "stalkermp/net/NetTypes.h"       // net::ConnectionId, net::DisconnectReason
#include "stalkermp/world/BubbleTypes.h"  // world::PlayerId, world::PlayerPosition
#include "stalkermp/world/EntityView.h"   // world::EntityId

namespace stalkermp::player
{
    // --- Identity (reused, not redefined) -----------------------------------
    // PlayerId is an identity value ONLY — never a connection, session, or
    // network handle. Value 0 = "none" (per the frozen world::PlayerId contract).
    using PlayerId = world::PlayerId;

    // --- Enumerations (fixed std::uint8_t layout; additive order) ------------

    // Host-authoritative lifecycle state of a player (Architecture §9).
    enum class PlayerLifecycleState : std::uint8_t
    {
        Joining,          // allocated, join pipeline in progress
        Active,           // live, controllable
        Dead,             // died; awaiting respawn scheduling
        AwaitingRespawn,  // respawn scheduled (tick-derived)
        Suspended,        // disconnected but retained (persistent)
        Removed,          // explicitly removed / released
    };

    [[nodiscard]] constexpr const char* PlayerLifecycleStateName(const PlayerLifecycleState s) noexcept
    {
        switch (s)
        {
        case PlayerLifecycleState::Joining:         return "Joining";
        case PlayerLifecycleState::Active:          return "Active";
        case PlayerLifecycleState::Dead:            return "Dead";
        case PlayerLifecycleState::AwaitingRespawn: return "AwaitingRespawn";
        case PlayerLifecycleState::Suspended:       return "Suspended";
        case PlayerLifecycleState::Removed:         return "Removed";
        }
        return "Unknown";
    }

    // Connection binding state of a player (Architecture §9).
    enum class PlayerConnectionState : std::uint8_t
    {
        Connected,  // bound to a live connection
        Suspended,  // connection lost; record retained for reclaim
        Reclaimed,  // reconnected; rebound to a new connection
    };

    [[nodiscard]] constexpr const char* PlayerConnectionStateName(const PlayerConnectionState s) noexcept
    {
        switch (s)
        {
        case PlayerConnectionState::Connected: return "Connected";
        case PlayerConnectionState::Suspended: return "Suspended";
        case PlayerConnectionState::Reclaimed: return "Reclaimed";
        }
        return "Unknown";
    }

    // Disposition of a disconnect: retain (suspend) vs release (remove).
    // Architecture §12/§17: ordinary disconnect retains; explicit removal releases.
    enum class DisconnectDisposition : std::uint8_t
    {
        Retain,  // suspend and keep the record/entity (persistent)
        Remove,  // release the record and despawn the owned entity
    };

    [[nodiscard]] constexpr const char* DisconnectDispositionName(const DisconnectDisposition d) noexcept
    {
        switch (d)
        {
        case DisconnectDisposition::Retain: return "Retain";
        case DisconnectDisposition::Remove: return "Remove";
        }
        return "Unknown";
    }

    // Per-stage join result vocabulary (Architecture §18/§19). Value only.
    enum class JoinOutcome : std::uint8_t
    {
        Accepted,
        RejectedCapacity,
        RejectedDuplicate,
        RejectedInvalid,
        RejectedSpawnFailed,
    };

    [[nodiscard]] constexpr const char* JoinOutcomeName(const JoinOutcome o) noexcept
    {
        switch (o)
        {
        case JoinOutcome::Accepted:            return "Accepted";
        case JoinOutcome::RejectedCapacity:    return "RejectedCapacity";
        case JoinOutcome::RejectedDuplicate:   return "RejectedDuplicate";
        case JoinOutcome::RejectedInvalid:     return "RejectedInvalid";
        case JoinOutcome::RejectedSpawnFailed: return "RejectedSpawnFailed";
        }
        return "Unknown";
    }

    // Spawn/despawn result vocabulary (Architecture §18). Value only.
    enum class SpawnOutcome : std::uint8_t
    {
        Spawned,
        EntityMissing,
        EngineUnavailable,
        RejectedInvalid,
    };

    [[nodiscard]] constexpr const char* SpawnOutcomeName(const SpawnOutcome o) noexcept
    {
        switch (o)
        {
        case SpawnOutcome::Spawned:           return "Spawned";
        case SpawnOutcome::EntityMissing:     return "EntityMissing";
        case SpawnOutcome::EngineUnavailable: return "EngineUnavailable";
        case SpawnOutcome::RejectedInvalid:   return "RejectedInvalid";
        }
        return "Unknown";
    }

    // Reconnect (reclaim) result vocabulary (Architecture §18). Value only.
    enum class ReconnectOutcome : std::uint8_t
    {
        Reclaimed,
        TokenUnknown,
        AlreadyActive,
        RejectedInvalid,
    };

    [[nodiscard]] constexpr const char* ReconnectOutcomeName(const ReconnectOutcome o) noexcept
    {
        switch (o)
        {
        case ReconnectOutcome::Reclaimed:       return "Reclaimed";
        case ReconnectOutcome::TokenUnknown:    return "TokenUnknown";
        case ReconnectOutcome::AlreadyActive:   return "AlreadyActive";
        case ReconnectOutcome::RejectedInvalid: return "RejectedInvalid";
        }
        return "Unknown";
    }

    // --- Structures (POD-style aggregates; default member initializers) ------

    // The canonical player record (Architecture §9/§10). Identity + lifecycle +
    // the PlayerId<->ConnectionId<->SessionMember<->EntityId mapping + tick-derived
    // timing. Carries no engine/platform state; no heap ownership.
    struct PlayerRecord
    {
        PlayerId id{};                        // owning identity (0 = none)
        net::ConnectionId connection{};       // current bound connection (0 = none when suspended)
        std::uint64_t sessionMember = 0;      // Sprint-006 session-member key (value only)
        world::EntityId entity{};             // persistent world entity (0 = none until spawned)
        PlayerLifecycleState lifecycle = PlayerLifecycleState::Joining;
        PlayerConnectionState connectionState = PlayerConnectionState::Connected;
        world::PlayerPosition lastPosition{}; // last host-owned position (positions-only)
        std::uint64_t joinTick = 0;           // tick of join (tick-derived)
        std::uint64_t respawnEligibleTick = 0;// scheduled respawn tick (0 = not scheduled)
        std::uint64_t reconnectToken = 0;     // Sprint-006 deterministic reclaim token (value only)
    };

    // Read-only projection of the id<->id mapping (Architecture §9/§10). Returned
    // by the Step-4 Find* lookups; a value type here.
    struct PlayerMappingView
    {
        PlayerId id{};
        net::ConnectionId connection{};
        std::uint64_t sessionMember = 0;
        world::EntityId entity{};

        [[nodiscard]] constexpr bool operator==(const PlayerMappingView& o) const noexcept
        {
            return id == o.id && connection == o.connection && sessionMember == o.sessionMember &&
                   entity == o.entity;
        }
        [[nodiscard]] constexpr bool operator!=(const PlayerMappingView& o) const noexcept
        {
            return !(*this == o);
        }
    };

    // Read-only tallies (Architecture §9/§20). Tick-derived; populated by Step-12
    // diagnostics, not here. Aggregate snapshot (no comparison operators).
    struct PlayerStatistics
    {
        std::uint32_t connected = 0;
        std::uint32_t suspended = 0;
        std::uint32_t dead = 0;
        std::uint32_t respawns = 0;
        std::uint32_t deaths = 0;
        std::uint32_t reconnects = 0;
        std::uint64_t joinTick = 0;
        std::uint64_t averageSessionDurationTicks = 0;
    };
} // namespace stalkermp::player
