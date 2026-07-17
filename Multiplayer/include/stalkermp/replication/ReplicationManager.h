// STALKER-MP — Replication manager (Sprint-009, Step 11)
//
// The umbrella IService + ITickable that drives the replication pipeline once per
// simulation tick at the reserved tick_order::kReplicationPipeline = 450 (strictly
// after Snapshot 400, before Persistence 500 / Networking 900). Each Update it
// acquires the latest immutable published snapshot from the Sprint-008
// SnapshotQueue (read-only), runs the synchronous ReplicationWorker exactly once
// (one worker per tick), and consumes the assembled packets — WITHOUT introducing
// transport (no socket, no send). It also processes inbound ReplicationAcks
// (0x0201) to advance per-client ack state via the client registry (monotonic;
// duplicate/stale acks ignored — Host Authority: an ack never mutates simulation).
//
// No thread is created, no async execution, no engine ownership beyond the frozen
// wiring point. Bootstrap wiring + the actual send are Step 12.
//
// Engine-free / OS-free. ADR-007 (no exceptions/RTTI; value outcomes); ADR-010
// (additive ids only); ADR-011 (single-threaded; no thread created).

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/FrameDispatcher.h"       // core::tick_order::kReplicationPipeline
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/net/ByteCursor.h"             // net::ByteReader
#include "stalkermp/replication/BubbleInterestPolicy.h" // IInterestPolicy
#include "stalkermp/replication/ReplicationClientRegistry.h"
#include "stalkermp/replication/ReplicationConfiguration.h"
#include "stalkermp/replication/ReplicationPacketBuilder.h" // ReplicationPacket
#include "stalkermp/replication/ReplicationWorker.h"
#include "stalkermp/snapshot/SnapshotQueue.h"      // snapshot::SnapshotQueue (read-only source)

namespace stalkermp::replication
{
    // A decoded ReplicationAck (client -> host; 0x0201). Values only.
    struct ReplicationAck
    {
        ClientId client{};
        std::uint64_t ackedReplicationId = 0;
        std::uint64_t ackedSnapshotTick = 0;
    };

    class ReplicationManager final : public core::IService, public core::ITickable
    {
    public:
        ReplicationManager(const ReplicationConfiguration& config, ReplicationClientRegistry& clients,
                           const IInterestPolicy& interest, snapshot::SnapshotQueue& snapshotSource)
            : m_config(config), m_clients(clients), m_snapshotSource(snapshotSource),
              m_worker(config, clients, interest)
        {
        }

        // --- core::IService / ISubsystem -------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Replication"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override
        {
            // Ordering-only edges: every producer must be registered earlier so the
            // manager ticks at kReplicationPipeline = 450 after them.
            return {"World", "EntityRegistry", "BubbleManager", "TransitionManager", "PlayerManager", "Networking",
                    "SnapshotManager"};
        }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable --------------------------------------------------
        // Invoked once per frame at kReplicationPipeline = 450 (wired in Step 12).
        void Tick(double deltaSeconds) override { (void)deltaSeconds; Update(); }

        // The per-tick entry: acquire the latest snapshot and run the worker once.
        void Update();

        // --- Inbound ack handling (0x0201) -----------------------------------
        // Advance a client's ack baseline. Monotonic: an ack older than or equal to
        // the current baseline is ignored (benign). Unknown client => value outcome.
        [[nodiscard]] core::Expected<void> HandleReplicationAck(const ReplicationAck& ack);
        // Parse a wire ReplicationAck payload (from the additive 0x0201 id) and apply
        // it. Malformed payload / wrong id => value outcome; state unchanged.
        [[nodiscard]] core::Expected<void> HandleReplicationAck(net::ByteReader& payload);

        // --- Read-only surface ------------------------------------------------
        [[nodiscard]] const ReplicationExecuteResult& LastResult() const noexcept { return m_lastResult; }
        [[nodiscard]] std::uint64_t Ticks() const noexcept { return m_tick; }
        [[nodiscard]] std::uint64_t AcksApplied() const noexcept { return m_acksApplied; }
        [[nodiscard]] std::uint64_t AcksIgnored() const noexcept { return m_acksIgnored; }

        static constexpr std::uint32_t TickOrder() noexcept { return core::tick_order::kReplicationPipeline; }

    private:
        ReplicationConfiguration m_config;
        ReplicationClientRegistry& m_clients;   // mutable (ack state); worker holds it const
        snapshot::SnapshotQueue& m_snapshotSource; // Sprint-008 published snapshots (read-only)
        ReplicationWorker m_worker;

        std::uint64_t m_tick = 0;
        std::uint64_t m_acksApplied = 0;
        std::uint64_t m_acksIgnored = 0;
        ReplicationExecuteResult m_lastResult; // packets consumed this tick (not transported)
    };
} // namespace stalkermp::replication
