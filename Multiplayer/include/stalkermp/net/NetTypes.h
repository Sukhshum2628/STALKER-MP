// STALKER-MP — Networking foundation value types (Sprint-006, Step 1)
//
// Engine-free AND OS-free identifier value types and enums for the networking
// subsystem. This file introduces types only — no transport, no sockets, no
// services, no logic, no runtime behavior.
//
// ADR-007: no engine/OS headers, no exceptions, no RTTI, no iostream; pure value
// types, trivially C4530-clean. ADR-009: TransportEndpoint is an OPAQUE integer
// handle, never an OS socket/address type. ADR-010: Channel's numeric values are
// wire-visible and therefore stable. ADR-011: no threads.

#pragma once

#include <cstdint>

namespace stalkermp::net
{
    // Session-unique, host-assigned link handle. Value 0 = "none". Never
    // wire-trusted (the host assigns it; peers cannot claim one). Never reused
    // within a session. Ordered by value (ascending determinism).
    struct ConnectionId
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsNone() const noexcept { return value == 0; }
        [[nodiscard]] constexpr bool operator==(const ConnectionId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const ConnectionId& o) const noexcept { return value != o.value; }
    };

    // Wire message discriminator (carried in PacketHeader.messageId). Control vs.
    // data range predicates live in ProtocolConstants.h (they need the range
    // constants). Value 0 is within the control range.
    struct MessageId
    {
        std::uint16_t value = 0;

        [[nodiscard]] constexpr bool operator==(const MessageId& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const MessageId& o) const noexcept { return value != o.value; }
    };

    // Opaque transport-level peer handle (ADR-009). NOT an OS socket/address — a
    // handle/index owned by the (future) ITransport. Value 0 = invalid. No OS
    // header may appear to define it; it is never dereferenced as a pointer.
    struct TransportEndpoint
    {
        std::uint32_t value = 0;

        [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
        [[nodiscard]] constexpr bool operator==(const TransportEndpoint& o) const noexcept { return value == o.value; }
        [[nodiscard]] constexpr bool operator!=(const TransportEndpoint& o) const noexcept { return value != o.value; }
    };

    // Logical delivery channel over the single transport. Numeric values are
    // wire-visible (ADR-010) and therefore stable. Reliable = 0 (safe default).
    enum class Channel : std::uint8_t
    {
        Reliable = 0,
        Unreliable = 1,
    };

    [[nodiscard]] constexpr const char* ChannelName(const Channel c) noexcept
    {
        switch (c)
        {
        case Channel::Reliable:   return "Reliable";
        case Channel::Unreliable: return "Unreliable";
        }
        return "Unknown";
    }

    // Per-connection handshake progress. State-machine logic is a later step;
    // this defines the states only. None = 0 (pre-handshake default).
    enum class HandshakeState : std::uint8_t
    {
        None = 0,
        HelloSent,
        ChallengeSent,
        ResponseSent,
        Established,
    };

    [[nodiscard]] constexpr const char* HandshakeStateName(const HandshakeState s) noexcept
    {
        switch (s)
        {
        case HandshakeState::None:          return "None";
        case HandshakeState::HelloSent:     return "HelloSent";
        case HandshakeState::ChallengeSent: return "ChallengeSent";
        case HandshakeState::ResponseSent:  return "ResponseSent";
        case HandshakeState::Established:   return "Established";
        }
        return "Unknown";
    }

    // Connection lifecycle phase. Disconnected = 0 (default).
    enum class ConnectionState : std::uint8_t
    {
        Disconnected = 0,
        Connecting,
        Handshaking,
        Connected,
        Disconnecting,
    };

    [[nodiscard]] constexpr const char* ConnectionStateName(const ConnectionState s) noexcept
    {
        switch (s)
        {
        case ConnectionState::Disconnected:  return "Disconnected";
        case ConnectionState::Connecting:    return "Connecting";
        case ConnectionState::Handshaking:   return "Handshaking";
        case ConnectionState::Connected:     return "Connected";
        case ConnectionState::Disconnecting: return "Disconnecting";
        }
        return "Unknown";
    }

    // Cause classification for the unified disconnect path (a later step). None = 0.
    enum class DisconnectReason : std::uint8_t
    {
        None = 0,
        Timeout,
        Graceful,
        Rejected,
        ProtocolError,
        CapacityFull,
        TransportError,
    };

    [[nodiscard]] constexpr const char* DisconnectReasonName(const DisconnectReason r) noexcept
    {
        switch (r)
        {
        case DisconnectReason::None:           return "None";
        case DisconnectReason::Timeout:        return "Timeout";
        case DisconnectReason::Graceful:       return "Graceful";
        case DisconnectReason::Rejected:       return "Rejected";
        case DisconnectReason::ProtocolError:  return "ProtocolError";
        case DisconnectReason::CapacityFull:   return "CapacityFull";
        case DisconnectReason::TransportError: return "TransportError";
        }
        return "Unknown";
    }

    // Value result of a (future) transport operation — keeps the transport seam
    // exception-free (ADR-007/009). Ok = 0.
    enum class TransportOutcome : std::uint8_t
    {
        Ok = 0,
        WouldBlock,
        Queued,
        Rejected,
        EndpointMissing,
        Error,
    };

    [[nodiscard]] constexpr const char* TransportOutcomeName(const TransportOutcome o) noexcept
    {
        switch (o)
        {
        case TransportOutcome::Ok:              return "Ok";
        case TransportOutcome::WouldBlock:      return "WouldBlock";
        case TransportOutcome::Queued:          return "Queued";
        case TransportOutcome::Rejected:        return "Rejected";
        case TransportOutcome::EndpointMissing: return "EndpointMissing";
        case TransportOutcome::Error:           return "Error";
        }
        return "Unknown";
    }
} // namespace stalkermp::net
