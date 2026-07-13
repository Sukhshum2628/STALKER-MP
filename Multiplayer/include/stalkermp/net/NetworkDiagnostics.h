// STALKER-MP — Network diagnostics (Sprint-006, Step 11)
//
// Read-only, deterministic, iostream-free (common::Format) views over the module
// state (connection table + session + host counters). NEVER mutates state.
//
// Bandwidth/RTT/packet-trace (spec §3) require byte/RTT counters inside HostServer,
// which is not in Step-11's modify list; those fields are reserved and reported as
// zero/empty until the host is instrumented (a later change). Engine-free / OS-free.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "stalkermp/net/ConnectionTable.h"
#include "stalkermp/net/HostServer.h"
#include "stalkermp/net/Session.h"

namespace stalkermp::net
{
    struct NetworkStatistics
    {
        std::size_t connectionCount = 0;
        std::size_t sessionCount = 0;
        std::size_t handshakingCount = 0;
        std::size_t connectedCount = 0;
        std::size_t disconnectingCount = 0;
        std::uint64_t droppedDatagrams = 0;
        std::uint64_t rejectedCapacity = 0;
    };

    struct NetworkConsistencyReport
    {
        bool tableHealthy = true;
        bool sessionHealthy = true;
        bool sessionWithinTable = true; // every session member is a live connection

        [[nodiscard]] bool IsHealthy() const noexcept
        {
            return tableHealthy && sessionHealthy && sessionWithinTable;
        }
    };

    class NetworkDiagnostics
    {
    public:
        NetworkDiagnostics(const ConnectionTable& table, const Session& session, const HostServer& host)
            : m_table(&table), m_session(&session), m_host(&host)
        {
        }

        [[nodiscard]] NetworkStatistics Statistics() const;
        [[nodiscard]] std::string DescribeState() const;
        [[nodiscard]] std::string DescribeConnection(ConnectionId id) const;
        [[nodiscard]] std::string DumpConnections() const; // ascending
        [[nodiscard]] NetworkConsistencyReport ValidateConsistency() const;

    private:
        const ConnectionTable* m_table;
        const Session* m_session;
        const HostServer* m_host;
    };
} // namespace stalkermp::net
