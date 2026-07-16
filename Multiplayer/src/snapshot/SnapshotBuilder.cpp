// STALKER-MP — Snapshot builder (Sprint-008, Step 6)
//
// See SnapshotBuilder.h. Deterministic, value-only capture into a pooled buffer;
// PoolExhausted / Overflow as value outcomes; buffer returned to the pool on a
// failed Capture. Engine-free / OS-free.

#include "stalkermp/snapshot/SnapshotBuilder.h"

#include <string>
#include <vector>

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::snapshot
{
    namespace
    {
        // Deterministic 32-bit FNV-1a over the (engine-free) weather profile name,
        // so a snapshot carries a stable integer weather id (no engine string in
        // the value-only snapshot; replay-stable).
        [[nodiscard]] std::uint32_t WeatherId(const std::string& name) noexcept
        {
            std::uint32_t hash = 2166136261u;
            for (const char c : name)
            {
                hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
                hash *= 16777619u;
            }
            return hash;
        }
    } // namespace

    void SnapshotBuilder::AbortToPool()
    {
        if (m_pool != nullptr && m_current != nullptr)
        {
            m_pool->ReturnToPool(m_current);
        }
        m_current = nullptr;
        m_pool = nullptr;
    }

    core::Expected<SimulationSnapshot*> SnapshotBuilder::BeginBuild(SnapshotPool& pool, const std::uint64_t tick)
    {
        // A prior in-progress build (if any) is abandoned back to its pool.
        AbortToPool();

        auto acquired = pool.Acquire();
        if (!acquired.HasValue())
        {
            return acquired.GetError(); // PoolExhausted (value outcome)
        }
        m_pool = &pool;
        m_current = acquired.Value();
        m_lastId = SnapshotId{m_nextId++}; // monotonic, never 0
        m_current->BeginBuild(m_lastId, tick, m_config.version);
        return m_current;
    }

    core::Expected<void> SnapshotBuilder::Capture(const world::IEntitySnapshotSource& entitySource,
                                                  const player::PlayerManager& playerSurface,
                                                  const world::IEnvironmentSource& environmentSource)
    {
        if (m_current == nullptr)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "SnapshotBuilder: Capture with no active build");
        }

        // --- Entities (value data only, ascending EntityId) ------------------
        std::vector<EntitySnapshot> entities;
        entitySource.Capture(entities);
        if (entities.size() > m_config.maxEntities)
        {
            AbortToPool();
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("SnapshotBuilder: entity Overflow ({} > {})", entities.size(), m_config.maxEntities));
        }
        for (const EntitySnapshot& entity : entities)
        {
            if (auto added = m_current->AddEntity(entity); !added)
            {
                AbortToPool();
                return added.GetError(); // non-ascending/duplicate/none from a bad source
            }
        }

        // --- Players (Sprint-007 read-only surface, ascending PlayerId) ------
        std::uint32_t playerCount = 0;
        for (const player::PlayerRecord& r : playerSurface.Registry().Records())
        {
            if (r.lifecycle == player::PlayerLifecycleState::Removed)
            {
                continue; // logically gone; not captured
            }
            if (playerCount >= m_config.maxPlayers)
            {
                AbortToPool();
                return core::MakeError(
                    core::ErrorCode::InvalidArgument,
                    common::Format("SnapshotBuilder: player Overflow (> {})", m_config.maxPlayers));
            }
            PlayerSnapshot p{};
            p.id = r.id;
            p.entity = r.entity;
            p.connectionState = r.connectionState;
            p.position = r.lastPosition.position;
            p.simulationState = static_cast<std::uint32_t>(r.lifecycle); // opaque lifecycle encoding
            p.authorityFlags = 0;                                        // reserved (opaque)
            if (auto added = m_current->AddPlayer(p); !added)
            {
                AbortToPool();
                return added.GetError();
            }
            ++playerCount;
        }

        // --- Environment (Sprint-002 IEnvironmentSource, value copy) ---------
        EnvironmentSnapshot env{};
        env.simulationTick = m_current->Metadata().simulationTick;
        if (const auto sample = environmentSource.Sample())
        {
            env.weatherId = WeatherId(sample->weatherName);
            env.emissionState = sample->emissionActive ? 1u : 0u;
        }
        if (auto set = m_current->SetEnvironment(env); !set)
        {
            AbortToPool();
            return set.GetError();
        }

        return core::Success();
    }

    core::Expected<const SimulationSnapshot*> SnapshotBuilder::Finalize()
    {
        if (m_current == nullptr)
        {
            return core::MakeError(core::ErrorCode::InvalidArgument, "SnapshotBuilder: Finalize with no active build");
        }
        if (auto sealed = m_current->Finalize(); !sealed)
        {
            AbortToPool();
            return sealed.GetError();
        }
        const SimulationSnapshot* const finalized = m_current;
        m_current = nullptr; // build complete; the pool/queue owns the buffer now
        m_pool = nullptr;
        return finalized;
    }
} // namespace stalkermp::snapshot
