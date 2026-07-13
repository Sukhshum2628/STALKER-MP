// STALKER-MP — Player spawn gateway seam (Sprint-007, Step 7 declares it; Step 9
// delivers the null; Step 10 delivers the engine adapter).
//
// The single engine-free boundary through which the player subsystem materializes
// and removes the persistent ALife player object. PlayerManager depends ONLY on
// this abstract interface (Implementation Plan Clarifications §7.1). The interface
// names no engine call and exposes no engine type (mirrors Sprint-005
// IAlifeSwitchGateway). Online/offline switching is NOT here — it remains the
// Sprint-005 Bubble->Transition pipeline (ADR-008).
//
// Engine-free / OS-free. ADR-007: no exceptions; results are value outcomes.

#pragma once

#include <cstdint>

#include "stalkermp/core/Expected.h"
#include "stalkermp/player/PlayerTypes.h" // SpawnOutcome
#include "stalkermp/world/BubbleTypes.h"  // world::PlayerPosition
#include "stalkermp/world/EntityView.h"   // world::EntityId

namespace stalkermp::player
{
    // Engine-free, opaque character-profile handle. NOT an engine type; the engine
    // adapter (Step 10) maps it to a real profile behind the boundary. Value 0 is
    // the default profile.
    struct PlayerProfile
    {
        std::uint64_t profileId = 0;
    };

    class IPlayerSpawnGateway
    {
    public:
        virtual ~IPlayerSpawnGateway() = default;

        // Materialize the persistent player entity at a spawn position. Returns its
        // EntityId on success, a value error on failure. Does NOT switch the entity
        // online/offline (Sprint-005 owns that).
        [[nodiscard]] virtual core::Expected<world::EntityId> Spawn(const PlayerProfile& profile,
                                                                    const world::PlayerPosition& position) = 0;

        // Remove a previously spawned player entity. Value outcome; never throws.
        [[nodiscard]] virtual SpawnOutcome Despawn(world::EntityId entity) = 0;
    };
} // namespace stalkermp::player
