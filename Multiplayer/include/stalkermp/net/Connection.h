// STALKER-MP — Connection record (Sprint-006, Step 5)
//
// Value record for one remote peer's link state. Storage + lifecycle state only —
// NO transport interaction, handshake logic, timeouts, or queues (later steps).
// Owns no simulation data and no player identity. Engine-free / OS-free. ADR-007.

#pragma once

#include <cstdint>

#include "stalkermp/net/NetTypes.h" // ConnectionId, TransportEndpoint, ConnectionState, HandshakeState, DisconnectReason

namespace stalkermp::net
{
    struct Connection
    {
        ConnectionId id{};                                    // host-assigned, ascending, non-reused
        TransportEndpoint endpoint{};                         // transport-owned handle (stored, not owned)
        ConnectionState state = ConnectionState::Disconnected;
        HandshakeState handshake = HandshakeState::None;
        DisconnectReason lastReason = DisconnectReason::None;

        // Timing slots (populated by later steps; tick-derived, never wall-clock).
        std::uint64_t createdTick = 0;
        std::uint64_t lastRecvTick = 0;
        std::uint64_t lastSendTick = 0;
        std::uint64_t establishedTick = 0;

        // Sequence-number state (used by Step 9 reliability).
        std::uint16_t nextSequence = 0;
        std::uint16_t remoteSequence = 0;

        // Reserved (null in Sprint-006). playerSlot -> Sprint-007; reconnectToken -> Sprint-012.
        std::uint32_t playerSlot = 0;
        std::uint64_t reconnectToken = 0;

        // Step 9: indices/handles into Module-owned stores (populated by Step 11
        // wiring). Handles, NOT pointers, so table growth/removal never dangles.
        // 0 = unassigned.
        std::uint32_t queuesHandle = 0;
        std::uint32_t reliabilityHandle = 0;
        std::uint32_t reassemblyHandle = 0;
    };
} // namespace stalkermp::net
