// STALKER-MP — ALife Transition Layer value types (Sprint-005, Architecture §10)
//
// Foundational, engine-free value types for the ALife Transition Layer. This
// file introduces types ONLY — no manager, no gateway seam, no service, no
// pipeline logic, no engine interaction. (Sprint-005 Implementation Plan, Step 1.)
//
// Frozen architecture references:
//   - §10 Data structures (value types) — the shapes below are verbatim.
//   - §12 State machine — TransitionState.
//   - ADR-007: no engine headers (One Engine Boundary), no exceptions, no
//     throwing STL surface here; pure value types, trivially C4530-clean.
//   - ADR-008: this file names no engine call; the switching mechanism
//     (Cooperative Flag Override via set_switch_online/offline) lives entirely
//     behind the gateway introduced in a later step.
//
// Determinism: TransitionCommand carries a total order (ascending EntityId, then
// kind) so command batches are canonically ordered without depending on any
// engine or container iteration order.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/world/EntityView.h" // world::EntityId (persistent identity)

namespace stalkermp::world
{
    // Per-entity transition intent (Architecture §12). This is the module's
    // OWN intent state — never the engine's authoritative online/offline status,
    // which the engine alone owns (§11). Default is Offline.
    enum class TransitionState : std::uint8_t
    {
        Offline = 0,     // not tracked as online; no in-flight transition
        PendingOnline,   // activation applied; awaiting confirmation
        Online,          // confirmed online
        PendingOffline,  // deactivation applied; awaiting confirmation
    };

    [[nodiscard]] constexpr const char* TransitionStateName(const TransitionState state) noexcept
    {
        switch (state)
        {
        case TransitionState::Offline:        return "Offline";
        case TransitionState::PendingOnline:  return "PendingOnline";
        case TransitionState::Online:         return "Online";
        case TransitionState::PendingOffline: return "PendingOffline";
        }
        return "Unknown";
    }

    // The kind of switch a command requests (Architecture §10).
    enum class TransitionKind : std::uint8_t
    {
        Activate = 0,  // drive the entity online
        Deactivate,    // drive the entity offline
    };

    [[nodiscard]] constexpr const char* TransitionKindName(const TransitionKind kind) noexcept
    {
        switch (kind)
        {
        case TransitionKind::Activate:   return "Activate";
        case TransitionKind::Deactivate: return "Deactivate";
        }
        return "Unknown";
    }

    // Per-command outcome reported by the switch gateway (Architecture §9/§10).
    enum class TransitionOutcome : std::uint8_t
    {
        Applied = 0,     // the switch was performed this apply
        AlreadyInState,  // no-op: engine already in the requested state (idempotency)
        EntityMissing,   // entity unknown to the engine at apply time (benign skip)
        Failed,          // switch reported failure; prior intent retained
    };

    [[nodiscard]] constexpr const char* TransitionOutcomeName(const TransitionOutcome outcome) noexcept
    {
        switch (outcome)
        {
        case TransitionOutcome::Applied:        return "Applied";
        case TransitionOutcome::AlreadyInState: return "AlreadyInState";
        case TransitionOutcome::EntityMissing:  return "EntityMissing";
        case TransitionOutcome::Failed:         return "Failed";
        }
        return "Unknown";
    }

    // One ordered switch request (Architecture §10). Batches of these are applied
    // in a single deterministic phase, ascending by EntityId. Carries identity and
    // intent only — no engine handle, no pointer (One Engine Boundary).
    struct TransitionCommand
    {
        EntityId id{};
        TransitionKind kind = TransitionKind::Activate;

        [[nodiscard]] constexpr bool operator==(const TransitionCommand& other) const noexcept
        {
            return id == other.id && kind == other.kind;
        }

        [[nodiscard]] constexpr bool operator!=(const TransitionCommand& other) const noexcept
        {
            return !(*this == other);
        }

        // Total order for canonical batch ordering: ascending EntityId, then kind.
        // Keeps command batches deterministic independent of container/engine order.
        [[nodiscard]] constexpr bool operator<(const TransitionCommand& other) const noexcept
        {
            if (id.value != other.id.value)
            {
                return id.value < other.id.value;
            }
            return static_cast<std::uint8_t>(kind) < static_cast<std::uint8_t>(other.kind);
        }
    };

    // A command paired with its applied outcome (Architecture §10).
    struct TransitionRecord
    {
        EntityId id{};
        TransitionKind kind = TransitionKind::Activate;
        TransitionOutcome outcome = TransitionOutcome::Applied;
    };

    // Read-only, per-tick output of the Transition Layer (Architecture §10/§13).
    // This is exactly the activation delta Sprint-008 replication will consume.
    // Both id lists are ascending by EntityId.
    struct TransitionResult
    {
        std::vector<EntityId> broughtOnline;
        std::vector<EntityId> broughtOffline;
        std::uint64_t tick = 0;
    };

    // Snapshot of transition bookkeeping for diagnostics (Architecture §10).
    struct TransitionStatistics
    {
        std::size_t online = 0;
        std::size_t pendingOnline = 0;
        std::size_t pendingOffline = 0;
        std::size_t offlineTracked = 0;
        std::size_t appliedThisTick = 0;
        std::size_t skippedThisTick = 0;
        std::size_t failedThisTick = 0;
    };
} // namespace stalkermp::world
