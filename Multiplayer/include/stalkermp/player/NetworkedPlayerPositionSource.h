// STALKER-MP — Networked player position source (Sprint-007, Step 8)
//
// Implements the frozen Sprint-004 world::IPlayerPositionSource by delegating to
// PlayerManager's active-player positions. Positions ONLY — no connection,
// session, or lifecycle state crosses into the World subsystem (invariant B11);
// ascending PlayerId (invariant B9). The Bubble Manager and its interface are
// UNCHANGED; this is a drop-in source implementation. Binding it into the Bubble
// is a later step (Step 11).
//
// Engine-free / OS-free. ADR-007. Non-owning (holds a const PlayerManager&).

#pragma once

#include <vector>

#include "stalkermp/player/PlayerManager.h"
#include "stalkermp/world/BubbleTypes.h"          // world::PlayerPosition
#include "stalkermp/world/IPlayerPositionSource.h" // world::IPlayerPositionSource

namespace stalkermp::player
{
    class NetworkedPlayerPositionSource final : public world::IPlayerPositionSource
    {
    public:
        explicit NetworkedPlayerPositionSource(const PlayerManager& manager) : m_manager(manager) {}

        // Immutable ascending (by PlayerId) snapshot of active players' positions.
        // Positions only; no leaked connection/session/lifecycle state.
        [[nodiscard]] std::vector<world::PlayerPosition> ActivePlayers() const override;

    private:
        const PlayerManager& m_manager; // non-owning
    };
} // namespace stalkermp::player
