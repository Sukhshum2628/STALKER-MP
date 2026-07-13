// STALKER-MP — Player session observer bridge (Sprint-007, Step 6)
//
// Subscribes to the Sprint-006 net::ISessionObserver join/leave events and
// records them into a PlayerDeltaQueue — ENQUEUE ONLY. It performs no registry,
// gateway, entity, or session mutation during the callback (which fires in the
// networking tick), preserving "networking owns no simulation" and determinism
// (Architecture AD-3, R-7). Non-owning: holds a PlayerDeltaQueue& supplied by the
// owner (the Step-7 manager owns the queue).
//
// Engine-free / OS-free. ADR-007. ADR-011 (single-threaded, synchronous).

#pragma once

#include <cstdint>

#include "stalkermp/net/ISessionObserver.h" // net::ISessionObserver, ConnectionId, DisconnectReason
#include "stalkermp/player/PlayerDeltaQueue.h"

namespace stalkermp::player
{
    class PlayerSessionObserver final : public net::ISessionObserver
    {
    public:
        explicit PlayerSessionObserver(PlayerDeltaQueue& queue) : m_queue(queue) {}

        // Enqueue only — no mutation of any owned state.
        void OnMemberJoined(net::ConnectionId id, std::uint64_t joinTick) override;
        void OnMemberLeft(net::ConnectionId id, net::DisconnectReason reason,
                          std::uint64_t reconnectToken) override;

    private:
        PlayerDeltaQueue& m_queue; // non-owning
    };
} // namespace stalkermp::player
