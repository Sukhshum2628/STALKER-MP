// STALKER-MP — Player session delta queue (Sprint-007, Step 6)
//
// The enqueue-only queue that captures Sprint-006 Session join/leave events for
// deterministic later application (Step 7 drains it at the player tick). The
// observer bridge (PlayerSessionObserver) only ENQUEUES here — no registry,
// gateway, entity, or session mutation happens during the networking tick
// (Architecture AD-3, R-7). Drain returns deltas in a fixed deterministic order
// (ascending ConnectionId, ties broken by arrival) and empties the queue.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "stalkermp/net/NetTypes.h" // net::ConnectionId, net::DisconnectReason

namespace stalkermp::player
{
    enum class PlayerSessionEventKind : std::uint8_t
    {
        Joined,
        Left,
    };

    [[nodiscard]] constexpr const char* PlayerSessionEventKindName(const PlayerSessionEventKind k) noexcept
    {
        switch (k)
        {
        case PlayerSessionEventKind::Joined: return "Joined";
        case PlayerSessionEventKind::Left:   return "Left";
        }
        return "Unknown";
    }

    // A single captured session event. Carries exactly the data the Sprint-006
    // ISessionObserver callbacks supply (plus the kind) — no derived/inferred
    // data, preserving enqueue-only.
    struct PlayerSessionDelta
    {
        PlayerSessionEventKind kind{};
        net::ConnectionId connection{};
        std::uint64_t tick = 0;                          // join tick (Joined) / leave tick (Left)
        net::DisconnectReason reason = net::DisconnectReason::None; // meaningful for Left
        std::uint64_t reconnectToken = 0;                // meaningful for Left
    };

    // Ordered, enqueue-only queue. Drain empties it and returns the deltas in the
    // fixed deterministic order.
    class PlayerDeltaQueue
    {
    public:
        void Enqueue(const PlayerSessionDelta& delta) { m_deltas.push_back(delta); }

        // Return all queued deltas in deterministic order (ascending ConnectionId,
        // ties broken by arrival order via stable sort) and empty the queue.
        [[nodiscard]] std::vector<PlayerSessionDelta> Drain()
        {
            std::vector<PlayerSessionDelta> out = std::exchange(m_deltas, {});
            std::stable_sort(out.begin(), out.end(),
                             [](const PlayerSessionDelta& a, const PlayerSessionDelta& b) {
                                 return a.connection.value < b.connection.value;
                             });
            return out;
        }

        [[nodiscard]] std::size_t size() const noexcept { return m_deltas.size(); }
        [[nodiscard]] bool empty() const noexcept { return m_deltas.empty(); }

    private:
        std::vector<PlayerSessionDelta> m_deltas; // arrival order until drained
    };
} // namespace stalkermp::player
