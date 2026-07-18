// STALKER-MP — Remote-entity interpolation manager (Sprint-010, Step 9)
//
// See InterpolationManager.h. Deterministic, append-only, ascending-unique remote
// state production via SnapshotBuffer::Pair + InterpolationStep. Engine-free /
// OS-free; no exceptions, no RTTI, no iostream; value outcomes.

#include "stalkermp/prediction/InterpolationManager.h"

#include "stalkermp/prediction/InterpolationStep.h" // Interpolate(entity, entity, factor)

namespace stalkermp::prediction
{
    PredictionOutcome InterpolationManager::Interpolate(const std::uint64_t renderTick,
                                                        std::vector<InterpolatedState>& out)
    {
        const SnapshotPair pair = m_snapshots.Pair(renderTick);
        if (!pair.valid || pair.from == nullptr || pair.to == nullptr)
        {
            return PredictionOutcome::NoAuthoritativeFrame; // nothing to interpolate
        }

        const std::vector<replication::EntityReplicationState>& fromEntities = pair.from->entities;
        const std::vector<replication::EntityReplicationState>& toEntities = pair.to->entities;

        // Merge-walk two EntityId-ascending lists; emit only entities present in BOTH
        // frames (no extrapolation of existence). Output stays ascending and unique.
        std::size_t i = 0;
        std::size_t j = 0;
        while (i < fromEntities.size() && j < toEntities.size())
        {
            const replication::EntityReplicationState& a = fromEntities[i];
            const replication::EntityReplicationState& b = toEntities[j];
            if (a.id.value == b.id.value)
            {
                out.push_back(prediction::Interpolate(a, b, pair.factor));
                ++i;
                ++j;
            }
            else if (a.id.value < b.id.value)
            {
                ++i; // only in `from` -> skip
            }
            else
            {
                ++j; // only in `to` -> skip
            }
        }

        return PredictionOutcome::Ok;
    }
} // namespace stalkermp::prediction
