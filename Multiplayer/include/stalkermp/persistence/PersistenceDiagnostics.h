// STALKER-MP — Persistence diagnostics collector (Sprint-011, Step 13)
//
// A pure, header-only, NON-INVASIVE collector of persistence metrics. It holds only
// monotonic counters plus deterministic aggregates (average save duration, worker
// utilisation, max queue depth); it references nothing in the pipeline, mutates no
// subsystem state, and produces no output. `Snapshot()` returns an immutable value
// copy of the tallies; `Reset()` restores the initial state.
//
// Timing is explicitly diagnostic: save durations and `timestampWallClock` are for
// human inspection ONLY and never participate in replay identity or control flow
// (Deterministic Simulation; Replay Determinism).
//
// This step introduces the collector ONLY — no pipeline reference, no mutation, no
// Bootstrap wiring.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/persistence/PersistenceTypes.h" // PersistenceStatistics, SaveTrigger

namespace stalkermp::persistence
{
    class PersistenceDiagnostics
    {
    public:
        // Restore the initial state (all counters/aggregates zeroed).
        void Reset() noexcept
        {
            m_stats = PersistenceStatistics{};
            m_durationSum = 0;
            m_durationSamples = 0;
            m_workerBusySamples = 0;
            m_workerTotalSamples = 0;
        }

        // An immutable value copy of the current tallies.
        [[nodiscard]] PersistenceStatistics Snapshot() const noexcept { return m_stats; }

        // Count a save request; an Autosave trigger also increments the autosave tally.
        void RecordSaveRequest(SaveTrigger trigger) noexcept
        {
            ++m_stats.saveRequests;
            if (trigger == SaveTrigger::Autosave)
            {
                ++m_stats.autosaves;
            }
        }

        void RecordCompleted() noexcept { ++m_stats.completedSaves; }
        void RecordFailure() noexcept { ++m_stats.failedSaves; }
        void RecordRetry() noexcept { ++m_stats.retries; }
        void RecordOverflow() noexcept { ++m_stats.queueOverflows; }

        // Record one save's duration (microseconds). DIAGNOSTIC timing only: updates
        // the last/max and the running sum used for the deterministic average.
        void RecordSaveDuration(std::uint64_t micros) noexcept
        {
            m_stats.lastSaveDurationMicros = micros;
            if (micros > m_stats.maxSaveDurationMicros)
            {
                m_stats.maxSaveDurationMicros = micros;
            }
            m_durationSum += micros;
            ++m_durationSamples;
        }

        // Sample the current queue depth (updates current + max).
        void RecordQueueDepth(std::uint32_t depth) noexcept
        {
            m_stats.currentQueueDepth = depth;
            if (depth > m_stats.maxQueueDepth)
            {
                m_stats.maxQueueDepth = depth;
            }
        }

        // Sample whether the worker did work this pass (for the utilisation aggregate).
        void RecordWorkerActivity(bool busy) noexcept
        {
            ++m_workerTotalSamples;
            if (busy)
            {
                ++m_workerBusySamples;
            }
        }

        // Set the diagnostic wall-clock timestamp. DIAGNOSTIC ONLY — never used in
        // replay identity or control flow.
        void RecordTimestamp(std::uint64_t wallClock) noexcept { m_stats.timestampWallClock = wallClock; }

        // Deterministic average save duration (integer microseconds); 0 when none.
        [[nodiscard]] std::uint64_t AverageSaveDurationMicros() const noexcept
        {
            return m_durationSamples == 0 ? 0 : m_durationSum / m_durationSamples;
        }

        // Deterministic worker utilisation, 0..100 (integer percent); 0 when no sample.
        [[nodiscard]] std::uint32_t WorkerUtilizationPercent() const noexcept
        {
            if (m_workerTotalSamples == 0)
            {
                return 0;
            }
            return static_cast<std::uint32_t>((m_workerBusySamples * 100) / m_workerTotalSamples);
        }

        // Read-only sample counts (for inspectors).
        [[nodiscard]] std::uint64_t DurationSamples() const noexcept { return m_durationSamples; }
        [[nodiscard]] std::uint64_t WorkerSamples() const noexcept { return m_workerTotalSamples; }

    private:
        PersistenceStatistics m_stats{};      // monotonic counters + last/max aggregates
        std::uint64_t m_durationSum = 0;      // sum of save durations (for the average)
        std::uint64_t m_durationSamples = 0;  // number of duration samples
        std::uint64_t m_workerBusySamples = 0;  // worker-busy samples
        std::uint64_t m_workerTotalSamples = 0; // total worker samples
    };
} // namespace stalkermp::persistence
