// STALKER-MP — Networking service (Sprint-006, Step 11)
//
// See NetworkingService.h. Engine-free / OS-free; no exceptions, core::Expected.
// Single ms->tick conversion in Initialize; one deterministic tick phase.

#include "stalkermp/net/NetworkingService.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::net
{
    core::Expected<void> NetworkingService::Initialize()
    {
        if (m_net.maxConnections == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "[networking] max_connections must be >= 1");
        }

        // Single ms->tick conversion point (host derives its budgets from this).
        m_host.SetMsPerTick(kDefaultMsPerTick);

        // Start binds/listens only when enabled; otherwise a no-op success.
        return m_host.Start(m_net, m_transportConfig, *m_transport);
    }

    void NetworkingService::Shutdown() noexcept
    {
        // Idempotent: Stop closes the listen socket and disconnects all; a second
        // call finds an empty table and a stopped host.
        m_host.Stop(*m_transport, m_table, *m_session);
    }

    void NetworkingService::Tick(double /*deltaSeconds*/)
    {
        // One monotonic tick; the single deterministic network phase.
        m_host.Tick(++m_tick, *m_transport, m_table, *m_session, *m_registry);
    }
} // namespace stalkermp::net
