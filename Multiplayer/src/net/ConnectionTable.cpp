// STALKER-MP — Connection table (Sprint-006, Step 5)
//
// See ConnectionTable.h. Engine-free / OS-free; no exceptions, core::Expected.
// Ascending non-reused ids; sorted vector + hash accelerator (BC-005).

#include "stalkermp/net/ConnectionTable.h"

#include "stalkermp/common/StringUtil.h" // common::Format
#include "stalkermp/net/Handshake.h"     // IsHandshakeComplete (handshake-timeout surface)

namespace stalkermp::net
{
    core::Expected<ConnectionId> ConnectionTable::Add(const TransportEndpoint endpoint)
    {
        if (m_entries.size() >= m_maxConnections)
        {
            // No dedicated capacity ErrorCode exists; the call site maps this to
            // DisconnectReason::CapacityFull. The message carries the semantics.
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("connection table at capacity ({})", m_maxConnections));
        }

        const ConnectionId id{m_nextId};
        ++m_nextId; // monotonic; never reused within a session (0 never assigned)

        Connection c;
        c.id = id;
        c.endpoint = endpoint;

        // Ids are strictly increasing, so the new id always appends at the end and
        // the canonical vector stays ascending without a full re-sort.
        m_entries.push_back(c);
        m_index.emplace(id.value, m_entries.size() - 1);
        return id;
    }

    const Connection* ConnectionTable::Find(const ConnectionId id) const
    {
        const auto it = m_index.find(id.value);
        if (it == m_index.end())
        {
            return nullptr;
        }
        return &m_entries[it->second];
    }

    Connection* ConnectionTable::FindMutable(const ConnectionId id)
    {
        const auto it = m_index.find(id.value);
        if (it == m_index.end())
        {
            return nullptr;
        }
        return &m_entries[it->second];
    }

    void ConnectionTable::Remove(const ConnectionId id)
    {
        const auto it = m_index.find(id.value);
        if (it == m_index.end())
        {
            return; // benign no-op
        }
        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(it->second));
        RebuildAccelerator(); // indices after the removed slot shifted
    }

    std::vector<ConnectionId> ConnectionTable::ScanTimeouts(const std::uint64_t currentTick,
                                                            const std::uint64_t handshakeTickBudget,
                                                            const std::uint64_t connectionTickBudget) const
    {
        std::vector<ConnectionId> flagged; // m_entries is ascending, so this is ascending too
        for (const Connection& c : m_entries)
        {
            if (!IsHandshakeComplete(c.handshake))
            {
                if (currentTick >= c.createdTick &&
                    (currentTick - c.createdTick) > handshakeTickBudget)
                {
                    flagged.push_back(c.id);
                }
            }
            else if (c.state == ConnectionState::Connected)
            {
                if (currentTick >= c.lastRecvTick &&
                    (currentTick - c.lastRecvTick) > connectionTickBudget)
                {
                    flagged.push_back(c.id);
                }
            }
        }
        return flagged;
    }

    DisconnectIntent ConnectionTable::BeginDisconnect(const ConnectionId id, const DisconnectReason reason)
    {
        Connection* const c = FindMutable(id);
        if (c == nullptr)
        {
            return DisconnectIntent{}; // absent / already removed -> no-op
        }

        DisconnectIntent intent;
        intent.performed = true;
        intent.id = id;
        intent.endpoint = c->endpoint; // captured for the best-effort Bye
        intent.reason = reason;

        c->state = ConnectionState::Disconnecting; // transient; record removed below
        c->lastReason = reason;

        Remove(id); // release + table removal (-> Disconnected)
        return intent;
    }

    std::vector<ConnectionId> ConnectionTable::Ids() const
    {
        std::vector<ConnectionId> ids;
        ids.reserve(m_entries.size());
        for (const Connection& c : m_entries)
        {
            ids.push_back(c.id);
        }
        return ids;
    }

    ConnectionTableReport ConnectionTable::ValidateConsistency() const
    {
        ConnectionTableReport report;

        for (std::size_t i = 0; i < m_entries.size(); ++i)
        {
            if (m_entries[i].id.value == 0)
            {
                report.noZeroId = false;
            }
            if (i > 0 && !(m_entries[i - 1].id.value < m_entries[i].id.value))
            {
                report.sortedUnique = false;
            }
        }

        if (m_index.size() != m_entries.size())
        {
            report.acceleratorConsistent = false;
        }
        else
        {
            for (std::size_t i = 0; i < m_entries.size(); ++i)
            {
                const auto it = m_index.find(m_entries[i].id.value);
                if (it == m_index.end() || it->second != i)
                {
                    report.acceleratorConsistent = false;
                    break;
                }
            }
        }

        if (m_entries.size() > m_maxConnections)
        {
            report.withinCapacity = false;
        }

        return report;
    }

    void ConnectionTable::RebuildAccelerator()
    {
        m_index.clear();
        m_index.reserve(m_entries.size());
        for (std::size_t i = 0; i < m_entries.size(); ++i)
        {
            m_index.emplace(m_entries[i].id.value, i);
        }
    }
} // namespace stalkermp::net
