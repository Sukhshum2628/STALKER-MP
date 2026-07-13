// STALKER-MP — Player position source seam (Sprint-004, §7.2; Design Review §3A)
//
// The positions-only abstraction that supplies connected players' positions to
// the Bubble Manager. It carries NO connection, session, or network state — only
// position (invariant B11; the WorldContext Player Count Leak lesson). This keeps
// the World subsystem free of player/session state.
//
// Sprint-004 provides a local in-memory stub (LocalPlayerPositionSource). The real
// networked source (Sprint-006/007) implements this same interface behind the seam;
// the Bubble Manager never changes.
//
// ADR-007: engine-agnostic; no engine headers, no engine pointers; no exceptions.

#pragma once

#include <vector>

#include "stalkermp/world/BubbleTypes.h" // world::PlayerPosition, world::PlayerId

namespace stalkermp::world
{
    class IPlayerPositionSource
    {
    public:
        virtual ~IPlayerPositionSource() = default;

        // Immutable snapshot of the active players, returned by value, in
        // ascending PlayerId order (invariant B9). Position only (B11).
        [[nodiscard]] virtual std::vector<PlayerPosition> ActivePlayers() const = 0;
    };
} // namespace stalkermp::world
