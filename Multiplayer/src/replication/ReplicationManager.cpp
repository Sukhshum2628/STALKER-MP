// STALKER-MP — Replication manager (Sprint-009, Step 11)
//
// See ReplicationManager.h. One worker per tick at kReplicationPipeline = 450;
// consumes the latest immutable snapshot read-only; processes inbound acks
// (monotonic; stale/duplicate ignored). No transport, no thread, no engine.

#include "stalkermp/replication/ReplicationManager.h"

#include "stalkermp/net/ReplicationMessageIds.h" // net::kMsgReplicationAck
#include "stalkermp/snapshot/SimulationSnapshot.h"

namespace stalkermp::replication
{
    namespace
    {
        [[nodiscard]] core::Expected<std::uint64_t> ReadU64(net::ByteReader& reader)
        {
            const auto low = reader.ReadU32();
            if (!low)
            {
                return low.GetError();
            }
            const auto high = reader.ReadU32();
            if (!high)
            {
                return high.GetError();
            }
            return static_cast<std::uint64_t>(low.Value()) |
                   (static_cast<std::uint64_t>(high.Value()) << 32);
        }
    } // namespace

    core::Expected<void> ReplicationManager::Initialize()
    {
        // The worker's queues are pre-reserved from the configuration at
        // construction; the inbound-ack message-handler registration happens at
        // the Bootstrap-wiring step (Step 12). Nothing to reserve here.
        return core::Success();
    }

    void ReplicationManager::Shutdown() noexcept
    {
        m_worker.FlushQueues();
    }

    void ReplicationManager::Update()
    {
        ++m_tick; // monotonic, deterministic

        // Acquire the latest immutable published snapshot (read-only). No snapshot
        // published yet => skip this tick (previous state remains valid).
        const snapshot::SimulationSnapshot* const snap = m_snapshotSource.Acquire();
        if (snap == nullptr)
        {
            m_lastResult = ReplicationExecuteResult{}; // nothing produced this tick
            return;
        }

        // Exactly one worker pass per tick.
        auto result = m_worker.Execute(*snap);
        m_snapshotSource.Release(snap); // release our consumer reference

        if (result)
        {
            m_lastResult = std::move(result).Value(); // consume the assembled packets
        }
        else
        {
            m_lastResult = ReplicationExecuteResult{};
        }
    }

    core::Expected<void> ReplicationManager::HandleReplicationAck(const ReplicationAck& ack)
    {
        const ClientRecord* const record = m_clients.FindById(ack.client);
        if (record == nullptr)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "ReplicationManager: ack for unknown client");
        }

        // Ignore duplicate/stale acks (monotonic baseline).
        if (ack.ackedReplicationId <= record->lastAckedReplicationId)
        {
            ++m_acksIgnored;
            return core::Success();
        }

        if (auto applied = m_clients.RecordAck(ack.client, ack.ackedReplicationId, ack.ackedSnapshotTick); !applied)
        {
            return applied;
        }
        ++m_acksApplied;
        return core::Success();
    }

    core::Expected<void> ReplicationManager::HandleReplicationAck(net::ByteReader& payload)
    {
        // Wire layout: u16 messageId | u64 clientId | u64 ackedReplicationId | u64 ackedSnapshotTick.
        const auto messageId = payload.ReadU16();
        if (!messageId)
        {
            return messageId.GetError();
        }
        if (messageId.Value() != net::kMsgReplicationAck.value)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   "ReplicationManager: payload is not a ReplicationAck (0x0201)");
        }

        ReplicationAck ack{};
        if (auto client = ReadU64(payload); client)
        {
            ack.client = ClientId{client.Value()};
        }
        else
        {
            return client.GetError();
        }
        if (auto rid = ReadU64(payload); rid)
        {
            ack.ackedReplicationId = rid.Value();
        }
        else
        {
            return rid.GetError();
        }
        if (auto tick = ReadU64(payload); tick)
        {
            ack.ackedSnapshotTick = tick.Value();
        }
        else
        {
            return tick.GetError();
        }
        return HandleReplicationAck(ack);
    }
} // namespace stalkermp::replication
