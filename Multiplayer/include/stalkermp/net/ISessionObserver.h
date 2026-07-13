// STALKER-MP — Session observer seam (Sprint-006, Step 8)
//
// Join/leave notifications for future sprints (persistence, reconnect). Non-owning
// subscribers; fired exactly once per event, in registration order. Engine-free /
// OS-free. ADR-007.

#pragma once

#include <cstdint>

#include "stalkermp/net/NetTypes.h" // ConnectionId, DisconnectReason

namespace stalkermp::net
{
    class ISessionObserver
    {
    public:
        virtual ~ISessionObserver() = default;

        virtual void OnMemberJoined(ConnectionId id, std::uint64_t joinTick) = 0;
        virtual void OnMemberLeft(ConnectionId id, DisconnectReason reason, std::uint64_t reconnectToken) = 0;
    };
} // namespace stalkermp::net
