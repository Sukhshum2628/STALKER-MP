// STALKER-MP — Player registry storage + lookups (Sprint-007, Steps 3–4)
//
// See PlayerRegistry.h. Step 3: ascending non-reused allocation, sorted-unique
// insert, capacity enforcement, retire. Step 4: read-only secondary lookups over
// hash accelerators (rebuilt on each mutation) + ValidateConsistency. No lookups
// mutate; no lifecycle. Engine-free / OS-free; no exceptions, Expected<T>.

#include "stalkermp/player/PlayerRegistry.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::player
{
    PlayerMappingView PlayerRegistry::MakeView(const PlayerRecord& r) noexcept
    {
        PlayerMappingView v{};
        v.id = r.id;
        v.connection = r.connection;
        v.sessionMember = r.sessionMember;
        v.entity = r.entity;
        return v;
    }

    std::size_t PlayerRegistry::LowerBoundIndex(const PlayerId id) const noexcept
    {
        std::size_t lo = 0;
        std::size_t hi = m_records.size();
        while (lo < hi)
        {
            const std::size_t mid = lo + (hi - lo) / 2;
            if (m_records[mid].id.value < id.value)
            {
                lo = mid + 1;
            }
            else
            {
                hi = mid;
            }
        }
        return lo;
    }

    void PlayerRegistry::RebuildAccelerators()
    {
        m_byPlayerId.clear();
        m_byConnection.clear();
        m_bySession.clear();
        m_byEntity.clear();
        for (std::size_t i = 0; i < m_records.size(); ++i)
        {
            const PlayerRecord& r = m_records[i];
            m_byPlayerId[r.id.value] = i; // id != 0 guaranteed by Insert
            if (r.connection.value != 0)
            {
                m_byConnection[r.connection.value] = i;
            }
            if (r.sessionMember != 0)
            {
                m_bySession[r.sessionMember] = i;
            }
            if (r.entity.value != 0)
            {
                m_byEntity[r.entity.value] = i;
            }
        }
    }

    PlayerId PlayerRegistry::Allocate() noexcept
    {
        const PlayerId id{m_nextId};
        ++m_nextId; // ascending, non-reused
        return id;
    }

    core::Expected<void> PlayerRegistry::Insert(const PlayerRecord& record)
    {
        if (record.id.value == 0)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "PlayerRegistry: cannot insert id 0 (none)");
        }
        if (m_records.size() >= m_capacity)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("PlayerRegistry: at capacity ({})", m_capacity));
        }

        const std::size_t idx = LowerBoundIndex(record.id);
        if (idx < m_records.size() && m_records[idx].id.value == record.id.value)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument,
                                   common::Format("PlayerRegistry: duplicate PlayerId {}", record.id.value));
        }

        m_records.insert(m_records.begin() + static_cast<std::ptrdiff_t>(idx), record);
        RebuildAccelerators();
        return core::Success();
    }

    void PlayerRegistry::Retire(const PlayerId id) noexcept
    {
        if (id.value == 0)
        {
            return;
        }
        const std::size_t idx = LowerBoundIndex(id);
        if (idx < m_records.size() && m_records[idx].id.value == id.value)
        {
            m_records.erase(m_records.begin() + static_cast<std::ptrdiff_t>(idx));
            RebuildAccelerators();
        }
    }

    std::optional<PlayerMappingView> PlayerRegistry::FindByPlayerId(const PlayerId id) const
    {
        if (id.value == 0)
        {
            return std::nullopt;
        }
        const auto it = m_byPlayerId.find(id.value);
        if (it == m_byPlayerId.end())
        {
            return std::nullopt;
        }
        return MakeView(m_records[it->second]);
    }

    std::optional<PlayerMappingView> PlayerRegistry::FindByConnection(const net::ConnectionId connection) const
    {
        if (connection.value == 0)
        {
            return std::nullopt;
        }
        const auto it = m_byConnection.find(connection.value);
        if (it == m_byConnection.end())
        {
            return std::nullopt;
        }
        return MakeView(m_records[it->second]);
    }

    std::optional<PlayerMappingView> PlayerRegistry::FindBySession(const std::uint64_t sessionMember) const
    {
        if (sessionMember == 0)
        {
            return std::nullopt;
        }
        const auto it = m_bySession.find(sessionMember);
        if (it == m_bySession.end())
        {
            return std::nullopt;
        }
        return MakeView(m_records[it->second]);
    }

    std::optional<PlayerMappingView> PlayerRegistry::FindByEntity(const world::EntityId entity) const
    {
        if (entity.value == 0)
        {
            return std::nullopt;
        }
        const auto it = m_byEntity.find(entity.value);
        if (it == m_byEntity.end())
        {
            return std::nullopt;
        }
        return MakeView(m_records[it->second]);
    }

    PlayerRegistryConsistency PlayerRegistry::ValidateConsistency() const
    {
        PlayerRegistryConsistency report;

        // Capacity.
        report.withinCapacity = m_records.size() <= m_capacity;

        // Ordering, uniqueness, no-zero-id.
        for (std::size_t i = 0; i < m_records.size(); ++i)
        {
            if (m_records[i].id.value == 0)
            {
                report.noZeroId = false;
            }
            if (i > 0 && m_records[i - 1].id.value >= m_records[i].id.value)
            {
                report.sortedUnique = false;
            }
        }

        // Accelerator agreement: each accelerator entry points at a record whose
        // corresponding key matches. Sizes must equal the number of live keys.
        std::size_t liveConnections = 0;
        std::size_t liveSessions = 0;
        std::size_t liveEntities = 0;
        for (std::size_t i = 0; i < m_records.size(); ++i)
        {
            const PlayerRecord& r = m_records[i];

            const auto pit = m_byPlayerId.find(r.id.value);
            if (pit == m_byPlayerId.end() || pit->second != i)
            {
                report.acceleratorsConsistent = false;
            }
            if (r.connection.value != 0)
            {
                ++liveConnections;
                const auto cit = m_byConnection.find(r.connection.value);
                if (cit == m_byConnection.end() || cit->second != i)
                {
                    report.acceleratorsConsistent = false;
                }
            }
            if (r.sessionMember != 0)
            {
                ++liveSessions;
                const auto sit = m_bySession.find(r.sessionMember);
                if (sit == m_bySession.end() || sit->second != i)
                {
                    report.acceleratorsConsistent = false;
                }
            }
            if (r.entity.value != 0)
            {
                ++liveEntities;
                const auto eit = m_byEntity.find(r.entity.value);
                if (eit == m_byEntity.end() || eit->second != i)
                {
                    report.acceleratorsConsistent = false;
                }
            }
        }
        if (m_byPlayerId.size() != m_records.size())
        {
            report.acceleratorsConsistent = false;
        }

        // Bijection: each live secondary key maps to at most one record. If the
        // accelerator has fewer entries than live keys, two records shared a key.
        if (m_byConnection.size() != liveConnections || m_bySession.size() != liveSessions ||
            m_byEntity.size() != liveEntities)
        {
            report.mappingBijective = false;
        }

        return report;
    }
} // namespace stalkermp::player
