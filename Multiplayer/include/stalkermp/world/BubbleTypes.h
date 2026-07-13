// STALKER-MP — Bubble subsystem value types (Sprint-004, §7.1/§7.6; Design Review §3)
//
// Foundational, engine-free value types for the Bubble Manager. This file
// introduces types only — no manager, no player/spatial sources, no computation,
// no events, no engine interaction.
//
// ADR-007: no engine headers, no engine pointers (One Engine Boundary), no
// exceptions, no throwing STL. Pure value types.

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/world/EntityView.h" // world::Vec3

namespace stalkermp::world
{
    // Lightweight opaque player identifier (Design Review §3A / invariant B11).
    // This is an identity value ONLY — never a connection, session, or network
    // handle. Value 0 is reserved as "none".
    struct PlayerId
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool operator==(const PlayerId& other) const noexcept { return value == other.value; }
        [[nodiscard]] constexpr bool operator!=(const PlayerId& other) const noexcept { return value != other.value; }
    };

    // A player's contribution to bubble computation: position only (B11), plus the
    // tick it was last updated. Carries no connection/session/network state. This
    // is the payload the future IPlayerPositionSource seam will supply; the seam
    // itself is a later step.
    struct PlayerPosition
    {
        PlayerId id{};
        Vec3 position{};
        std::uint64_t lastUpdateTick = 0;
    };

    // Classification of a single entity against the unified bubble (§7.6).
    // This is the REGISTRATION-independent activation membership, distinct from the
    // Sprint-003 registration state and from ALife online/offline (Sprint-005).
    enum class BubbleMembership : std::uint8_t
    {
        Outside = 0,          // outside the unified region
        Inside,               // inside the unified region
        PendingActivation,    // crossed inward this evaluation (offline -> online)
        PendingDeactivation,  // crossed outward this evaluation (online -> offline)
    };

    [[nodiscard]] constexpr const char* BubbleMembershipName(const BubbleMembership membership) noexcept
    {
        switch (membership)
        {
        case BubbleMembership::Outside:             return "Outside";
        case BubbleMembership::Inside:              return "Inside";
        case BubbleMembership::PendingActivation:   return "PendingActivation";
        case BubbleMembership::PendingDeactivation: return "PendingDeactivation";
        }
        return "Outside";
    }

    // Direction of an activation-boundary crossing (future informational events,
    // §7.8). Value type only; event dispatch is a later step.
    enum class EntityActivationTransition : std::uint8_t
    {
        Entered = 0, // Outside -> Inside
        Exited,      // Inside  -> Outside
    };

    [[nodiscard]] constexpr const char* EntityActivationTransitionName(const EntityActivationTransition t) noexcept
    {
        switch (t)
        {
        case EntityActivationTransition::Entered: return "Entered";
        case EntityActivationTransition::Exited:  return "Exited";
        }
        return "Entered";
    }

    // Immutable per-tick summary of the computed activation state (§7.1
    // BubbleState). Populated by the Bubble Manager in a later step; defined here
    // as the stable snapshot value type. Counts only — no session state (B11).
    struct BubbleState
    {
        std::size_t activePlayerCount = 0;  // number of players contributing this tick
        std::size_t onlineEntityCount = 0;  // entities inside the unified region
        std::uint64_t updateTick = 0;       // tick this state was computed

        [[nodiscard]] constexpr bool IsActive() const noexcept { return activePlayerCount > 0; }
    };
} // namespace stalkermp::world
