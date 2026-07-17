// STALKER-MP — Replication packet assembly (Sprint-009, Step 9)
//
// Serializes queued replication records (Step 8) into a fixed, deterministic,
// little-endian wire payload under the additive DATA-range id
// net::kMsgReplicationUpdate (0x0200) — ADR-010 respected, never renumbered.
// Byte layout is stable and reproducible: identical input yields identical bytes.
// Budget is enforced from ReplicationConfiguration (bandwidth + per-update caps);
// overflow is a value outcome; a packet is never left partially written (records
// that do not fit remain in the queue).
//
// This step introduces packet assembly ONLY — no transport, socket I/O, worker,
// threading, or tick/service. Consumes Step-08 queues, Step-06/Step-01 value
// types, and the Sprint-006 net::ByteWriter framing. Writes into an in-memory
// byte buffer; it never sends.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/NetTypes.h"                    // net::MessageId
#include "stalkermp/net/ReplicationMessageIds.h"       // kMsgReplicationUpdate
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationQueues.h"   // FixedRecordQueue, QueuedRecord
#include "stalkermp/replication/ReplicationTypes.h"

namespace stalkermp::replication
{
    // An assembled replication packet: the wire bytes plus their summary. Values
    // only; the bytes are a complete, self-consistent frame (header + records +
    // checksum) — never a partial write.
    struct ReplicationPacket
    {
        net::MessageId messageId{net::kMsgReplicationUpdate};
        ClientId client{};
        ReplicationReliability reliability = ReplicationReliability::Unreliable;
        std::uint16_t recordCount = 0;
        std::vector<std::uint8_t> bytes; // the little-endian wire payload

        [[nodiscard]] std::size_t PacketSize() const noexcept { return bytes.size(); }
        [[nodiscard]] bool Empty() const noexcept { return recordCount == 0; }
    };

    class ReplicationPacketBuilder
    {
    public:
        // Fixed wire sizes (bytes). Stable ABI (ADR-010): fields are never
        // reordered; new fields are appended in a future versioned layout only.
        static constexpr std::size_t kHeaderBytes = 21;   // msgId(2)+client(8)+tick(8)+reliability(1)+count(2)
        static constexpr std::size_t kRecordBytes = 26;   // kind(1)+id(4)+version(4)+flags(4)+pos(12)+priority(1)
        static constexpr std::size_t kChecksumBytes = 4;  // u32 checksum

        explicit ReplicationPacketBuilder(const ReplicationConfiguration& config) noexcept
            : m_byteBudget(config.bandwidthBudgetBytesPerTick), m_maxRecords(config.maxEntitiesPerUpdate)
        {
        }

        // Drain up to the budget/cap of records (FIFO) from `queue` into one
        // deterministic packet for `client` at `sourceSnapshotTick`. Records that
        // do not fit remain in the queue. Returns Overflow (value outcome) when the
        // configured byte budget cannot fit even an empty frame (header+checksum).
        [[nodiscard]] core::Expected<ReplicationPacket>
        BuildPacket(FixedRecordQueue& queue, ReplicationReliability reliability, ClientId client,
                    std::uint64_t sourceSnapshotTick);

    private:
        std::uint32_t m_byteBudget; // 0 = advisory/unbounded (cap by maxRecords)
        std::uint32_t m_maxRecords;
    };
} // namespace stalkermp::replication
