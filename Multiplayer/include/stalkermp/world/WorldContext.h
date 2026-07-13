// STALKER-MP — World context (Sprint-002, §7.4)
//
// Immutable snapshot of deterministic, world-intrinsic simulation state,
// published once per world tick by WorldManager and handed to consumers by
// value. Contains NO player, networking, or session state (Sprint-002 §7.4
// note): player influence reaches world machinery exclusively through the
// Bubble Manager input (Sprint-004, RFC-0002).
//
// This is the module's first immutable-handoff type; its value semantics
// deliberately anticipate the snapshot architecture of RFC-0003.

#pragma once

#include <cstdint>

#include "stalkermp/world/WorldTypes.h"

namespace stalkermp::world
{
    struct WorldContext
    {
        std::uint64_t tick = 0;            // Monotonic world tick index.
        double deltaSeconds = 0.0;         // Wall-clock delta for this tick.
        double simulationSeconds = 0.0;    // Accumulated simulation time (speed-scaled; frozen while paused).
        WorldTimeOfDay timeOfDay{};        // Derived in-world time of day.
        WeatherId weather{};               // Active weather profile identifier.
        bool emissionActive = false;       // Global emission state.
    };
} // namespace stalkermp::world
