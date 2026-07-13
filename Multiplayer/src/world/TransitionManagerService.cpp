// STALKER-MP — ALife Transition Manager service (Sprint-005, Step 8)
//
// See TransitionManagerService.h. Engine-free; no exceptions, no throwing STL
// (ADR-007). Lifecycle + per-frame tick only — no dispatcher subscription and no
// Bootstrap wiring in this step (those are Step 10).

#include "stalkermp/world/TransitionManagerService.h"

#include "stalkermp/core/Log.h"

namespace stalkermp::world
{
    TransitionManagerService::TransitionManagerService(const BubbleManager* bubble,
                                                       const EntityRegistry* registry,
                                                       IAlifeSwitchGateway* gateway)
        : m_manager(bubble, registry, gateway)
    {
    }

    core::Expected<void> TransitionManagerService::Initialize()
    {
        // No engine resources to acquire; the TransitionManager and its transient
        // intent state are constructed with the service. Nothing to do here beyond
        // reporting readiness.
        return core::Success();
    }

    void TransitionManagerService::Shutdown() noexcept
    {
        // No engine resources; the TransitionManager (and its transient intent
        // state) is destroyed with the service. The dispatcher subscription is
        // removed at the composition root before shutdown (a later step). Nothing
        // to release here.
    }

    void TransitionManagerService::Tick(double /*deltaSeconds*/)
    {
        // One transition-pipeline tick per frame. The monotonic counter is the
        // current tick (deterministic; passed to TransitionManager::Update).
        m_manager.Update(++m_tick);
    }
} // namespace stalkermp::world
