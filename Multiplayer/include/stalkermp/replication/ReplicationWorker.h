// STALKER-MP — Replication worker (Sprint-009, Step 10)
//
// The synchronous, single-threaded pipeline that turns one immutable snapshot
// into per-client replication packets. For each active client (ascending, one at
// a time) it runs the frozen pipeline:
//   1. Interest selection   (Step 5 IInterestPolicy)
//   2. Delta generation     (Step 6 DeltaEncoder, vs the client's baseline)
//   3. Classification        (Step 7 §7.A classifier, inside the queues)
//   4. Queue population      (Step 8 ReplicationQueues)
//   5. Packet assembly       (Step 9 ReplicationPacketBuilder)
// and accumulates the assembled packets into the result. It NEVER creates a
// thread, schedules, sends, touches a socket, handles an ack, registers a tick,
// mutates simulation, or accesses the engine — Execute runs entirely on the
// calling thread. Sending the produced packets is the composition root's job
// (Step 11/12). Designed to run on a future worker thread behind the snapshot
// seam (ADR-011), but Sprint-009 exercises it synchronously.
//
// Deterministic: identical registry + snapshot + baselines yield identical
// packets (byte-for-byte). Per-client entity baselines are owned here.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream; Expected<T>.

#pragma once

#include <cstdint>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/replication/BubbleInterestPolicy.h"    // IInterestPolicy
#include "stalkermp/replication/DeltaEncoder.h"
#include "stalkermp/replication/ReplicationClientRegistry.h"
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationPacketBuilder.h"
#include "stalkermp/replication/ReplicationQueues.h"
#include "stalkermp/replication/ReplicationTypes.h"
#include "stalkermp/snapshot/ISnapshotView.h"

namespace stalkermp::replication
{
    // The outcome of one Execute pass. Values only.
    struct ReplicationExecuteResult
    {
        std::uint32_t clientsProcessed = 0;
        std::uint32_t reliablePackets = 0;   // non-empty reliable packets produced
        std::uint32_t unreliablePackets = 0; // non-empty unreliable packets produced
        std::uint64_t recordsQueued = 0;     // total change records routed to queues
        std::uint64_t bytesAssembled = 0;    // total assembled packet bytes
        std::vector<ReplicationPacket> packets; // ready for the transport step (not sent here)
    };

    class ReplicationWorker
    {
    public:
        ReplicationWorker(const ReplicationConfiguration& config, const ReplicationClientRegistry& clients,
                          const IInterestPolicy& interest)
            : m_config(config), m_clients(clients), m_interest(interest), m_queues(config), m_packetBuilder(config)
        {
        }

        // Synchronous lifecycle hooks (no thread is created — future-thread
        // readiness only). StartWorker/StopWorker are inert; FlushQueues clears
        // the internal scratch queues.
        void StartWorker() noexcept { m_running = true; }
        void StopWorker() noexcept { m_running = false; }
        void FlushQueues() noexcept { m_queues.Clear(); }

        // Run the full pipeline for every active client against `snapshot`,
        // producing per-client packets. Synchronous; deterministic; returns a
        // value outcome. Does not send.
        [[nodiscard]] core::Expected<ReplicationExecuteResult> Execute(const snapshot::ISnapshotView& snapshot);

        [[nodiscard]] bool Running() const noexcept { return m_running; }

    private:
        struct Baseline
        {
            std::uint64_t clientId = 0;
            std::vector<EntityReplicationState> entities; // last-sent entity states
        };

        [[nodiscard]] std::vector<EntityReplicationState>& BaselineFor(ClientId id);

        ReplicationConfiguration m_config;
        const ReplicationClientRegistry& m_clients; // read-only
        const IInterestPolicy& m_interest;          // read-only
        ReplicationQueues m_queues;                 // internal scratch (cleared per client)
        ReplicationPacketBuilder m_packetBuilder;
        std::vector<Baseline> m_baselines;          // per-client, owned here
        bool m_running = false;
    };
} // namespace stalkermp::replication
