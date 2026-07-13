// STALKER-MP — Loopback transport (Sprint-006, Step 3)
//
// Two in-process ITransport endpoints wired to each other through shared
// in-memory lanes: Send on one appears in the peer's Received() after Poll.
// Clean channel (no knobs) — used for host<->fake-client integration (Step 10).
// Engine-free / OS-free; no threads; no exceptions.

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "stalkermp/net/ITransport.h"

namespace stalkermp::net
{
    class LoopbackTransport final : public ITransport
    {
    public:
        using Lane = std::deque<ReceivedDatagram>;

        // Create two connected transports. Endpoint 1 <-> endpoint 2.
        [[nodiscard]] static std::pair<std::unique_ptr<LoopbackTransport>, std::unique_ptr<LoopbackTransport>>
        MakePair()
        {
            auto laneA = std::make_shared<Lane>(); // delivered to A
            auto laneB = std::make_shared<Lane>(); // delivered to B
            auto a = std::unique_ptr<LoopbackTransport>(
                new LoopbackTransport(TransportEndpoint{1}, TransportEndpoint{2}, laneA, laneB));
            auto b = std::unique_ptr<LoopbackTransport>(
                new LoopbackTransport(TransportEndpoint{2}, TransportEndpoint{1}, laneB, laneA));
            return {std::move(a), std::move(b)};
        }

        [[nodiscard]] TransportEndpoint Self() const { return m_self; }
        [[nodiscard]] TransportEndpoint Peer() const { return m_peer; }

        // --- ITransport -------------------------------------------------------
        [[nodiscard]] core::Expected<void> Bind(std::uint16_t, std::string_view) override
        {
            m_bound = true;
            m_acceptedReady.push_back(m_peer); // the single peer is "accepted" once
            return core::Success();
        }
        [[nodiscard]] core::Expected<void> Listen(std::uint32_t) override { return core::Success(); }

        void Poll(std::size_t maxBytesBudget) override
        {
            if (!m_bound)
            {
                return;
            }
            std::size_t used = 0;
            while (!m_inbox->empty())
            {
                const std::size_t sz = m_inbox->front().bytes.size();
                if (used + sz > maxBytesBudget)
                {
                    break; // remainder stays for the next tick
                }
                used += sz;
                m_received.push_back(std::move(m_inbox->front()));
                m_inbox->pop_front();
            }
        }

        [[nodiscard]] TransportOutcome Send(TransportEndpoint to, Channel channel, ConstByteSpan bytes) override
        {
            if (!m_bound)
            {
                return TransportOutcome::Error;
            }
            if (to != m_peer)
            {
                return TransportOutcome::EndpointMissing;
            }
            ReceivedDatagram d;
            d.from = m_self;
            d.channel = channel;
            d.bytes.assign(bytes.data, bytes.data + bytes.size); // owned value copy
            m_outbox->push_back(std::move(d));
            return TransportOutcome::Ok; // immediate loopback delivery to the lane
        }

        void CloseListen() override { m_bound = false; }
        void CloseEndpoint(TransportEndpoint) override {}

        [[nodiscard]] std::vector<TransportEndpoint> Accepted() override
        {
            return std::exchange(m_acceptedReady, {});
        }
        [[nodiscard]] std::vector<ReceivedDatagram> Received() override
        {
            return std::exchange(m_received, {});
        }

    private:
        LoopbackTransport(TransportEndpoint self, TransportEndpoint peer,
                          std::shared_ptr<Lane> inbox, std::shared_ptr<Lane> outbox)
            : m_self(self), m_peer(peer), m_inbox(std::move(inbox)), m_outbox(std::move(outbox))
        {
        }

        TransportEndpoint m_self{};
        TransportEndpoint m_peer{};
        std::shared_ptr<Lane> m_inbox;  // datagrams destined for this transport
        std::shared_ptr<Lane> m_outbox; // datagrams this transport sends to the peer
        bool m_bound = false;
        std::vector<ReceivedDatagram> m_received;
        std::vector<TransportEndpoint> m_acceptedReady;
    };
} // namespace stalkermp::net
