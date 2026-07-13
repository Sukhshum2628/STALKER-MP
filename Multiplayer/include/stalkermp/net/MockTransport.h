// STALKER-MP — Mock transport (Sprint-006, Step 3)
//
// Deterministic in-memory ITransport for unit tests. Scriptable inbound and
// accepts; captured outbound with deterministic failure knobs (drop/duplicate/
// reorder/corrupt). Engine-free / OS-free; no threads; no exceptions.

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "stalkermp/net/ITransport.h"

namespace stalkermp::net
{
    class MockTransport final : public ITransport
    {
    public:
        // Captured outbound packet (post-knob).
        struct SentPacket
        {
            TransportEndpoint to{};
            Channel channel = Channel::Reliable;
            std::vector<std::uint8_t> bytes;
        };

        // --- Test scripting ---------------------------------------------------
        void RegisterEndpoint(TransportEndpoint e, bool online = true) { m_endpoints[e.value] = online; }
        void SetOnline(TransportEndpoint e, bool online) { m_endpoints[e.value] = online; }

        void InjectInbound(TransportEndpoint from, Channel channel, std::vector<std::uint8_t> bytes)
        {
            m_stagedInbound.push_back(ReceivedDatagram{from, channel, std::move(bytes)});
        }
        void InjectAccept(TransportEndpoint e)
        {
            m_endpoints[e.value] = true; // an accepted peer is known & online
            m_acceptedReady.push_back(e);
        }

        // --- Deterministic failure knobs (applied to outbound capture) --------
        void SetOutgoingCapacity(std::size_t n) { m_outgoingCapacity = n; }
        std::size_t dropNextN = 0;    // drop the next N captured sends
        bool duplicateNext = false;   // record the next send twice
        bool corruptNext = false;     // flip byte[0] of the next send
        std::size_t reorderWindow = 0; // >=2 => pairwise-swap consecutive sends

        // --- Inspection -------------------------------------------------------
        [[nodiscard]] const std::vector<SentPacket>& Outgoing() const { return m_outgoing; }
        [[nodiscard]] bool IsBound() const { return m_bound; }

        // --- ITransport -------------------------------------------------------
        [[nodiscard]] core::Expected<void> Bind(std::uint16_t, std::string_view) override
        {
            m_bound = true;
            return core::Success();
        }
        [[nodiscard]] core::Expected<void> Listen(std::uint32_t) override { return core::Success(); }

        void Poll(std::size_t maxBytesBudget) override
        {
            if (!m_bound)
            {
                return; // defined no-op before Bind
            }
            std::size_t used = 0;
            while (!m_stagedInbound.empty())
            {
                const std::size_t sz = m_stagedInbound.front().bytes.size();
                if (used + sz > maxBytesBudget)
                {
                    break; // remainder stays staged for the next tick
                }
                used += sz;
                m_received.push_back(std::move(m_stagedInbound.front()));
                m_stagedInbound.pop_front();
            }
        }

        [[nodiscard]] TransportOutcome Send(TransportEndpoint to, Channel channel, ConstByteSpan bytes) override
        {
            if (!m_bound)
            {
                return TransportOutcome::Error; // defined: Send before Bind
            }
            const auto it = m_endpoints.find(to.value);
            if (it == m_endpoints.end() || !it->second)
            {
                return TransportOutcome::EndpointMissing;
            }
            if (m_outgoing.size() >= m_outgoingCapacity)
            {
                return TransportOutcome::WouldBlock;
            }

            if (dropNextN > 0)
            {
                --dropNextN;
                return TransportOutcome::Queued; // dropped from capture, still "accepted"
            }

            SentPacket pkt;
            pkt.to = to;
            pkt.channel = channel;
            pkt.bytes.assign(bytes.data, bytes.data + bytes.size); // owned value copy
            if (corruptNext && !pkt.bytes.empty())
            {
                pkt.bytes[0] = static_cast<std::uint8_t>(pkt.bytes[0] ^ 0xFF);
                corruptNext = false;
            }

            const bool duplicate = duplicateNext;
            duplicateNext = false;

            EmitOutbound(std::move(pkt));
            if (duplicate)
            {
                SentPacket copy;
                copy.to = to;
                copy.channel = channel;
                copy.bytes.assign(bytes.data, bytes.data + bytes.size);
                EmitOutbound(std::move(copy));
            }
            return TransportOutcome::Queued;
        }

        void CloseListen() override { m_bound = false; }
        void CloseEndpoint(TransportEndpoint e) override { m_endpoints.erase(e.value); }

        [[nodiscard]] std::vector<TransportEndpoint> Accepted() override
        {
            return std::exchange(m_acceptedReady, {});
        }
        [[nodiscard]] std::vector<ReceivedDatagram> Received() override
        {
            return std::exchange(m_received, {});
        }

    private:
        // Pairwise reorder within a window of 2 (deterministic): hold one packet;
        // when the next arrives, emit the new one first, then the held one.
        void EmitOutbound(SentPacket pkt)
        {
            if (reorderWindow >= 2)
            {
                if (m_holdValid)
                {
                    m_outgoing.push_back(std::move(pkt));       // new first
                    m_outgoing.push_back(std::move(m_hold));    // then held (swapped)
                    m_holdValid = false;
                }
                else
                {
                    m_hold = std::move(pkt);
                    m_holdValid = true;
                }
                return;
            }
            m_outgoing.push_back(std::move(pkt));
        }

        bool m_bound = false;
        std::size_t m_outgoingCapacity = 1024;
        std::unordered_map<std::uint32_t, bool> m_endpoints; // value -> online
        std::deque<ReceivedDatagram> m_stagedInbound;
        std::vector<ReceivedDatagram> m_received;
        std::vector<TransportEndpoint> m_acceptedReady;
        std::vector<SentPacket> m_outgoing;
        SentPacket m_hold;
        bool m_holdValid = false;
    };
} // namespace stalkermp::net
