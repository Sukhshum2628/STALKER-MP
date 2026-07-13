// STALKER-MP — Fragmentation / reassembly (Sprint-006, Step 9)
//
// See Fragmentation.h. Engine-free / OS-free; no exceptions, core::Expected.

#include "stalkermp/net/Fragmentation.h"

#include <algorithm>

namespace stalkermp::net
{
    core::Expected<void> SerializeFragmentHeader(const Fragment& fragment, ByteWriter& writer)
    {
        if (auto r = writer.WriteU16(fragment.messageId.value); !r) { return r; }
        if (auto r = writer.WriteU16(fragment.index); !r) { return r; }
        if (auto r = writer.WriteU16(fragment.count); !r) { return r; }
        return core::Success();
    }

    core::Expected<Fragment> DeserializeFragmentHeader(ByteReader& reader)
    {
        Fragment f;
        auto msg = reader.ReadU16();
        if (!msg) { return msg.GetError(); }
        f.messageId = MessageId{msg.Value()};
        auto index = reader.ReadU16();
        if (!index) { return index.GetError(); }
        f.index = index.Value();
        auto count = reader.ReadU16();
        if (!count) { return count.GetError(); }
        f.count = count.Value();
        return f;
    }

    std::vector<Fragment> Fragmentation::Split(const MessageId messageId,
                                               const std::vector<std::uint8_t>& payload) const
    {
        std::vector<Fragment> fragments;
        const std::size_t total = payload.size();
        const std::size_t count = total == 0 ? 1 : (total + m_maxFragmentPayload - 1) / m_maxFragmentPayload;

        for (std::size_t i = 0; i < count; ++i)
        {
            const std::size_t begin = i * m_maxFragmentPayload;
            const std::size_t end = std::min(begin + m_maxFragmentPayload, total);
            Fragment f;
            f.messageId = messageId;
            f.index = static_cast<std::uint16_t>(i);
            f.count = static_cast<std::uint16_t>(count);
            if (end > begin)
            {
                f.bytes.assign(payload.begin() + static_cast<std::ptrdiff_t>(begin),
                               payload.begin() + static_cast<std::ptrdiff_t>(end));
            }
            fragments.push_back(std::move(f));
        }
        return fragments;
    }

    AcceptResult Fragmentation::Accept(const ConnectionId connection, const Fragment& fragment,
                                       const std::uint64_t currentTick, CompletedMessage& out)
    {
        if (fragment.count == 0 || fragment.index >= fragment.count)
        {
            return AcceptResult::Rejected;
        }

        // Single-fragment message completes immediately.
        const std::uint64_t key = Key(connection, fragment.messageId);
        auto it = m_pending.find(key);
        if (it == m_pending.end())
        {
            Reassembly r;
            r.count = fragment.count;
            r.firstTick = currentTick;
            r.parts.resize(fragment.count);
            it = m_pending.emplace(key, std::move(r)).first;
        }
        else if (it->second.count != fragment.count)
        {
            return AcceptResult::Rejected; // conflicting fragment count for the same message
        }

        Reassembly& r = it->second;
        if (!r.parts[fragment.index].has_value())
        {
            r.parts[fragment.index] = fragment.bytes; // owned copy
            ++r.have;
        }
        // else: duplicate fragment -> idempotent (ignored)

        if (r.have == r.count)
        {
            out.messageId = fragment.messageId;
            out.bytes.clear();
            std::size_t totalPayloadSize = 0;
            for (const auto& part : r.parts)
            {
                totalPayloadSize += part->size();
            }
            out.bytes.resize(totalPayloadSize);
            std::uint8_t* dst = out.bytes.data();
            for (const auto& part : r.parts)
            {
                if (const std::size_t sz = part->size())
                {
                    std::copy(part->begin(), part->end(), dst);
                    dst += sz;
                }
            }
            m_pending.erase(it);
            return AcceptResult::Completed;
        }
        return AcceptResult::Incomplete;
    }

    std::size_t Fragmentation::PruneExpired(const std::uint64_t currentTick)
    {
        std::size_t dropped = 0;
        for (auto it = m_pending.begin(); it != m_pending.end();)
        {
            if (currentTick >= it->second.firstTick && (currentTick - it->second.firstTick) > m_budgetTicks)
            {
                it = m_pending.erase(it);
                ++dropped;
                ++m_droppedIncomplete;
            }
            else
            {
                ++it;
            }
        }
        return dropped;
    }

    bool Fragmentation::ValidateConsistency(const std::uint64_t currentTick) const
    {
        for (const auto& entry : m_pending)
        {
            if (currentTick >= entry.second.firstTick && (currentTick - entry.second.firstTick) > m_budgetTicks)
            {
                return false; // an orphan buffer past budget (not yet pruned)
            }
        }
        return true;
    }
} // namespace stalkermp::net
