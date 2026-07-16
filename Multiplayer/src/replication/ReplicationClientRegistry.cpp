// STALKER-MP — Replication client registry (Sprint-009, Step 4)
//
// See ReplicationClientRegistry.h. Ascending non-reused ClientId allocation;
// capacity-bounded; monotonic acks; deterministic ascending order. Value
// outcomes (ADR-007). Engine-free / OS-free.

#include "stalkermp/replication/ReplicationClientRegistry.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::replication
{
    std::size_t ReplicationClientRegistry::IndexOf(const ClientId id) const noexcept
    {
        for (std::size_t i = 0; i < m_records.size(); ++i)
        {
            if (m_records[i].id == id)
            {
                return i;
            }
        }
        return m_records.size();
    }

    core::Expected<ClientId> ReplicationClientRegistry::Register(const net::ConnectionId connection,
                                                                const world::PlayerPosition& focus)
    {
        if (m_records.size() >= m_maxClients)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("ReplicationClientRegistry: at capacity ({})", m_maxClients));
        }
        if (FindByConnection(connection) != nullptr)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("ReplicationClientRegistry: connection {} already registered", connection.value));
        }

        ClientRecord record{};
        record.id = ClientId{m_nextClientId++}; // ascending, never reused
        record.connection = connection;
        record.focus = focus;
        record.active = true;
        // Ids are allocated strictly ascending and appended, so the vector stays
        // sorted ascending by ClientId.
        m_records.push_back(record);
        return record.id;
    }

    core::Expected<void> ReplicationClientRegistry::Unregister(const ClientId id)
    {
        const std::size_t idx = IndexOf(id);
        if (idx >= m_records.size())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("ReplicationClientRegistry: unknown client {}", id.value));
        }
        m_records.erase(m_records.begin() + static_cast<std::ptrdiff_t>(idx)); // order preserved
        return core::Success();
    }

    const ClientRecord* ReplicationClientRegistry::FindById(const ClientId id) const noexcept
    {
        const std::size_t idx = IndexOf(id);
        return idx < m_records.size() ? &m_records[idx] : nullptr;
    }

    const ClientRecord* ReplicationClientRegistry::FindByConnection(const net::ConnectionId connection) const noexcept
    {
        for (const ClientRecord& r : m_records)
        {
            if (r.connection == connection)
            {
                return &r;
            }
        }
        return nullptr;
    }

    core::Expected<void> ReplicationClientRegistry::UpdateFocus(const ClientId id, const world::PlayerPosition& focus)
    {
        const std::size_t idx = IndexOf(id);
        if (idx >= m_records.size())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("ReplicationClientRegistry: unknown client {}", id.value));
        }
        m_records[idx].focus = focus;
        return core::Success();
    }

    core::Expected<void> ReplicationClientRegistry::RecordAck(const ClientId id,
                                                             const std::uint64_t ackedReplicationId,
                                                             const std::uint64_t ackedSnapshotTick)
    {
        const std::size_t idx = IndexOf(id);
        if (idx >= m_records.size())
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("ReplicationClientRegistry: unknown client {}", id.value));
        }
        // Monotonic: only advance the baseline; older/equal acks are ignored.
        if (ackedReplicationId > m_records[idx].lastAckedReplicationId)
        {
            m_records[idx].lastAckedReplicationId = ackedReplicationId;
            m_records[idx].lastAckedSnapshotTick = ackedSnapshotTick;
        }
        return core::Success();
    }

    std::vector<ClientRecord> ReplicationClientRegistry::ActiveClients() const
    {
        std::vector<ClientRecord> out;
        out.reserve(m_records.size());
        for (const ClientRecord& r : m_records) // already ascending by ClientId
        {
            if (r.active)
            {
                out.push_back(r);
            }
        }
        return out;
    }

    bool ReplicationClientRegistry::ValidateConsistency() const noexcept
    {
        std::uint64_t previous = 0;
        for (const ClientRecord& r : m_records)
        {
            if (r.id.value == 0)
            {
                return false; // no none-id record
            }
            if (r.id.value <= previous)
            {
                return false; // must be strictly ascending + unique
            }
            previous = r.id.value;
        }
        return m_records.size() <= m_maxClients;
    }
} // namespace stalkermp::replication
