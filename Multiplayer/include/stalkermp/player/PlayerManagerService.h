// STALKER-MP — Player manager service (Sprint-007, Step 11)
//
// The umbrella IService + ITickable that integrates the player subsystem into the
// composition root. Ticked once per frame at tick_order::kPlayerLifecycle = 250
// (EntityRegistry -> Player -> Bubble). The PlayerManager, its PlayerDeltaQueue,
// and the NetworkedPlayerPositionSource are owned by the Runtime (constructed
// before the Bubble, which binds the networked source at construction); this
// service references them and owns the session observer + the two request
// handlers. On Initialize it subscribes the enqueue-only observer to the Session
// and registers the additive player-request message ids (0x0100/0x0101). On Tick
// it drains queued session deltas and applies them deterministically (all
// mutation happens here at 250, never in the observer — Architecture AD-3).
//
// Engine-free / OS-free. ADR-007; ADR-011 (single-threaded). Networking untouched
// except the two additive DATA-range ids (ADR-010).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/FrameDispatcher.h"  // core::ITickable
#include "stalkermp/core/ServiceRegistry.h"  // core::IService
#include "stalkermp/net/ByteCursor.h"        // net::ByteReader
#include "stalkermp/net/MessageRegistry.h"   // net::MessageRegistry, IMessageHandler, DispatchContext
#include "stalkermp/net/Session.h"           // net::Session
#include "stalkermp/player/PlayerDeltaQueue.h"
#include "stalkermp/player/PlayerManager.h"
#include "stalkermp/player/PlayerSessionObserver.h"

namespace stalkermp::player
{
    class PlayerManagerService final : public core::IService, public core::ITickable
    {
    public:
        // References the Runtime-owned PlayerManager + queue (so the Bubble can
        // bind the networked source at construction, before this late-registered
        // service exists); owns the observer + request handlers.
        PlayerManagerService(PlayerManager& manager, PlayerDeltaQueue& queue, net::Session& session,
                             net::MessageRegistry& registry)
            : m_manager(manager), m_queue(queue), m_session(session), m_registry(registry), m_observer(queue),
              m_joinHandler(manager), m_respawnHandler(manager)
        {
        }

        // --- core::IService ---------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "PlayerManager"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override
        {
            // Ordering-only edges (must be registered earlier). The transition
            // service registers as "TransitionManager".
            return {"World", "EntityRegistry", "BubbleManager", "TransitionManager", "Networking"};
        }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override {}

        // --- core::ITickable --------------------------------------------------
        void Tick(double deltaSeconds) override;

        [[nodiscard]] PlayerManager& Manager() noexcept { return m_manager; }
        [[nodiscard]] std::uint64_t LastTick() const noexcept { return m_tick; }

    private:
        // Minimal host-side request routing (Host Authority): client requests are
        // routed into the host-authoritative manager; the host owns the outcome.
        class JoinRequestHandler final : public net::IMessageHandler
        {
        public:
            explicit JoinRequestHandler(PlayerManager& manager) : m_manager(manager) {}
            [[nodiscard]] core::Expected<void> Handle(net::ByteReader& /*payload*/,
                                                      net::DispatchContext& context) override
            {
                (void)m_manager.RequestJoin(context.source, PlayerProfile{}, context.tick);
                return core::Success();
            }

        private:
            PlayerManager& m_manager;
        };

        class RespawnRequestHandler final : public net::IMessageHandler
        {
        public:
            explicit RespawnRequestHandler(PlayerManager& manager) : m_manager(manager) {}
            [[nodiscard]] core::Expected<void> Handle(net::ByteReader& /*payload*/,
                                                      net::DispatchContext& context) override
            {
                if (const auto view = m_manager.FindByConnection(context.source))
                {
                    (void)m_manager.RequestRespawn(view->id, context.tick);
                }
                return core::Success();
            }

        private:
            PlayerManager& m_manager;
        };

        PlayerManager& m_manager;         // Runtime-owned
        PlayerDeltaQueue& m_queue;        // Runtime-owned
        net::Session& m_session;          // Sprint-006 (SessionService-owned)
        net::MessageRegistry& m_registry; // Sprint-006 (MessageRegistryService-owned)

        PlayerSessionObserver m_observer;       // enqueue-only bridge (owned here)
        JoinRequestHandler m_joinHandler;       // owned here
        RespawnRequestHandler m_respawnHandler; // owned here
        std::uint64_t m_tick = 0;               // monotonic frame counter
    };
} // namespace stalkermp::player
