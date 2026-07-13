// STALKER-MP — Host server (Sprint-006, Step 10)
//
// The authoritative endpoint: accept/admission/handshake/heartbeat/disconnect and
// the deterministic single-threaded per-tick pump, over the engine-free ITransport
// seam (mock/loopback in tests). Reads NO simulation state. Host-authoritative
// ConnectionIds. NO module services (Step 11), no Bootstrap (Step 12), no real
// sockets (Step 13). ADR-007/010/011. Engine-free / OS-free.

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/net/ConnectionTable.h"
#include "stalkermp/net/Handshake.h"
#include "stalkermp/net/IConnectionAuthenticator.h"
#include "stalkermp/net/ITransport.h"
#include "stalkermp/net/MessageRegistry.h"
#include "stalkermp/net/NetTypes.h"
#include "stalkermp/net/NetworkingConfig.h"
#include "stalkermp/net/Session.h"
#include "stalkermp/net/TransportConfig.h"

namespace stalkermp::net
{
    class HostServer
    {
    public:
        explicit HostServer(IConnectionAuthenticator& authenticator) : m_handshake(authenticator) {}

        // Bind + listen (no-op success when NetworkingConfig.enabled == false).
        [[nodiscard]] core::Expected<void> Start(const NetworkingConfig& net, const TransportConfig& transport,
                                                 ITransport& io);

        // Close the listen socket and disconnect all connections (best-effort Bye).
        void Stop(ITransport& io, ConnectionTable& table, Session& session) noexcept;

        // One deterministic per-tick pass (architecture §7 / spec §3).
        void Tick(std::uint64_t currentTick, ITransport& io, ConnectionTable& table, Session& session,
                  MessageRegistry& registry);

        [[nodiscard]] bool IsRunning() const noexcept { return m_running; }
        [[nodiscard]] std::uint64_t DroppedDatagrams() const noexcept { return m_droppedDatagrams; }
        [[nodiscard]] std::uint64_t RejectedCapacity() const noexcept { return m_rejectedCapacity; }

        // Ticks-per-frame conversion base (Step 11 supplies the real value).
        void SetMsPerTick(std::uint32_t msPerTick) noexcept { m_msPerTick = msPerTick == 0 ? 1 : msPerTick; }

    private:
        struct HostConn
        {
            TransportEndpoint endpoint{};
            std::uint16_t outSequence = 0;
            std::deque<HandshakeMessage> handshakeInbox;                       // buffered; one advance/tick
            std::deque<std::pair<Channel, std::vector<std::uint8_t>>> outbox;  // serialized packets
        };

        void Disconnect(ConnectionId id, DisconnectReason reason, ITransport& io, ConnectionTable& table,
                        Session& session);
        void EnqueueControl(HostConn& hc, MessageId id, Channel channel, const std::vector<std::uint8_t>& payload);
        [[nodiscard]] std::vector<std::uint8_t> BuildPacket(MessageId id, Channel channel, std::uint16_t sequence,
                                                            const std::vector<std::uint8_t>& payload) const;

        Handshake m_handshake;
        bool m_running = false;
        std::uint32_t m_msPerTick = 16;

        NetworkingConfig m_net;
        TransportConfig m_transport;
        std::uint64_t m_handshakeTicks = 1;
        std::uint64_t m_connectionTicks = 1;
        std::uint64_t m_heartbeatTicks = 1;

        std::map<std::uint32_t, HostConn> m_conns;                 // by ConnectionId.value (ascending)
        std::unordered_map<std::uint32_t, std::uint32_t> m_endpointToConn; // endpoint.value -> id.value

        std::uint64_t m_droppedDatagrams = 0;
        std::uint64_t m_rejectedCapacity = 0;
    };
} // namespace stalkermp::net
