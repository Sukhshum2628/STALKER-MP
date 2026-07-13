// STALKER-MP — Wire packet header (Sprint-006)
//
// Step 1: the fixed wire-header VALUE SHAPE (ADR-010) — field layout, widths,
//   authoritative wire size, and flag bit constants. No serialization here.
// Step 2: the field-by-field little-endian codec (SerializeHeader /
//   DeserializeHeader / ValidateHeader) + HeaderError vocabulary.
//
// ADR-010: the on-wire form is exactly kPacketHeaderWireSize contiguous bytes,
// little-endian, serialized FIELD BY FIELD. sizeof(PacketHeader) (which may be
// padded) is NEVER used for framing and the struct is NEVER memcpy'd/reinterpreted
// to/from the wire. ADR-007: no exceptions; codec returns core::Expected<T>.
// Engine-free and OS-free.

#pragma once

#include <cstddef>
#include <cstdint>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ByteCursor.h" // net::ByteWriter, net::ByteReader (Step 2)
#include "stalkermp/net/NetTypes.h"   // net::Channel, net::MessageId

namespace stalkermp::net
{
    // Fixed wire header (field order is the wire order; ADR-010).
    struct PacketHeader
    {
        std::uint16_t magic = 0;           // == kProtocolMagic
        std::uint16_t protocolVersion = 0; // == kProtocolVersion
        std::uint8_t channel = 0;          // Channel underlying value
        std::uint8_t flags = 0;            // bit set below
        std::uint16_t sequence = 0;
        std::uint16_t ack = 0;
        std::uint32_t ackBits = 0;
        std::uint16_t messageId = 0;       // MessageId.value
        std::uint16_t payloadLength = 0;
    };

    // AUTHORITATIVE on-wire byte count (2+2+1+1+2+2+4+2+2). Framing uses this,
    // never sizeof(PacketHeader).
    inline constexpr std::size_t kPacketHeaderWireSize = 18;

    // Defined flag bits. Any bit outside this set is a reserved-flag violation.
    inline constexpr std::uint8_t kFlagFragment = 0x01;
    inline constexpr std::uint8_t kFlagAck = 0x02;
    inline constexpr std::uint8_t kFlagControl = 0x04;
    inline constexpr std::uint8_t kDefinedFlagsMask =
        static_cast<std::uint8_t>(kFlagFragment | kFlagAck | kFlagControl);

    // ---- Step 2: codec + validation ---------------------------------------

    enum class HeaderError : std::uint8_t
    {
        None = 0,
        Truncated,        // fewer than kPacketHeaderWireSize bytes available
        BadMagic,         // magic != kProtocolMagic
        VersionMismatch,  // protocolVersion != kProtocolVersion
        BadChannel,       // channel not in {Reliable, Unreliable}
        ReservedFlag,     // a bit set outside kDefinedFlagsMask
        BadLength,        // payloadLength exceeds the available/allowed bound
    };

    [[nodiscard]] constexpr const char* HeaderErrorName(const HeaderError e) noexcept
    {
        switch (e)
        {
        case HeaderError::None:            return "None";
        case HeaderError::Truncated:       return "Truncated";
        case HeaderError::BadMagic:        return "BadMagic";
        case HeaderError::VersionMismatch: return "VersionMismatch";
        case HeaderError::BadChannel:      return "BadChannel";
        case HeaderError::ReservedFlag:    return "ReservedFlag";
        case HeaderError::BadLength:       return "BadLength";
        }
        return "Unknown";
    }

    // Serialize the header field-by-field, little-endian, into the writer.
    // Writes exactly kPacketHeaderWireSize bytes on success; returns an error
    // (and leaves no "valid" partial header) if the writer lacks space.
    [[nodiscard]] core::Expected<void> SerializeHeader(const PacketHeader& header, ByteWriter& writer);

    // Extract a header field-by-field from the reader (advances by exactly
    // kPacketHeaderWireSize on success), then apply semantic validation with no
    // payload context (magic → version → channel → reserved flags). Truncation
    // (underflow) surfaces as HeaderError::Truncated.
    [[nodiscard]] core::Expected<PacketHeader> DeserializeHeader(ByteReader& reader);

    // Validate an already-extracted header. When availablePayloadBytes is given,
    // also enforces payloadLength <= availablePayloadBytes. Ordering:
    // magic → version → channel → reserved flags → length.
    [[nodiscard]] core::Expected<void>
    ValidateHeader(const PacketHeader& header, std::size_t availablePayloadBytes);
} // namespace stalkermp::net
