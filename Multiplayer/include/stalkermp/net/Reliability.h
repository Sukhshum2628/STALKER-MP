// STALKER-MP — Reliability bookkeeping (Sprint-006, Step 9)
//
// Reliable-channel sliding window: sequence assignment, in-flight tracking,
// ack processing (ack + ackBits, ADR-010), tick-derived retransmit selection,
// and window backpressure. Plus the unreliable newest-wins gate. NO transport,
// no host loop. Engine-free / OS-free. ADR-007.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace stalkermp::net
{
    // Wraparound-aware: is 16-bit sequence `a` strictly newer than `b`?
    [[nodiscard]] constexpr bool SequenceNewer(const std::uint16_t a, const std::uint16_t b) noexcept
    {
        const std::uint16_t diff = static_cast<std::uint16_t>(a - b);
        return diff != 0 && diff < 0x8000;
    }

    // Unreliable receive gate: accept only strictly-newer sequences (newest-wins).
    class UnreliableReceiver
    {
    public:
        [[nodiscard]] bool Accept(const std::uint16_t sequence) noexcept
        {
            if (!m_has || SequenceNewer(sequence, m_last))
            {
                m_last = sequence;
                m_has = true;
                return true;
            }
            return false; // older or equal -> discarded
        }
        void Reset() noexcept { m_has = false; m_last = 0; }

    private:
        std::uint16_t m_last = 0;
        bool m_has = false;
    };

    enum class SendResult : std::uint8_t
    {
        Sent = 0,
        WindowFull, // backpressure: reliable window at capacity
    };

    struct RetransmitPacket
    {
        std::uint16_t sequence = 0;
        std::vector<std::uint8_t> bytes;
    };

    class Reliability
    {
    public:
        Reliability(std::uint16_t window, std::uint64_t retransmitIntervalTicks)
            : m_window(window), m_interval(retransmitIntervalTicks)
        {
        }

        // Register a reliable send. Assigns the next sequence (outSequence) and
        // stores it in-flight. If the window is full -> WindowFull (nothing sent).
        [[nodiscard]] SendResult RegisterSend(std::vector<std::uint8_t> bytes, std::uint64_t sendTick,
                                              std::uint16_t& outSequence);

        // Process an inbound ack: the latest acked sequence `ack` plus 32 prior in
        // `ackBits` (bit i => sequence ack-1-i). Acked packets leave the in-flight
        // set; unknown sequences are ignored.
        void ProcessAck(std::uint16_t ack, std::uint32_t ackBits);

        // Collect in-flight packets whose retransmit interval has elapsed
        // (currentTick - sendTick >= interval); resets their sendTick so they are
        // not re-collected until another interval passes.
        [[nodiscard]] std::vector<RetransmitPacket> CollectRetransmits(std::uint64_t currentTick);

        [[nodiscard]] std::size_t InFlightCount() const noexcept { return m_inflight.size(); }
        void Clear() { m_inflight.clear(); }

        // True when the in-flight set is within the window bound.
        [[nodiscard]] bool ValidateConsistency() const noexcept { return m_inflight.size() <= m_window; }

    private:
        struct InFlight
        {
            std::uint16_t sequence = 0;
            std::uint64_t sendTick = 0;
            std::vector<std::uint8_t> bytes;
        };

        void Ack(std::uint16_t sequence);

        std::uint16_t m_nextSequence = 0;
        std::uint16_t m_window;
        std::uint64_t m_interval;
        std::vector<InFlight> m_inflight; // in send order
    };
} // namespace stalkermp::net
