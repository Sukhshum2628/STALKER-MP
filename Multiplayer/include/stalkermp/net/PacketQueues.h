// STALKER-MP — Packet queues (Sprint-006, Step 9)
//
// Per-connection, per-Channel bounded outgoing/incoming queues of OWNED byte
// buffers. Deterministic overflow policy: reliable overflow -> disconnect signal
// (caller maps to DisconnectReason::TransportError); unreliable overflow ->
// drop-oldest. NO host loop, no services, no transport. Engine-free / OS-free.
// ADR-007. Owned buffers (moved in/out; never aliased into transport memory).

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

#include "stalkermp/net/NetTypes.h" // Channel

namespace stalkermp::net
{
    enum class EnqueueResult : std::uint8_t
    {
        Enqueued = 0,
        DroppedOldest,     // unreliable: oldest dropped to make room
        ReliableOverflow,  // reliable: not enqueued; caller must disconnect (TransportError)
    };

    class PacketQueues
    {
    public:
        PacketQueues(std::size_t maxOutgoing, std::size_t maxIncoming)
            : m_maxOutgoing(maxOutgoing), m_maxIncoming(maxIncoming)
        {
        }

        [[nodiscard]] EnqueueResult EnqueueOutgoing(Channel channel, std::vector<std::uint8_t> bytes)
        {
            return Push(m_outgoing[Index(channel)], m_maxOutgoing, channel, std::move(bytes));
        }
        [[nodiscard]] EnqueueResult EnqueueIncoming(Channel channel, std::vector<std::uint8_t> bytes)
        {
            return Push(m_incoming[Index(channel)], m_maxIncoming, channel, std::move(bytes));
        }

        [[nodiscard]] std::optional<std::vector<std::uint8_t>> DequeueOutgoing(Channel channel)
        {
            return Pop(m_outgoing[Index(channel)]);
        }
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> DequeueIncoming(Channel channel)
        {
            return Pop(m_incoming[Index(channel)]);
        }

        [[nodiscard]] std::size_t OutgoingCount(Channel channel) const { return m_outgoing[Index(channel)].size(); }
        [[nodiscard]] std::size_t IncomingCount(Channel channel) const { return m_incoming[Index(channel)].size(); }

        void Clear()
        {
            for (auto& q : m_outgoing) { q.clear(); }
            for (auto& q : m_incoming) { q.clear(); }
        }

        // True when every queue is within its configured bound.
        [[nodiscard]] bool ValidateConsistency() const
        {
            for (const auto& q : m_outgoing) { if (q.size() > m_maxOutgoing) { return false; } }
            for (const auto& q : m_incoming) { if (q.size() > m_maxIncoming) { return false; } }
            return true;
        }

    private:
        [[nodiscard]] static std::size_t Index(Channel c) noexcept { return static_cast<std::size_t>(c); }

        [[nodiscard]] static EnqueueResult Push(std::deque<std::vector<std::uint8_t>>& q, std::size_t max,
                                                Channel channel, std::vector<std::uint8_t> bytes)
        {
            if (q.size() >= max)
            {
                if (channel == Channel::Reliable)
                {
                    return EnqueueResult::ReliableOverflow; // not enqueued
                }
                q.pop_front(); // unreliable: drop-oldest
                q.push_back(std::move(bytes));
                return EnqueueResult::DroppedOldest;
            }
            q.push_back(std::move(bytes));
            return EnqueueResult::Enqueued;
        }

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> Pop(std::deque<std::vector<std::uint8_t>>& q)
        {
            if (q.empty())
            {
                return std::nullopt;
            }
            std::vector<std::uint8_t> front = std::move(q.front());
            q.pop_front();
            return front;
        }

        std::size_t m_maxOutgoing;
        std::size_t m_maxIncoming;
        std::deque<std::vector<std::uint8_t>> m_outgoing[2]; // [Channel]
        std::deque<std::vector<std::uint8_t>> m_incoming[2];
    };
} // namespace stalkermp::net
