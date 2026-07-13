// STALKER-MP — Player manager service (Sprint-007, Step 11)
//
// See PlayerManagerService.h. Initialize subscribes the enqueue-only observer to
// the Session and registers the additive player-request message ids (duplicate
// registration -> deterministic Initialize failure, per the frozen MessageRegistry
// Registration Verification Rules). Tick drains + applies queued deltas at 250.
// Engine-free / OS-free.

#include "stalkermp/player/PlayerManagerService.h"

#include "stalkermp/player/PlayerMessageIds.h"

namespace stalkermp::player
{
    core::Expected<void> PlayerManagerService::Initialize()
    {
        // Enqueue-only session bridge (fires during the networking tick; mutates
        // nothing — deltas are applied here at tick 250, Architecture AD-3).
        m_session.Subscribe(&m_observer);

        // Additive DATA-range player-request ids (frozen allocation). RegisterData
        // rejects a duplicate id -> that error fails Initialize deterministically.
        if (auto registered = m_registry.RegisterData(kMsgPlayerJoinRequest, &m_joinHandler); !registered)
        {
            return registered;
        }
        if (auto registered = m_registry.RegisterData(kMsgPlayerRespawnRequest, &m_respawnHandler); !registered)
        {
            return registered;
        }
        return core::Success();
    }

    void PlayerManagerService::Tick(const double /*deltaSeconds*/)
    {
        ++m_tick; // monotonic, deterministic (tick-derived control flow)
        m_manager.ApplyPendingDeltas(m_queue, m_tick);
    }
} // namespace stalkermp::player
