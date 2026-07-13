// STALKER-MP — Bubble Manager service (Sprint-004, §7.1)
//
// See BubbleManagerService.h. Engine-free; no exceptions, no throwing STL
// (ADR-007). Lifecycle only — no per-frame update in this step.

#include "stalkermp/world/BubbleManagerService.h"

#include <utility>

#include "stalkermp/core/Log.h"

namespace stalkermp::world
{
    BubbleManagerService::BubbleManagerService(BubbleConfiguration configuration,
                                               const ISpatialQueries* spatialQueries,
                                               const IPlayerPositionSource* playerSource)
        : m_manager(std::move(configuration), spatialQueries, playerSource)
    {
    }

    core::Expected<void> BubbleManagerService::Initialize()
    {
        if (m_manager.Configuration().debugFlags != 0)
        {
            core::Log().Info("BubbleManager", "Bubble manager initialized");
        }
        return core::Success();
    }

    void BubbleManagerService::Shutdown() noexcept
    {
        // No engine resources; the BubbleManager (and its transient activation
        // state) is destroyed with the service. The dispatcher subscription is
        // removed at the composition root before shutdown. Nothing to release here.
    }

    void BubbleManagerService::Tick(double /*deltaSeconds*/)
    {
        // One activation computation per frame. The monotonic counter is the
        // current tick (deterministic; §3C uses it as BubbleState.updateTick).
        m_manager.Update(++m_tick);
    }
} // namespace stalkermp::world
