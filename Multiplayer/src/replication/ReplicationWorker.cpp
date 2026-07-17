// STALKER-MP — Replication worker (Sprint-009, Step 10)
//
// See ReplicationWorker.h. Synchronous per-client pipeline: interest -> delta ->
// classify/queue -> packet. Deterministic; value outcomes; no send, no thread,
// no engine. Engine-free / OS-free.

#include "stalkermp/replication/ReplicationWorker.h"

namespace stalkermp::replication
{
    std::vector<EntityReplicationState>& ReplicationWorker::BaselineFor(const ClientId id)
    {
        for (Baseline& b : m_baselines)
        {
            if (b.clientId == id.value)
            {
                return b.entities;
            }
        }
        m_baselines.push_back(Baseline{id.value, {}});
        return m_baselines.back().entities;
    }

    namespace
    {
        // Build the current relevant entity states (ascending, values only) by
        // intersecting the ascending relevant ids with the ascending snapshot
        // entities. Both inputs are ascending-unique, so a two-pointer merge is
        // deterministic and O(n).
        [[nodiscard]] std::vector<EntityReplicationState>
        BuildCurrentStates(const std::vector<world::EntityId>& relevant, const snapshot::ISnapshotView& snapshot)
        {
            std::vector<EntityReplicationState> current;
            current.reserve(relevant.size());

            const std::vector<snapshot::EntitySnapshot>& entities = snapshot.Entities();
            std::size_t ri = 0;
            std::size_t ei = 0;
            while (ri < relevant.size() && ei < entities.size())
            {
                const std::uint32_t rId = relevant[ri].value;
                const std::uint32_t eId = entities[ei].id.value;
                if (rId < eId)
                {
                    ++ri; // relevant id not in the snapshot (defensive) -> skip
                }
                else if (eId < rId)
                {
                    ++ei;
                }
                else
                {
                    const snapshot::EntitySnapshot& e = entities[ei];
                    EntityReplicationState s{};
                    s.id = e.id;
                    s.position = e.position;
                    s.velocity = e.velocity;
                    s.stateFlags = e.state; // opaque runtime state
                    s.version = 0;          // versioning is assigned by the delta encoder
                    current.push_back(s);
                    ++ri;
                    ++ei;
                }
            }
            return current;
        }
    } // namespace

    core::Expected<ReplicationExecuteResult> ReplicationWorker::Execute(const snapshot::ISnapshotView& snapshot)
    {
        ReplicationExecuteResult result;
        const std::uint64_t tick = snapshot.Metadata().simulationTick;

        // Active clients are ascending by ClientId (deterministic, one at a time).
        const std::vector<ClientRecord> clients = m_clients.ActiveClients();
        for (const ClientRecord& client : clients)
        {
            // 1. Interest selection (ascending, unique, append-only).
            std::vector<world::EntityId> relevant;
            m_interest.SelectRelevant(client, snapshot, relevant);

            // 2. Delta generation vs this client's baseline.
            const std::vector<EntityReplicationState> current = BuildCurrentStates(relevant, snapshot);
            std::vector<EntityReplicationState>& baseline = BaselineFor(client.id);
            auto delta = EncodeEntityDelta(baseline, current, m_config.maxEntitiesPerUpdate);
            if (!delta)
            {
                // Cap exceeded for this client: skip its update this pass (the
                // baseline is unchanged; the client catches up next pass). Value
                // outcome, not a throw; other clients still process.
                ++result.clientsProcessed;
                continue;
            }
            const ReplicationChangeSet& changes = delta.Value();
            result.recordsQueued +=
                changes.added.size() + changes.changed.size() + changes.removed.size();

            // 3./4. Classify + populate the (scratch) queues. Queue overflow is a
            // benign shaping outcome — packets are built from what fit.
            m_queues.Clear();
            (void)m_queues.EnqueueChangeSet(changes);

            // 5. Assemble one reliable and one unreliable packet per client.
            if (auto reliable = m_packetBuilder.BuildPacket(m_queues.Reliable(),
                                                            ReplicationReliability::Reliable, client.id, tick))
            {
                if (!reliable.Value().Empty())
                {
                    result.bytesAssembled += reliable.Value().PacketSize();
                    ++result.reliablePackets;
                    result.packets.push_back(std::move(reliable).Value());
                }
            }
            if (auto unreliable = m_packetBuilder.BuildPacket(m_queues.Unreliable(),
                                                             ReplicationReliability::Unreliable, client.id, tick))
            {
                if (!unreliable.Value().Empty())
                {
                    result.bytesAssembled += unreliable.Value().PacketSize();
                    ++result.unreliablePackets;
                    result.packets.push_back(std::move(unreliable).Value());
                }
            }

            // Advance the client's baseline to the current set (versioned).
            baseline = NextEntityBaseline(baseline, current);
            ++result.clientsProcessed;
        }

        return result;
    }
} // namespace stalkermp::replication
