// STALKER-MP — Replication value types & vocabulary (Sprint-009, Step 1)
//
// The engine-free, OS-free foundational vocabulary of the Replication Pipeline:
// identity types, enumerations, and POD value structures that every later
// Sprint-009 step builds on. Types ONLY — no logic, no service, no tick, no
// thread, no packets, no message ids. Mirrors snapshot::SnapshotTypes (Sprint-008
// Step 1): value-only, deterministic layout, `0 = none`, wall-clock excluded from
// replay identity.
//
// Replication transports authoritative state to clients; it owns no entities,
// executes no gameplay, and mutates no simulation. These value captures never
// hold a live object or raw pointer (Host Authority; ADR-008 read-only).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/player/PlayerTypes.h" // player::PlayerId, player::PlayerConnectionState
#include "stalkermp/world/EntityView.h"   // world::EntityId, world::Vec3

namespace stalkermp::replication
{
    // ------------------------------------------------------------ Identity types

    // A replication target (maps to a connected client). 0 = none.
    struct ClientId
    {
        std::uint64_t value = 0;
        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
    };

    [[nodiscard]] constexpr bool operator==(const ClientId a, const ClientId b) noexcept
    {
        return a.value == b.value;
    }
    [[nodiscard]] constexpr bool operator!=(const ClientId a, const ClientId b) noexcept
    {
        return a.value != b.value;
    }

    // Monotonic identity of a (future) replication update. 0 = none.
    struct ReplicationId
    {
        std::uint64_t value = 0;
        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
    };

    [[nodiscard]] constexpr bool operator==(const ReplicationId a, const ReplicationId b) noexcept
    {
        return a.value == b.value;
    }
    [[nodiscard]] constexpr bool operator!=(const ReplicationId a, const ReplicationId b) noexcept
    {
        return a.value != b.value;
    }

    // ------------------------------------------------------------- Enumerations
    // Fixed std::uint8_t underlying type (deterministic ABI); reserved 0 carries
    // the neutral/safe meaning; new enumerators are appended, never renumbered.

    enum class ReplicationReliability : std::uint8_t
    {
        Unreliable = 0,
        Reliable,
    };

    [[nodiscard]] constexpr const char* ReplicationReliabilityName(const ReplicationReliability r) noexcept
    {
        switch (r)
        {
        case ReplicationReliability::Unreliable: return "Unreliable";
        case ReplicationReliability::Reliable:   return "Reliable";
        }
        return "Unknown";
    }

    enum class ReplicationPriority : std::uint8_t
    {
        Low = 0,
        Medium,
        High,
    };

    [[nodiscard]] constexpr const char* ReplicationPriorityName(const ReplicationPriority p) noexcept
    {
        switch (p)
        {
        case ReplicationPriority::Low:    return "Low";
        case ReplicationPriority::Medium: return "Medium";
        case ReplicationPriority::High:   return "High";
        }
        return "Unknown";
    }

    enum class ReplicationChannel : std::uint8_t
    {
        Unreliable = 0,
        Reliable,
        Control,
    };

    [[nodiscard]] constexpr const char* ReplicationChannelName(const ReplicationChannel c) noexcept
    {
        switch (c)
        {
        case ReplicationChannel::Unreliable: return "Unreliable";
        case ReplicationChannel::Reliable:   return "Reliable";
        case ReplicationChannel::Control:    return "Control";
        }
        return "Unknown";
    }

    enum class ReplicationState : std::uint8_t
    {
        Pending = 0,
        Built,
        Queued,
        Sent,
        Acknowledged,
        Dropped,
    };

    [[nodiscard]] constexpr const char* ReplicationStateName(const ReplicationState s) noexcept
    {
        switch (s)
        {
        case ReplicationState::Pending:      return "Pending";
        case ReplicationState::Built:        return "Built";
        case ReplicationState::Queued:       return "Queued";
        case ReplicationState::Sent:         return "Sent";
        case ReplicationState::Acknowledged: return "Acknowledged";
        case ReplicationState::Dropped:      return "Dropped";
        }
        return "Unknown";
    }

    // Named value outcome for future fallible replication operations (ADR-007).
    enum class ReplicationOutcome : std::uint8_t
    {
        Ok = 0,
        ClientUnknown,
        SnapshotUnavailable,
        Overflow,
        NotRelevant,
    };

    [[nodiscard]] constexpr const char* ReplicationOutcomeName(const ReplicationOutcome o) noexcept
    {
        switch (o)
        {
        case ReplicationOutcome::Ok:                  return "Ok";
        case ReplicationOutcome::ClientUnknown:       return "ClientUnknown";
        case ReplicationOutcome::SnapshotUnavailable: return "SnapshotUnavailable";
        case ReplicationOutcome::Overflow:            return "Overflow";
        case ReplicationOutcome::NotRelevant:         return "NotRelevant";
        }
        return "Unknown";
    }

    // ---------------------------------------------------------- Value captures
    // Values only — never a live object, reference, or handle into simulation.

    struct EntityReplicationState
    {
        world::EntityId id{};      // 0 = none
        world::Vec3 position{};
        world::Vec3 velocity{};
        std::uint32_t stateFlags = 0; // opaque
        std::uint32_t version = 0;    // per-entity replication version
        ReplicationPriority priority = ReplicationPriority::Low;
        ReplicationReliability reliability = ReplicationReliability::Unreliable;
    };

    struct PlayerReplicationState
    {
        player::PlayerId id{};    // 0 = none
        world::EntityId entity{}; // 0 = none
        world::Vec3 position{};
        player::PlayerConnectionState connectionState = player::PlayerConnectionState::Connected;
        std::uint32_t authorityFlags = 0; // opaque, host-authoritative (read-only)
        std::uint32_t version = 0;
    };

    // Provenance + summary of a (future) replication update. timestampWallClock is
    // DIAGNOSTIC ONLY and never participates in replay identity.
    struct ReplicationMetadata
    {
        ReplicationId id{};
        ClientId client{};
        std::uint64_t sourceSnapshotId = 0;   // which SimulationSnapshot this derives from
        std::uint64_t sourceSnapshotTick = 0;
        std::uint32_t entityCount = 0;
        std::uint32_t playerCount = 0;
        std::uint32_t byteCount = 0;          // reserved (0 until packet assembly)
        ReplicationReliability reliability = ReplicationReliability::Unreliable;
        ReplicationState state = ReplicationState::Pending;
        std::uint64_t timestampWallClock = 0; // DIAGNOSTIC ONLY — not replay identity
    };

    // Read-only aggregate tallies (populated by the manager/diagnostics later).
    struct ReplicationStatistics
    {
        std::uint64_t updatesBuilt = 0;
        std::uint64_t updatesSent = 0;
        std::uint64_t updatesDropped = 0;
        std::uint64_t bytesSent = 0;
        std::uint32_t entitiesReplicated = 0;
        std::uint32_t playersReplicated = 0;
        std::uint32_t activeClients = 0;      // gauge: clients processed in the last tick
        std::uint64_t lastBuildDurationTicks = 0;
        // Diagnostic collector counters (Sprint-009, Step 13; additive, POD, 0 =
        // none). Monotonic across RecordTick/RecordPacket/RecordAck/RecordOverflow.
        std::uint64_t ticks = 0;
        std::uint64_t reliablePackets = 0;
        std::uint64_t unreliablePackets = 0;
        std::uint64_t acksApplied = 0;
        std::uint64_t acksIgnored = 0;
        std::uint64_t overflows = 0;
    };

    // Read-only consistency report; the value shape is fixed here and populated by
    // later steps/diagnostics. Healthy by default.
    struct ReplicationConsistency
    {
        bool idMonotonic = true;                  // ReplicationIds strictly increasing
        bool consumesImmutableSnapshotsOnly = true;
        bool noLiveObjectCaptured = true;         // captures are values only
        bool interestRelevantOnly = true;         // only relevant entities replicated
        bool reliabilityClassified = true;        // every record classified
        bool prioritiesOrdered = true;            // priority ordering total
        bool noSimulationMutation = true;         // replication mutates nothing

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return idMonotonic && consumesImmutableSnapshotsOnly && noLiveObjectCaptured &&
                   interestRelevantOnly && reliabilityClassified && prioritiesOrdered && noSimulationMutation;
        }
    };
} // namespace stalkermp::replication
