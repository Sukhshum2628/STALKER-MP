// STALKER-MP — Replication queues (Sprint-009, Step 8)
//
// See ReplicationQueues.h. Deterministic change-set routing using the frozen
// §7.A classifier; per-queue capacity budgets with Overflow value outcomes.
// Engine-free / OS-free; no allocation during queue operations.

#include "stalkermp/replication/ReplicationQueues.h"

namespace stalkermp::replication
{
    QueuedRecord ReplicationQueues::MakeRecord(const ReplicationChangeKind kind, const EntityReplicationState& entity)
    {
        QueuedRecord record{};
        record.kind = kind;
        record.entity = entity;
        record.reliability = ClassifyReliability(kind); // §7.A
        record.priority = ClassifyPriority(kind);       // §7.A
        return record;
    }

    ReplicationOutcome ReplicationQueues::EnqueueChangeSet(const ReplicationChangeSet& changes) noexcept
    {
        ReplicationOutcome outcome = ReplicationOutcome::Ok;

        // Deterministic enqueue order: removed, added, changed.
        // Category -> §7.A kind:
        //   removed -> EntityRemove (reliable),
        //   added   -> EntitySpawn  (reliable),
        //   changed -> Position     (unreliable movement; representative delta kind
        //              — richer per-field kinds are assigned by the worker later).
        for (const world::EntityId id : changes.removed)
        {
            EntityReplicationState e{};
            e.id = id; // removal carries the id only
            if (const auto r = Enqueue(MakeRecord(ReplicationChangeKind::EntityRemove, e));
                r != ReplicationOutcome::Ok && outcome == ReplicationOutcome::Ok)
            {
                outcome = r;
            }
        }
        for (const EntityReplicationState& e : changes.added)
        {
            if (const auto r = Enqueue(MakeRecord(ReplicationChangeKind::EntitySpawn, e));
                r != ReplicationOutcome::Ok && outcome == ReplicationOutcome::Ok)
            {
                outcome = r;
            }
        }
        for (const EntityReplicationState& e : changes.changed)
        {
            if (const auto r = Enqueue(MakeRecord(ReplicationChangeKind::Position, e));
                r != ReplicationOutcome::Ok && outcome == ReplicationOutcome::Ok)
            {
                outcome = r;
            }
        }
        return outcome;
    }
} // namespace stalkermp::replication
