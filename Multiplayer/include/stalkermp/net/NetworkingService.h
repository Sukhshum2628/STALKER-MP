// STALKER-MP — Networking service (Sprint-006, Step 11)
//
// The umbrella: core::IService + core::ITickable. Owns the HostServer and
// ConnectionTable (and, transitively, the per-connection queue store inside the
// host); holds non-owning refs to the ITransport, Session, and MessageRegistry.
// Dependencies() are ORDERING names only — NO simulation handle is stored
// (networking never owns simulation). NO dispatcher subscription / Bootstrap
// wiring here (that is Step 12). Engine-free / OS-free. ADR-007/010/011.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/net/ConnectionTable.h"
#include "stalkermp/net/HostServer.h"
#include "stalkermp/net/IConnectionAuthenticator.h"
#include "stalkermp/net/ITransport.h"
#include "stalkermp/net/MessageRegistry.h"
#include "stalkermp/net/NetworkingConfig.h"
#include "stalkermp/net/Session.h"
#include "stalkermp/net/TransportConfig.h"

namespace stalkermp::net
{
    // Fixed frame period for the single ms->tick conversion (Initialize).
    inline constexpr std::uint32_t kDefaultMsPerTick = 16;

    class NetworkingService final : public core::IService,
                                    public core::ITickable
    {
    public:
        // Non-owning refs (transport/session/registry/authenticator) must outlive
        // the service. The ConnectionTable + HostServer are owned here.
        NetworkingService(NetworkingConfig net, TransportConfig transport, ITransport& io, Session& session,
                          MessageRegistry& registry, IConnectionAuthenticator& authenticator)
            : m_host(authenticator)
            , m_table(net.maxConnections)
            , m_transport(&io)
            , m_session(&session)
            , m_registry(&registry)
            , m_net(std::move(net))
            , m_transportConfig(std::move(transport))
        {
        }

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Networking"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override
        {
            return {"World", "EntityRegistry", "BubbleManager", "TransitionManager"};
        }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable ------------------------------------------------
        // The single deterministic network phase per frame (Step-10 host pass).
        void Tick(double deltaSeconds) override;

        // --- Access (read-only; for diagnostics/tests) ----------------------
        [[nodiscard]] const ConnectionTable& Table() const noexcept { return m_table; }
        [[nodiscard]] const HostServer& Host() const noexcept { return m_host; }
        [[nodiscard]] std::uint64_t LastTick() const noexcept { return m_tick; }

    private:
        HostServer m_host;
        ConnectionTable m_table;
        ITransport* m_transport;   // non-owning
        Session* m_session;        // non-owning (SessionService-owned)
        MessageRegistry* m_registry; // non-owning (MessageRegistryService-owned)
        NetworkingConfig m_net;
        TransportConfig m_transportConfig;
        std::uint64_t m_tick = 0;
    };
} // namespace stalkermp::net
