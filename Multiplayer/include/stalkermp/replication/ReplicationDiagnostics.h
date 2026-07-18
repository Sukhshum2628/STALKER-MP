// STALKER-MP — Replication diagnostics (Sprint-009, Step 13)
//
// A pure, non-invasive diagnostic COLLECTOR. It accumulates monotonic counters
// from the Step-10 worker results and the Step-11 acknowledgement events, and
// returns an immutable value snapshot on demand. It NEVER influences replication
// behavior: it holds no reference to the pipeline, mutates nothing but its own
// counters, produces no output, and does no networking/transport/scheduling/
// threading/simulation/engine work. Deterministic; Reset restores the initial
// state.
//
// Consumes only:
//   - RecordTick(const ReplicationExecuteResult&)  (Step 10 worker result)
//   - RecordPacket(reliability, byteCount)         (per assembled packet)
//   - RecordAck(applied)                           (Step 11 ack event)
//   - RecordOverflow()                             (a bounded-memory overflow)
//
// Snapshot() returns replication::ReplicationStatistics (the immutable value).
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/replication/ReplicationTypes.h"
#include "stalkermp/replication/ReplicationWorker.h" // ReplicationExecuteResult

namespace stalkermp::replication
{
    class ReplicationDiagnostics
    {
    public:
        ReplicationDiagnostics() = default;

        // Restore the initial (all-zero) state.
        void Reset() noexcept { m_stats = ReplicationStatistics{}; }

        // Immutable value snapshot of the current counters (a copy; the collector
        // is unaffected).
        [[nodiscard]] ReplicationStatistics Snapshot() const noexcept { return m_stats; }

        // --- Non-invasive collection (monotonic counters) --------------------

        // One worker pass completed. Folds tick-level aggregates: increments the
        // tick counter, sets the activeClients gauge, and adds the entities routed.
        void RecordTick(const ReplicationExecuteResult& result) noexcept
        {
            ++m_stats.ticks;
            m_stats.activeClients = result.clientsProcessed;      // gauge (last tick)
            m_stats.entitiesReplicated += static_cast<std::uint32_t>(result.recordsQueued);
        }

        // One assembled packet. Counts it built + sent, adds its bytes, and bins it
        // by reliability.
        void RecordPacket(const ReplicationReliability reliability, const std::uint64_t byteCount) noexcept
        {
            ++m_stats.updatesBuilt;
            ++m_stats.updatesSent;
            m_stats.bytesSent += byteCount;
            if (reliability == ReplicationReliability::Reliable)
            {
                ++m_stats.reliablePackets;
            }
            else
            {
                ++m_stats.unreliablePackets;
            }
        }

        // One acknowledgement event: applied (advanced a baseline) or ignored
        // (duplicate/stale).
        void RecordAck(const bool applied) noexcept
        {
            if (applied)
            {
                ++m_stats.acksApplied;
            }
            else
            {
                ++m_stats.acksIgnored;
            }
        }

        // One bounded-memory overflow (a dropped update).
        void RecordOverflow() noexcept
        {
            ++m_stats.overflows;
            ++m_stats.updatesDropped;
        }

    private:
        ReplicationStatistics m_stats{};
    };
} // namespace stalkermp::replication
