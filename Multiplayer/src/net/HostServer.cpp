// STALKER-MP — Host server (Sprint-006, Step 10)
//
// See HostServer.h. Engine-free / OS-free; no exceptions, value results.
// Deterministic single-threaded per-tick pump. Reads no simulation state.

#include "stalkermp/net/HostServer.h"

#include <algorithm>

#include "stalkermp/net/PacketHeader.h"
#include "stalkermp/net/ProtocolConstants.h"

namespace stalkermp::net
{
    namespace
    {
        [[nodiscard]] std::vector<std::uint8_t> HandshakePayload(std::uint32_t nonce)
        {
            std::vector<std::uint8_t> p(6);
            ByteWriter w(p.data(), p.size());
            (void)w.WriteU16(0);      // version (unused on outbound)
            (void)w.WriteU32(nonce);  // server nonce (Challenge) / 0 (Established)
            return p;
        }

        [[nodiscard]] std::vector<std::uint8_t> TickPayload(std::uint64_t tick)
        {
            std::vector<std::uint8_t> p(8);
            ByteWriter w(p.data(), p.size());
            (void)w.WriteU32(static_cast<std::uint32_t>(tick & 0xFFFFFFFFull));
            (void)w.WriteU32(static_cast<std::uint32_t>(tick >> 32));
            return p;
        }

        [[nodiscard]] bool IsHandshakeMessageId(MessageId id) noexcept
        {
            return id == kMsgHello || id == kMsgChallenge || id == kMsgResponse || id == kMsgEstablished;
        }
    } // namespace

    core::Expected<void> HostServer::Start(const NetworkingConfig& net, const TransportConfig& transport,
                                           ITransport& io)
    {
        m_net = net;
        m_transport = transport;
        m_handshakeTicks = TicksFromMs(net.handshakeTimeoutMs, m_msPerTick);
        m_connectionTicks = TicksFromMs(net.connectionTimeoutMs, m_msPerTick);
        m_heartbeatTicks = TicksFromMs(net.heartbeatIntervalMs, m_msPerTick);

        if (!net.enabled)
        {
            m_running = true; // no-op listen; ticks are harmless no-ops
            return core::Success();
        }
        if (auto r = io.Bind(transport.listenPort, transport.bindAddress); !r) { return r; }
        if (auto r = io.Listen(transport.backlog); !r) { return r; }
        m_running = true;
        return core::Success();
    }

    void HostServer::Stop(ITransport& io, ConnectionTable& table, Session& session) noexcept
    {
        io.CloseListen();
        for (const ConnectionId id : table.Ids())
        {
            Disconnect(id, DisconnectReason::Graceful, io, table, session);
        }
        m_conns.clear();
        m_endpointToConn.clear();
        m_running = false;
    }

    std::vector<std::uint8_t> HostServer::BuildPacket(const MessageId id, const Channel channel,
                                                      const std::uint16_t sequence,
                                                      const std::vector<std::uint8_t>& payload) const
    {
        PacketHeader h;
        h.magic = kProtocolMagic;
        h.protocolVersion = kProtocolVersion;
        h.channel = static_cast<std::uint8_t>(channel);
        h.flags = kFlagControl;
        h.sequence = sequence;
        h.messageId = id.value;
        h.payloadLength = static_cast<std::uint16_t>(payload.size());

        std::vector<std::uint8_t> pkt(kPacketHeaderWireSize + payload.size());
        ByteWriter w(pkt.data(), pkt.size());
        (void)SerializeHeader(h, w);
        if (!payload.empty())
        {
            std::copy(payload.begin(), payload.end(),
                      pkt.begin() + static_cast<std::ptrdiff_t>(kPacketHeaderWireSize));
        }
        return pkt;
    }

    void HostServer::EnqueueControl(HostConn& hc, const MessageId id, const Channel channel,
                                    const std::vector<std::uint8_t>& payload)
    {
        hc.outbox.push_back({channel, BuildPacket(id, channel, hc.outSequence++, payload)});
    }

    void HostServer::Disconnect(const ConnectionId id, const DisconnectReason reason, ITransport& io,
                                ConnectionTable& table, Session& session)
    {
        const DisconnectIntent intent = table.BeginDisconnect(id, reason);
        if (intent.performed)
        {
            const std::vector<std::uint8_t> bye = BuildPacket(kMsgBye, Channel::Reliable, 0, {});
            (void)io.Send(intent.endpoint, Channel::Reliable, ConstByteSpan{bye.data(), bye.size()});
        }
        session.Remove(id, reason);
        const auto it = m_conns.find(id.value);
        if (it != m_conns.end())
        {
            m_endpointToConn.erase(it->second.endpoint.value);
            m_conns.erase(it);
        }
    }

    void HostServer::Tick(const std::uint64_t currentTick, ITransport& io, ConnectionTable& table,
                          Session& session, MessageRegistry& registry)
    {
        if (!m_running)
        {
            return;
        }

        // 1) Pump the transport (non-blocking, byte-budgeted).
        io.Poll(m_transport.maxBytesPerTick);

        // 2) Accept new endpoints -> Add (capacity admission).
        for (const TransportEndpoint ep : io.Accepted())
        {
            auto added = table.Add(ep);
            if (!added)
            {
                ++m_rejectedCapacity; // over capacity: not admitted (CapacityFull)
                const std::vector<std::uint8_t> bye = BuildPacket(kMsgBye, Channel::Reliable, 0, {});
                (void)io.Send(ep, Channel::Reliable, ConstByteSpan{bye.data(), bye.size()});
                continue;
            }
            const ConnectionId id = added.Value();
            if (Connection* c = table.FindMutable(id))
            {
                c->endpoint = ep;
                c->state = ConnectionState::Handshaking;
                c->createdTick = currentTick;
                c->lastRecvTick = currentTick;
            }
            HostConn hc;
            hc.endpoint = ep;
            m_conns.emplace(id.value, std::move(hc));
            m_endpointToConn[ep.value] = id.value;
        }

        // 3) Route received datagrams.
        for (ReceivedDatagram& dg : io.Received())
        {
            const auto eit = m_endpointToConn.find(dg.from.value);
            if (eit == m_endpointToConn.end())
            {
                ++m_droppedDatagrams; // unknown endpoint
                continue;
            }
            const ConnectionId id{eit->second};
            Connection* const c = table.FindMutable(id);
            if (c == nullptr)
            {
                continue;
            }
            ByteReader r(dg.bytes.data(), dg.bytes.size());
            auto header = DeserializeHeader(r);
            if (!header)
            {
                ++m_droppedDatagrams; // malformed header
                continue;
            }
            c->lastRecvTick = currentTick;
            const MessageId mid{header.Value().messageId};
            const auto cit = m_conns.find(id.value);
            if (cit == m_conns.end())
            {
                continue;
            }
            HostConn& hc = cit->second;

            if (IsHandshakeMessageId(mid))
            {
                auto msg = DeserializeHandshake(mid, r);
                if (!msg)
                {
                    ++m_droppedDatagrams;
                    continue;
                }
                hc.handshakeInbox.push_back(msg.Value()); // one advance/tick (step 4)
            }
            else if (mid == kMsgPing)
            {
                EnqueueControl(hc, kMsgPong, Channel::Reliable, TickPayload(currentTick));
            }
            else if (mid == kMsgBye)
            {
                Disconnect(id, DisconnectReason::Graceful, io, table, session);
            }
            else if (mid == kMsgPong)
            {
                // RTT diagnostic (Step 11) -> ignored here.
            }
            else if (IsDataId(mid))
            {
                DispatchContext ctx{id, currentTick};
                (void)registry.Dispatch(mid, r, ctx);
            }
            // other control ids: ignored.
        }

        // 4a) Timeouts (ascending ConnectionId), unified disconnect.
        for (const ConnectionId tid : table.ScanTimeouts(currentTick, m_handshakeTicks, m_connectionTicks))
        {
            Disconnect(tid, DisconnectReason::Timeout, io, table, session);
        }

        // 4b) Handshake advance: at most ONE step per connection per tick (ascending).
        for (const ConnectionId id : table.Ids())
        {
            const auto cit = m_conns.find(id.value);
            if (cit == m_conns.end() || cit->second.handshakeInbox.empty())
            {
                continue;
            }
            Connection* const c = table.FindMutable(id);
            if (c == nullptr)
            {
                continue;
            }
            HostConn& hc = cit->second;
            const HandshakeMessage msg = hc.handshakeInbox.front();
            hc.handshakeInbox.pop_front();

            const HandshakeResult res = m_handshake.Advance(*c, std::optional<HandshakeMessage>(msg), currentTick);
            if (res.drop)
            {
                Disconnect(id, *res.drop, io, table, session);
                continue;
            }
            if (res.outbound)
            {
                EnqueueControl(hc, res.outbound->id, Channel::Reliable, HandshakePayload(res.outbound->nonce));
            }
            if (c->handshake == HandshakeState::Established)
            {
                c->state = ConnectionState::Connected;
                c->establishedTick = currentTick;
                c->lastSendTick = currentTick; // heartbeat fires one interval later
                (void)session.Admit(id, currentTick);
            }
        }

        // 5) Heartbeat: Connected connections past the interval since last send.
        for (const ConnectionId id : table.Ids())
        {
            Connection* const c = table.FindMutable(id);
            if (c == nullptr || c->state != ConnectionState::Connected)
            {
                continue;
            }
            if (currentTick >= c->lastSendTick + m_heartbeatTicks)
            {
                const auto cit = m_conns.find(id.value);
                if (cit != m_conns.end())
                {
                    EnqueueControl(cit->second, kMsgPing, Channel::Reliable, TickPayload(currentTick));
                    c->lastSendTick = currentTick;
                }
            }
        }

        // 6) Drain outgoing queues -> Send, bounded by maxBytesPerTick.
        std::size_t sent = 0;
        const std::size_t budget = m_transport.maxBytesPerTick;
        bool budgetReached = false;
        for (const ConnectionId id : table.Ids())
        {
            if (budgetReached)
            {
                break;
            }
            const auto cit = m_conns.find(id.value);
            if (cit == m_conns.end())
            {
                continue;
            }
            HostConn& hc = cit->second;
            while (!hc.outbox.empty())
            {
                const auto& front = hc.outbox.front();
                if (sent + front.second.size() > budget)
                {
                    budgetReached = true; // remainder deferred to next tick
                    break;
                }
                const TransportOutcome outcome =
                    io.Send(hc.endpoint, front.first, ConstByteSpan{front.second.data(), front.second.size()});
                if (outcome == TransportOutcome::WouldBlock)
                {
                    budgetReached = true; // retry next tick
                    break;
                }
                sent += front.second.size();
                hc.outbox.pop_front();
            }
        }
    }
} // namespace stalkermp::net
