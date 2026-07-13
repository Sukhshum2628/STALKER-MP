// STALKER-MP — Session (Sprint-006, Step 8)
//
// See Session.h. Engine-free / OS-free; no exceptions, core::Expected.
// Ascending members; sorted vector + hash accelerator (BC-005); observers fire
// exactly once in registration order.

#include "stalkermp/net/Session.h"

#include <algorithm>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::net
{
    void Session::Subscribe(ISessionObserver* const observer)
    {
        if (observer != nullptr)
        {
            m_observers.push_back(observer);
        }
    }

    core::Expected<void> Session::Admit(const ConnectionId id, const std::uint64_t joinTick)
    {
        if (m_members.size() >= m_maxMembers)
        {
            // No dedicated capacity ErrorCode; the call site maps to CapacityFull.
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("session at capacity ({})", m_maxMembers));
        }

        const auto pos = std::lower_bound(
            m_members.begin(), m_members.end(), id.value,
            [](const SessionMember& m, const std::uint32_t v) { return m.id.value < v; });
        if (pos != m_members.end() && pos->id.value == id.value)
        {
            return core::MakeError(core::ErrorCode::AlreadyExists,
                                   common::Format("connection {} already a session member", id.value));
        }

        SessionMember member;
        member.id = id;
        member.joinTick = joinTick;
        m_members.insert(pos, member);
        RebuildAccelerator();

        for (ISessionObserver* const obs : m_observers)
        {
            obs->OnMemberJoined(id, joinTick);
        }
        return core::Success();
    }

    void Session::Remove(const ConnectionId id, const DisconnectReason reason)
    {
        const auto it = m_index.find(id.value);
        if (it == m_index.end())
        {
            return; // absent -> no-op, no spurious OnMemberLeft
        }
        const SessionMember member = m_members[it->second];
        const std::uint64_t token = MintToken(member.id, member.joinTick);

        m_members.erase(m_members.begin() + static_cast<std::ptrdiff_t>(it->second));
        RebuildAccelerator();

        for (ISessionObserver* const obs : m_observers)
        {
            obs->OnMemberLeft(id, reason, token);
        }
    }

    core::Expected<ConnectionId> Session::TryReclaim(const std::uint64_t /*reconnectToken*/)
    {
        // Reconnect reclaim is Sprint-012; the seam is reserved and unsupported here.
        return core::MakeError(core::ErrorCode::NotFound, "session reclaim not supported in Sprint-006");
    }

    bool Session::Contains(const ConnectionId id) const
    {
        return m_index.find(id.value) != m_index.end();
    }

    SessionReport Session::ValidateConsistency() const
    {
        SessionReport report;
        for (std::size_t i = 0; i < m_members.size(); ++i)
        {
            if (m_members[i].id.value == 0)
            {
                report.noZeroId = false;
            }
            if (i > 0 && !(m_members[i - 1].id.value < m_members[i].id.value))
            {
                report.sortedUnique = false;
            }
        }
        if (m_index.size() != m_members.size())
        {
            report.acceleratorConsistent = false;
        }
        else
        {
            for (std::size_t i = 0; i < m_members.size(); ++i)
            {
                const auto it = m_index.find(m_members[i].id.value);
                if (it == m_index.end() || it->second != i)
                {
                    report.acceleratorConsistent = false;
                    break;
                }
            }
        }
        if (m_members.size() > m_maxMembers)
        {
            report.withinCapacity = false;
        }
        return report;
    }

    void Session::RebuildAccelerator()
    {
        m_index.clear();
        m_index.reserve(m_members.size());
        for (std::size_t i = 0; i < m_members.size(); ++i)
        {
            m_index.emplace(m_members[i].id.value, i);
        }
    }

    std::uint64_t Session::MintToken(const ConnectionId id, const std::uint64_t joinTick) noexcept
    {
        // Deterministic, opaque token. Never 0.
        std::uint64_t t = (static_cast<std::uint64_t>(id.value) << 32) ^ (joinTick * 2654435761ull);
        t ^= 0x5350AA55A55A5350ull;
        return t == 0 ? 1ull : t;
    }
} // namespace stalkermp::net
