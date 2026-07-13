// STALKER-MP — Handshake state machine (Sprint-006, Step 6)
//
// See Handshake.h. Engine-free / OS-free; no exceptions, value results.
// Single-step-per-tick, host-side; ADR-010 (control ids, version exact-match).

#include "stalkermp/net/Handshake.h"

#include "stalkermp/net/ProtocolConstants.h"

namespace stalkermp::net
{
    namespace
    {
        [[nodiscard]] bool IsHandshakeId(const MessageId id) noexcept
        {
            return id == kMsgHello || id == kMsgChallenge || id == kMsgResponse || id == kMsgEstablished;
        }

        // A non-handshake control id (Ping/Pong/Bye, ...) received mid-handshake is
        // ignored (no advance, no drop). A handshake id out of order — or a
        // non-control (data) id — is a ProtocolError.
        [[nodiscard]] bool IsIgnorableDuringHandshake(const MessageId id) noexcept
        {
            return IsControlId(id) && !IsHandshakeId(id);
        }

        [[nodiscard]] HandshakeResult NoChange(const Connection& c)
        {
            HandshakeResult r;
            r.state = c.handshake;
            r.advanced = false;
            return r;
        }

        [[nodiscard]] HandshakeResult Drop(const Connection& c, const DisconnectReason reason)
        {
            HandshakeResult r;
            r.state = c.handshake; // unchanged
            r.drop = reason;
            r.advanced = false;
            return r;
        }
    } // namespace

    core::Expected<void> SerializeHandshake(const HandshakeMessage& msg, ByteWriter& writer)
    {
        if (auto r = writer.WriteU16(msg.version); !r) { return r; }
        if (auto r = writer.WriteU32(msg.nonce); !r) { return r; }
        return core::Success();
    }

    core::Expected<HandshakeMessage> DeserializeHandshake(const MessageId id, ByteReader& reader)
    {
        HandshakeMessage msg;
        msg.id = id;
        auto version = reader.ReadU16();
        if (!version) { return version.GetError(); }
        msg.version = version.Value();
        auto nonce = reader.ReadU32();
        if (!nonce) { return nonce.GetError(); }
        msg.nonce = nonce.Value();
        return msg;
    }

    HandshakeResult Handshake::Advance(Connection& connection,
                                       const std::optional<HandshakeMessage>& incoming,
                                       [[maybe_unused]] const std::uint64_t tick)
    {
        if (!incoming.has_value())
        {
            return NoChange(connection); // no message this tick -> no state change
        }
        const HandshakeMessage& m = *incoming;

        switch (connection.handshake)
        {
        case HandshakeState::None:
        {
            if (m.id == kMsgHello)
            {
                // Version checked before any allocation (ADR-010 exact-match).
                if (m.version != kProtocolVersion)
                {
                    return Drop(connection, DisconnectReason::ProtocolError);
                }
                HandshakeResult r;
                r.outbound = OutboundControl{kMsgChallenge, ServerNonce(connection.id)};
                r.state = HandshakeState::ChallengeSent;
                r.advanced = true;
                connection.handshake = r.state;
                return r;
            }
            if (IsIgnorableDuringHandshake(m.id))
            {
                return NoChange(connection);
            }
            return Drop(connection, DisconnectReason::ProtocolError);
        }
        case HandshakeState::ChallengeSent:
        {
            if (m.id == kMsgResponse)
            {
                if (m.nonce != ServerNonce(connection.id))
                {
                    return Drop(connection, DisconnectReason::ProtocolError);
                }
                AuthPayload payload;
                payload.nonce = m.nonce;
                if (m_authenticator->Authorize(connection, payload) == AuthDecision::Deny)
                {
                    return Drop(connection, DisconnectReason::Rejected);
                }
                HandshakeResult r;
                r.outbound = OutboundControl{kMsgEstablished, 0};
                r.state = HandshakeState::Established;
                r.advanced = true;
                connection.handshake = r.state;
                return r;
            }
            if (IsIgnorableDuringHandshake(m.id))
            {
                return NoChange(connection);
            }
            return Drop(connection, DisconnectReason::ProtocolError);
        }
        case HandshakeState::Established:
        case HandshakeState::HelloSent:   // client-side states; host is never in them
        case HandshakeState::ResponseSent:
        {
            if (IsIgnorableDuringHandshake(m.id))
            {
                return NoChange(connection);
            }
            return Drop(connection, DisconnectReason::ProtocolError);
        }
        }
        return Drop(connection, DisconnectReason::ProtocolError);
    }
} // namespace stalkermp::net
