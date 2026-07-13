// STALKER-MP — Snapshot value types (Sprint-008, Step 1)
//
// The engine-free / OS-free vocabulary for the immutable Snapshot System
// (Architecture §9/§10). This file introduces TYPES ONLY — value structs,
// enumerations, and const char* name helpers. No builder, pool, queue, manager,
// configuration, diagnostics, or engine seam; no allocation; no logic beyond
// lightweight value helpers.
//
// ADR-007: no engine headers (One Engine Boundary), no OS headers (One Platform
// Boundary), no exceptions, no RTTI, no iostream. Deterministic fixed layout.
//
// Snapshots capture VALUES ONLY — never a raw pointer or a handle into live
// simulation. Reused ids: world::EntityId / world::Vec3 (Sprint-002),
// player::PlayerId / player::PlayerConnectionState (Sprint-007). Value 0 is the
// "none" sentinel for every id. `timestampWallClock` is a DIAGNOSTIC field
// excluded from replay identity (Architecture §13).

#pragma once

#include <cstdint>

#include "stalkermp/player/PlayerTypes.h" // player::PlayerId, player::PlayerConnectionState
#include "stalkermp/world/EntityView.h"   // world::EntityId, world::Vec3

namespace stalkermp::snapshot
{
    // --- Identity -------------------------------------------------------------
    // Monotonic, host-assigned snapshot id. Value 0 = "none". Never reused.
    struct SnapshotId
    {
        std::uint64_t value = 0;

        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const SnapshotId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const SnapshotId& o) const noexcept { return value != o.value; }
    };

    // --- Enumerations (fixed std::uint8_t layout; additive order) -------------

    // Snapshot projection kind (Architecture §9). Only `Simulation` is materialized
    // in Sprint-008; Replication/Persistence/Thread are RESERVED projections
    // (materialized by their owning sprints — Architecture §22).
    enum class SnapshotKind : std::uint8_t
    {
        Simulation,
        Replication,
        Persistence,
        Thread,
    };

    [[nodiscard]] constexpr const char* SnapshotKindName(const SnapshotKind k) noexcept
    {
        switch (k)
        {
        case SnapshotKind::Simulation:  return "Simulation";
        case SnapshotKind::Replication: return "Replication";
        case SnapshotKind::Persistence: return "Persistence";
        case SnapshotKind::Thread:      return "Thread";
        }
        return "Unknown";
    }

    // Lifecycle state (Architecture §9/§12). A snapshot is immutable once Finalized.
    enum class SnapshotState : std::uint8_t
    {
        Building,   // under construction on the frame thread
        Finalized,  // sealed — immutable
        Published,  // handed to the queue for consumers
        Retired,    // ref-count zero + superseded; buffer returned to the pool
    };

    [[nodiscard]] constexpr const char* SnapshotStateName(const SnapshotState s) noexcept
    {
        switch (s)
        {
        case SnapshotState::Building:  return "Building";
        case SnapshotState::Finalized: return "Finalized";
        case SnapshotState::Published: return "Published";
        case SnapshotState::Retired:   return "Retired";
        }
        return "Unknown";
    }

    // --- Structures (POD-style aggregates; default member initializers) -------

    // Snapshot header (Architecture §7.9/§9). `timestampWallClock` is diagnostic
    // only and is EXCLUDED from replay identity; deterministic identity is
    // (id, simulationTick, content).
    struct SnapshotMetadata
    {
        SnapshotId id{};                       // monotonic (0 = none)
        std::uint64_t simulationTick = 0;      // tick the snapshot captures
        std::uint32_t version = 0;             // snapshot format version
        std::uint32_t entityCount = 0;
        std::uint32_t playerCount = 0;
        std::uint64_t buildDurationTicks = 0;  // diagnostic (tick-derived)
        std::uint64_t timestampWallClock = 0;  // DIAGNOSTIC ONLY — not replay identity
        SnapshotKind kind = SnapshotKind::Simulation;
        SnapshotState state = SnapshotState::Building;
    };

    // Per-entity captured value data (Architecture §7.6). Values only; never a
    // raw pointer. `position` is the captured transform; `inventoryRef` is a value
    // reference (EntityId), never a pointer.
    struct EntitySnapshot
    {
        world::EntityId id{};          // 0 = none
        world::Vec3 position{};        // captured transform
        world::Vec3 velocity{};
        std::uint32_t state = 0;       // opaque runtime state id
        std::uint32_t flags = 0;       // opaque flag bits
        world::EntityId inventoryRef{};// value reference (0 = none)
        std::uint64_t runtimeState = 0;// opaque runtime state word
    };

    // Per-player captured value data (Architecture §7.7).
    struct PlayerSnapshot
    {
        player::PlayerId id{};         // 0 = none
        world::EntityId entity{};      // owning entity (0 = none)
        player::PlayerConnectionState connectionState = player::PlayerConnectionState::Connected;
        world::Vec3 position{};        // current position (positions-only)
        std::uint32_t simulationState = 0; // opaque
        std::uint32_t authorityFlags = 0;  // opaque host-authority flags
    };

    // Captured environment (Architecture §7.8). Value copy of the per-tick
    // environment; engine-free (integers only).
    struct EnvironmentSnapshot
    {
        std::uint64_t simulationTick = 0;
        std::uint32_t weatherId = 0;
        std::uint32_t timeOfDaySeconds = 0;
        std::uint32_t emissionState = 0;
        std::uint32_t lighting = 0;
        std::uint32_t environmentVersion = 0;
    };

    // Read-only tallies (Architecture §20). Populated by the manager/diagnostics
    // in later steps; a value snapshot here.
    struct SnapshotStatistics
    {
        std::uint64_t built = 0;
        std::uint64_t published = 0;
        std::uint64_t retired = 0;
        std::uint32_t queueDepth = 0;
        std::uint32_t poolInUse = 0;
        std::uint32_t poolCapacity = 0;
        std::uint64_t lastBuildDurationTicks = 0;
        std::uint32_t entityCount = 0;
        std::uint32_t playerCount = 0;
    };

    // Read-only consistency report (Architecture §20). Extended/populated by the
    // diagnostics step; the value shape is fixed here.
    struct SnapshotConsistency
    {
        bool idMonotonic = true;           // SnapshotIds strictly increasing
        bool noDuplicateIds = true;        // no duplicate entity/player ids within a snapshot
        bool countsMatch = true;           // metadata counts == captured counts
        bool entitiesAscending = true;     // entities ordered ascending by EntityId
        bool playersAscending = true;      // players ordered ascending by PlayerId
        bool immutableWhenPublished = true;// published snapshots are Finalized/immutable
        bool noRetiredAliasing = true;     // no consumer references a retired buffer

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return idMonotonic && noDuplicateIds && countsMatch && entitiesAscending && playersAscending &&
                   immutableWhenPublished && noRetiredAliasing;
        }
    };
} // namespace stalkermp::snapshot
