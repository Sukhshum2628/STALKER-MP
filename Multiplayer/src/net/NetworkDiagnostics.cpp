// STALKER-MP — Network diagnostics (Sprint-006, Step 11)
//
// See NetworkDiagnostics.h. Read-only; iostream-free (common::Format). ADR-007.

#include "stalkermp/net/NetworkDiagnostics.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::net
{
    NetworkStatistics NetworkDiagnostics::Statistics() const
    {
        NetworkStatistics s;
        s.connectionCount = m_table->Count();
        s.sessionCount = m_session->Count();
        s.droppedDatagrams = m_host->DroppedDatagrams();
        s.rejectedCapacity = m_host->RejectedCapacity();
        for (const Connection& c : m_table->Entries())
        {
            switch (c.state)
            {
            case ConnectionState::Handshaking:   ++s.handshakingCount; break;
            case ConnectionState::Connected:     ++s.connectedCount; break;
            case ConnectionState::Disconnecting: ++s.disconnectingCount; break;
            case ConnectionState::Connecting:
            case ConnectionState::Disconnected:  break;
            }
        }
        return s;
    }

    std::string NetworkDiagnostics::DescribeState() const
    {
        const NetworkStatistics s = Statistics();
        return common::Format(
            "Networking running={} connections={} session={} handshaking={} connected={} dropped={} rejected={}",
            m_host->IsRunning(), s.connectionCount, s.sessionCount, s.handshakingCount, s.connectedCount,
            s.droppedDatagrams, s.rejectedCapacity);
    }

    std::string NetworkDiagnostics::DescribeConnection(const ConnectionId id) const
    {
        const Connection* const c = m_table->Find(id);
        if (c == nullptr)
        {
            return common::Format("conn {} <absent>", id.value);
        }
        return common::Format("conn {} state={} handshake={} createdTick={} lastRecvTick={}",
                              id.value, ConnectionStateName(c->state), HandshakeStateName(c->handshake),
                              c->createdTick, c->lastRecvTick);
    }

    std::string NetworkDiagnostics::DumpConnections() const
    {
        std::string out = common::Format("Connections ({}):", m_table->Count());
        for (const ConnectionId id : m_table->Ids()) // ascending
        {
            out += ' ';
            out += std::to_string(id.value);
        }
        return out;
    }

    NetworkConsistencyReport NetworkDiagnostics::ValidateConsistency() const
    {
        NetworkConsistencyReport report;
        report.tableHealthy = m_table->ValidateConsistency().IsHealthy();
        report.sessionHealthy = m_session->ValidateConsistency().IsHealthy();
        for (const SessionMember& m : m_session->Members())
        {
            if (m_table->Find(m.id) == nullptr)
            {
                report.sessionWithinTable = false;
                break;
            }
        }
        return report;
    }
} // namespace stalkermp::net
