// STALKER-MP — Player session observer bridge (Sprint-007, Step 6)
//
// See PlayerSessionObserver.h. Enqueue-only translation of Sprint-006 session
// join/leave callbacks into PlayerSessionDelta records. No mutation of any owned
// state; engine-free / OS-free.

#include "stalkermp/player/PlayerSessionObserver.h"

namespace stalkermp::player
{
    void PlayerSessionObserver::OnMemberJoined(const net::ConnectionId id, const std::uint64_t joinTick)
    {
        PlayerSessionDelta delta{};
        delta.kind = PlayerSessionEventKind::Joined;
        delta.connection = id;
        delta.tick = joinTick;
        // reason/reconnectToken not applicable to a join; left at defaults.
        m_queue.Enqueue(delta);
    }

    void PlayerSessionObserver::OnMemberLeft(const net::ConnectionId id, const net::DisconnectReason reason,
                                             const std::uint64_t reconnectToken)
    {
        PlayerSessionDelta delta{};
        delta.kind = PlayerSessionEventKind::Left;
        delta.connection = id;
        delta.reason = reason;
        delta.reconnectToken = reconnectToken;
        // tick left at default 0; the leave tick is not supplied by the callback.
        m_queue.Enqueue(delta);
    }
} // namespace stalkermp::player
