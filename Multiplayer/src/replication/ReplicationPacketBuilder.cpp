// STALKER-MP — Replication packet assembly (Sprint-009, Step 9)
//
// See ReplicationPacketBuilder.h. Deterministic, fixed little-endian framing via
// net::ByteWriter under the additive id net::kMsgReplicationUpdate; budget
// enforced; no partial packet. Engine-free / OS-free.

#include "stalkermp/replication/ReplicationPacketBuilder.h"

#include <cstring> // std::memcpy

#include "stalkermp/common/StringUtil.h" // common::Format
#include "stalkermp/net/ByteCursor.h"    // net::ByteWriter

namespace stalkermp::replication
{
    namespace
    {
        [[nodiscard]] core::Expected<void> WriteU64(net::ByteWriter& w, const std::uint64_t value)
        {
            if (auto r = w.WriteU32(static_cast<std::uint32_t>(value & 0xFFFFFFFFu)); !r)
            {
                return r;
            }
            return w.WriteU32(static_cast<std::uint32_t>((value >> 32) & 0xFFFFFFFFu));
        }

        [[nodiscard]] core::Expected<void> WriteFloat(net::ByteWriter& w, const float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits)); // deterministic bit pattern
            return w.WriteU32(bits);
        }

        // Deterministic FNV-1a over the assembled header+records bytes.
        [[nodiscard]] std::uint32_t Checksum(const std::uint8_t* data, const std::size_t length) noexcept
        {
            std::uint32_t hash = 2166136261u;
            for (std::size_t i = 0; i < length; ++i)
            {
                hash ^= data[i];
                hash *= 16777619u;
            }
            return hash;
        }
    } // namespace

    core::Expected<ReplicationPacket> ReplicationPacketBuilder::BuildPacket(FixedRecordQueue& queue,
                                                                           const ReplicationReliability reliability,
                                                                           const ClientId client,
                                                                           const std::uint64_t sourceSnapshotTick)
    {
        // Determine how many records fit (deterministic; records are fixed size).
        std::size_t budgetRecords = m_maxRecords;
        if (m_byteBudget != 0)
        {
            if (m_byteBudget < kHeaderBytes + kChecksumBytes)
            {
                return core::MakeError(
                    core::ErrorCode::InvalidArgument,
                    common::Format("ReplicationPacketBuilder: budget {} < frame {} (Overflow)", m_byteBudget,
                                   kHeaderBytes + kChecksumBytes));
            }
            budgetRecords = (m_byteBudget - kHeaderBytes - kChecksumBytes) / kRecordBytes;
        }

        std::size_t fit = queue.Size();
        if (fit > budgetRecords)
        {
            fit = budgetRecords;
        }
        if (fit > m_maxRecords)
        {
            fit = m_maxRecords;
        }
        if (fit > 0xFFFFu) // recordCount is u16 on the wire
        {
            fit = 0xFFFFu;
        }

        ReplicationPacket packet;
        packet.messageId = net::kMsgReplicationUpdate;
        packet.client = client;
        packet.reliability = reliability;
        packet.recordCount = static_cast<std::uint16_t>(fit);
        packet.bytes.resize(kHeaderBytes + fit * kRecordBytes + kChecksumBytes);

        net::ByteWriter writer(packet.bytes.data(), packet.bytes.size());

        // --- Header (fixed little-endian) ------------------------------------
        if (auto r = writer.WriteU16(packet.messageId.value); !r) return r.GetError();
        if (auto r = WriteU64(writer, client.value); !r) return r.GetError();
        if (auto r = WriteU64(writer, sourceSnapshotTick); !r) return r.GetError();
        if (auto r = writer.WriteU8(static_cast<std::uint8_t>(reliability)); !r) return r.GetError();
        if (auto r = writer.WriteU16(packet.recordCount); !r) return r.GetError();

        // --- Records (FIFO, fully written or not at all) ----------------------
        for (std::size_t i = 0; i < fit; ++i)
        {
            const auto record = queue.Dequeue();
            if (!record.has_value())
            {
                break; // queue drained (defensive; fit <= Size)
            }
            const EntityReplicationState& e = record->entity;
            if (auto r = writer.WriteU8(static_cast<std::uint8_t>(record->kind)); !r) return r.GetError();
            if (auto r = writer.WriteU32(e.id.value); !r) return r.GetError();
            if (auto r = writer.WriteU32(e.version); !r) return r.GetError();
            if (auto r = writer.WriteU32(e.stateFlags); !r) return r.GetError();
            if (auto r = WriteFloat(writer, e.position.x); !r) return r.GetError();
            if (auto r = WriteFloat(writer, e.position.y); !r) return r.GetError();
            if (auto r = WriteFloat(writer, e.position.z); !r) return r.GetError();
            if (auto r = writer.WriteU8(static_cast<std::uint8_t>(record->priority)); !r) return r.GetError();
        }

        // --- Checksum over the header + records ------------------------------
        const std::uint32_t checksum = Checksum(packet.bytes.data(), writer.BytesWritten());
        if (auto r = writer.WriteU32(checksum); !r) return r.GetError();

        return packet;
    }
} // namespace stalkermp::replication
