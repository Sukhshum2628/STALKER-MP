// STALKER-MP — Reliability bookkeeping (Sprint-006, Step 9)
//
// See Reliability.h. Engine-free / OS-free; no exceptions, value results.

#include "stalkermp/net/Reliability.h"

#include <algorithm>

namespace stalkermp::net
{
    SendResult Reliability::RegisterSend(std::vector<std::uint8_t> bytes, const std::uint64_t sendTick,
                                         std::uint16_t& outSequence)
    {
        if (m_inflight.size() >= m_window)
        {
            return SendResult::WindowFull; // backpressure; nothing assigned/stored
        }
        const std::uint16_t sequence = m_nextSequence++;
        m_inflight.push_back(InFlight{sequence, sendTick, std::move(bytes)});
        outSequence = sequence;
        return SendResult::Sent;
    }

    void Reliability::Ack(const std::uint16_t sequence)
    {
        const auto it = std::find_if(m_inflight.begin(), m_inflight.end(),
                                     [sequence](const InFlight& f) { return f.sequence == sequence; });
        if (it != m_inflight.end())
        {
            m_inflight.erase(it); // known -> removed; unknown -> ignored
        }
    }

    void Reliability::ProcessAck(const std::uint16_t ack, const std::uint32_t ackBits)
    {
        Ack(ack);
        for (std::uint32_t i = 0; i < 32; ++i)
        {
            if ((ackBits & (1u << i)) != 0)
            {
                Ack(static_cast<std::uint16_t>(ack - 1 - i));
            }
        }
    }

    std::vector<RetransmitPacket> Reliability::CollectRetransmits(const std::uint64_t currentTick)
    {
        std::vector<RetransmitPacket> out;
        for (InFlight& f : m_inflight)
        {
            if (currentTick >= f.sendTick && (currentTick - f.sendTick) >= m_interval)
            {
                out.push_back(RetransmitPacket{f.sequence, f.bytes}); // copy for re-send
                f.sendTick = currentTick;                             // reset the interval
            }
        }
        return out;
    }
} // namespace stalkermp::net
