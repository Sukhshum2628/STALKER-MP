// STALKER-MP — Wire packet header codec (Sprint-006, Step 2)
//
// Field-by-field, little-endian serialize/deserialize/validate for the fixed
// wire header (ADR-010). No memcpy/reinterpret of PacketHeader; no packing
// assumption; kPacketHeaderWireSize drives framing. ADR-007: no exceptions,
// core::Expected. Engine-free and OS-free.

#include "stalkermp/net/PacketHeader.h"

#include "stalkermp/common/StringUtil.h" // common::Format (ADR-007-safe)
#include "stalkermp/net/NetTypes.h"
#include "stalkermp/net/ProtocolConstants.h"

namespace stalkermp::net
{
    namespace
    {
        [[nodiscard]] core::Error HeaderErr(const HeaderError code)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("packet header: {}", HeaderErrorName(code)));
        }

        // Semantic checks with a fixed order: magic → version → channel →
        // reserved flags. Length is checked separately (needs payload context).
        [[nodiscard]] core::Expected<void> ValidateSemantics(const PacketHeader& h)
        {
            if (h.magic != kProtocolMagic)
            {
                return HeaderErr(HeaderError::BadMagic);
            }
            if (h.protocolVersion != kProtocolVersion)
            {
                return HeaderErr(HeaderError::VersionMismatch);
            }
            if (h.channel != static_cast<std::uint8_t>(Channel::Reliable) &&
                h.channel != static_cast<std::uint8_t>(Channel::Unreliable))
            {
                return HeaderErr(HeaderError::BadChannel);
            }
            if ((h.flags & static_cast<std::uint8_t>(~kDefinedFlagsMask)) != 0)
            {
                return HeaderErr(HeaderError::ReservedFlag);
            }
            return core::Success();
        }
    } // namespace

    core::Expected<void> SerializeHeader(const PacketHeader& header, ByteWriter& writer)
    {
        if (auto r = writer.WriteU16(header.magic); !r) { return r; }
        if (auto r = writer.WriteU16(header.protocolVersion); !r) { return r; }
        if (auto r = writer.WriteU8(header.channel); !r) { return r; }
        if (auto r = writer.WriteU8(header.flags); !r) { return r; }
        if (auto r = writer.WriteU16(header.sequence); !r) { return r; }
        if (auto r = writer.WriteU16(header.ack); !r) { return r; }
        if (auto r = writer.WriteU32(header.ackBits); !r) { return r; }
        if (auto r = writer.WriteU16(header.messageId); !r) { return r; }
        if (auto r = writer.WriteU16(header.payloadLength); !r) { return r; }
        return core::Success();
    }

    core::Expected<PacketHeader> DeserializeHeader(ByteReader& reader)
    {
        PacketHeader header;

        // Underflow at any field means fewer than kPacketHeaderWireSize bytes were
        // available — report it as Truncated (the reader's own error is discarded
        // in favor of the header-level classification).
        auto magic = reader.ReadU16();
        if (!magic) { return HeaderErr(HeaderError::Truncated); }
        header.magic = magic.Value();

        auto version = reader.ReadU16();
        if (!version) { return HeaderErr(HeaderError::Truncated); }
        header.protocolVersion = version.Value();

        auto channel = reader.ReadU8();
        if (!channel) { return HeaderErr(HeaderError::Truncated); }
        header.channel = channel.Value();

        auto flags = reader.ReadU8();
        if (!flags) { return HeaderErr(HeaderError::Truncated); }
        header.flags = flags.Value();

        auto sequence = reader.ReadU16();
        if (!sequence) { return HeaderErr(HeaderError::Truncated); }
        header.sequence = sequence.Value();

        auto ack = reader.ReadU16();
        if (!ack) { return HeaderErr(HeaderError::Truncated); }
        header.ack = ack.Value();

        auto ackBits = reader.ReadU32();
        if (!ackBits) { return HeaderErr(HeaderError::Truncated); }
        header.ackBits = ackBits.Value();

        auto messageId = reader.ReadU16();
        if (!messageId) { return HeaderErr(HeaderError::Truncated); }
        header.messageId = messageId.Value();

        auto payloadLength = reader.ReadU16();
        if (!payloadLength) { return HeaderErr(HeaderError::Truncated); }
        header.payloadLength = payloadLength.Value();

        // Semantic validation (no payload context here): magic → version →
        // channel → reserved flags.
        if (auto v = ValidateSemantics(header); !v)
        {
            return v.GetError();
        }
        return header;
    }

    core::Expected<void> ValidateHeader(const PacketHeader& header, const std::size_t availablePayloadBytes)
    {
        if (auto v = ValidateSemantics(header); !v)
        {
            return v;
        }
        if (header.payloadLength > availablePayloadBytes)
        {
            return HeaderErr(HeaderError::BadLength);
        }
        return core::Success();
    }
} // namespace stalkermp::net
