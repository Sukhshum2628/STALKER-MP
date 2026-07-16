// STALKER-MP — Replication delta generation (Sprint-009, Step 6)
//
// See DeltaEncoder.h. Pure, deterministic two-pointer merge over ascending-unique
// inputs; value-in/value-out; Overflow value outcome. Engine-free / OS-free.

#include "stalkermp/replication/DeltaEncoder.h"

#include "stalkermp/common/StringUtil.h" // common::Format

namespace stalkermp::replication
{
    namespace
    {
        [[nodiscard]] bool Vec3Equal(const world::Vec3& a, const world::Vec3& b) noexcept
        {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }

        // Tracked replicated fields for entity change detection (not version).
        [[nodiscard]] bool EntityTrackedEqual(const EntityReplicationState& a,
                                              const EntityReplicationState& b) noexcept
        {
            return Vec3Equal(a.position, b.position) && Vec3Equal(a.velocity, b.velocity) &&
                   a.stateFlags == b.stateFlags;
        }

        // Tracked replicated fields for player change detection.
        [[nodiscard]] bool PlayerTrackedEqual(const PlayerReplicationState& a,
                                              const PlayerReplicationState& b) noexcept
        {
            return Vec3Equal(a.position, b.position) && a.connectionState == b.connectionState &&
                   a.authorityFlags == b.authorityFlags;
        }
    } // namespace

    core::Expected<ReplicationChangeSet>
    EncodeEntityDelta(const std::vector<EntityReplicationState>& baseline,
                      const std::vector<EntityReplicationState>& current, const std::uint32_t maxEntitiesPerUpdate)
    {
        ReplicationChangeSet result;

        // Two-pointer merge over ascending-unique EntityId.
        std::size_t bi = 0;
        std::size_t ci = 0;
        while (bi < baseline.size() && ci < current.size())
        {
            const std::uint32_t bId = baseline[bi].id.value;
            const std::uint32_t cId = current[ci].id.value;
            if (cId < bId) // present in current, absent in baseline -> added
            {
                EntityReplicationState e = current[ci];
                e.version = 1;
                result.added.push_back(e);
                ++ci;
            }
            else if (bId < cId) // present in baseline, absent in current -> removed
            {
                result.removed.push_back(baseline[bi].id);
                ++bi;
            }
            else // present in both
            {
                if (!EntityTrackedEqual(baseline[bi], current[ci]))
                {
                    EntityReplicationState e = current[ci];
                    e.version = baseline[bi].version + 1; // bump on change
                    result.changed.push_back(e);
                }
                // unchanged => omitted
                ++bi;
                ++ci;
            }
        }
        for (; ci < current.size(); ++ci) // remaining current -> added
        {
            EntityReplicationState e = current[ci];
            e.version = 1;
            result.added.push_back(e);
        }
        for (; bi < baseline.size(); ++bi) // remaining baseline -> removed
        {
            result.removed.push_back(baseline[bi].id);
        }

        const std::size_t emitted = result.added.size() + result.changed.size();
        if (emitted > maxEntitiesPerUpdate)
        {
            return core::MakeError(
                core::ErrorCode::InvalidArgument,
                common::Format("DeltaEncoder: entity Overflow ({} > {})", emitted, maxEntitiesPerUpdate));
        }
        return result;
    }

    std::vector<PlayerReplicationState> EncodePlayerDelta(const std::vector<PlayerReplicationState>& baseline,
                                                          const std::vector<PlayerReplicationState>& current)
    {
        std::vector<PlayerReplicationState> changed;

        std::size_t bi = 0;
        std::size_t ci = 0;
        while (bi < baseline.size() && ci < current.size())
        {
            const std::uint32_t bId = baseline[bi].id.value;
            const std::uint32_t cId = current[ci].id.value;
            if (cId < bId) // added
            {
                PlayerReplicationState p = current[ci];
                p.version = 1;
                changed.push_back(p);
                ++ci;
            }
            else if (bId < cId) // removed player -> not part of playersChanged
            {
                ++bi;
            }
            else
            {
                if (!PlayerTrackedEqual(baseline[bi], current[ci]))
                {
                    PlayerReplicationState p = current[ci];
                    p.version = baseline[bi].version + 1;
                    changed.push_back(p);
                }
                ++bi;
                ++ci;
            }
        }
        for (; ci < current.size(); ++ci) // remaining current -> added
        {
            PlayerReplicationState p = current[ci];
            p.version = 1;
            changed.push_back(p);
        }
        return changed;
    }

    std::vector<EntityReplicationState> NextEntityBaseline(const std::vector<EntityReplicationState>& baseline,
                                                           const std::vector<EntityReplicationState>& current)
    {
        std::vector<EntityReplicationState> next;
        next.reserve(current.size());

        std::size_t bi = 0;
        std::size_t ci = 0;
        while (bi < baseline.size() && ci < current.size())
        {
            const std::uint32_t bId = baseline[bi].id.value;
            const std::uint32_t cId = current[ci].id.value;
            if (cId < bId) // added -> version 1
            {
                EntityReplicationState e = current[ci];
                e.version = 1;
                next.push_back(e);
                ++ci;
            }
            else if (bId < cId) // removed -> dropped from baseline
            {
                ++bi;
            }
            else
            {
                EntityReplicationState e = current[ci];
                e.version = EntityTrackedEqual(baseline[bi], current[ci]) ? baseline[bi].version
                                                                          : baseline[bi].version + 1;
                next.push_back(e);
                ++bi;
                ++ci;
            }
        }
        for (; ci < current.size(); ++ci) // remaining current -> added
        {
            EntityReplicationState e = current[ci];
            e.version = 1;
            next.push_back(e);
        }
        return next;
    }
} // namespace stalkermp::replication
