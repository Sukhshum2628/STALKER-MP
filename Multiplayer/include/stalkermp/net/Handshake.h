// STALKER-MP — Handshake state machine (Sprint-006, Step 6)
//
// Deterministic, single-step-per-tick, HOST-side handshake over control messages:
//   Hello -> (Challenge out) -> Response -> (Established out).
// The driver is stateless: it operates on the Connection by reference (handshake
// state lives on the Connection, Step 5) and derives the server nonce
// deterministically from the ConnectionId (no per-connection storage, no new
// Connection field). NO transport internals, timeout (Step 7), or session (Step 8).
//
// ADR-007 (no exceptions; value results). ADR-010 (control ids; version exact-
// match; reject-before-allocate). Engine-free / OS-free.

#pragma once

#include <cstdint>
#include <optional>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ByteCursor.h"
#include "stalkermp/net/Connection.h"
#include "stalkermp/net/IConnectionAuthenticator.h"
#include "stalkermp/net/NetTypes.h"

namespace stalkermp::net
{
    // A parsed incoming handshake control message. `id` is the packet header's
    // messageId; `version` is meaningful for Hello; `nonce` is the client nonce
    // (Hello) or the echoed server nonce (Response).
    struct HandshakeMessage
    {
        MessageId id{};
        std::uint16_t version = 0;
        std::uint32_t nonce = 0;
    };

    // An outbound control message the host should send (Challenge carries the
    // server nonce; Established carries none).
    struct OutboundControl
    {
        MessageId id{};
        std::uint32_t nonce = 0;
    };

    // Result of one Advance: the resulting handshake state, an optional outbound
    // message to send, and an optional drop reason (Step 7 performs the drop).
    struct HandshakeResult
    {
        HandshakeState state = HandshakeState::None;
        std::optional<OutboundControl> outbound;
        std::optional<DisconnectReason> drop;
        bool advanced = false;
    };

    // True once the handshake has reached Established. Surfaced here so the
    // timeout scan (Step 7) can tell which connections are still handshaking
    // (subject to the handshake-timeout budget) without hard-coding the state.
    [[nodiscard]] constexpr bool IsHandshakeComplete(const HandshakeState state) noexcept
    {
        return state == HandshakeState::Established;
    }

    // Deterministic server nonce for a connection (never 0). Recomputable at
    // Challenge and Response time, so no nonce needs to be stored.
    [[nodiscard]] constexpr std::uint32_t ServerNonce(const ConnectionId id) noexcept
    {
        std::uint32_t x = id.value * 2654435761u; // Knuth multiplicative hash
        x ^= 0x5350AA55u;
        x = (x << 13) | (x >> 19); // rotate
        return x == 0 ? 1u : x;
    }

    // Payload codec for handshake messages (version + nonce), so messages can be
    // round-tripped through a transport. `id` comes from the packet header.
    [[nodiscard]] core::Expected<void> SerializeHandshake(const HandshakeMessage& msg, ByteWriter& writer);
    [[nodiscard]] core::Expected<HandshakeMessage> DeserializeHandshake(MessageId id, ByteReader& reader);

    class Handshake
    {
    public:
        explicit Handshake(IConnectionAuthenticator& authenticator) : m_authenticator(&authenticator) {}

        // Advance the connection's handshake by AT MOST one state, consuming AT
        // MOST one incoming handshake message. No incoming message -> no change.
        // On a state advance, connection.handshake is updated. On an illegal
        // transition, `drop` is set (ProtocolError) / on deny (Rejected) and the
        // state is left unchanged. `tick` is reserved for later-step host use.
        [[nodiscard]] HandshakeResult Advance(Connection& connection,
                                              const std::optional<HandshakeMessage>& incoming,
                                              std::uint64_t tick);

    private:
        IConnectionAuthenticator* m_authenticator; // non-owning
    };
} // namespace stalkermp::net
