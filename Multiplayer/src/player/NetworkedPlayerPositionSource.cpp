// STALKER-MP — Networked player position source (Sprint-007, Step 8)
//
// See NetworkedPlayerPositionSource.h. Positions-only delegation to the
// PlayerManager; the Bubble Manager and world::IPlayerPositionSource are unchanged.
// Engine-free / OS-free.

#include "stalkermp/player/NetworkedPlayerPositionSource.h"

namespace stalkermp::player
{
    std::vector<world::PlayerPosition> NetworkedPlayerPositionSource::ActivePlayers() const
    {
        // PlayerManager already returns positions only, ascending by PlayerId,
        // for Active players (invariants B9/B11). No transformation needed.
        return m_manager.ActivePlayerPositions();
    }
} // namespace stalkermp::player
