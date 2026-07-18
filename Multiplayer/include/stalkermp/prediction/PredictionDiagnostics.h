// STALKER-MP — Prediction diagnostics collector (Sprint-010, Step 13)
//
// A pure, header-only, NON-INVASIVE collector of prediction/interpolation metrics.
// It holds only monotonic counters plus deterministic correction aggregates
// (last / max / average magnitude); it references nothing in the pipeline, mutates
// no simulation or subsystem state, and produces no output. `Snapshot()` returns
// an immutable value copy of the tallies; `Reset()` restores the initial state.
//
// Timing is explicitly diagnostic: `timestampWallClock` (and RecordTimestamp) is a
// wall-clock value for human inspection ONLY and never participates in replay
// identity or any control flow (Deterministic Simulation; Replay Determinism).
//
// This step introduces the collector ONLY — no pipeline reference, no mutation, no
// driver, and no engine binding.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/prediction/PredictionTypes.h" // PredictionStatistics

namespace stalkermp::prediction
{
    class PredictionDiagnostics
    {
    public:
        // Restore the initial state (all counters/aggregates zeroed).
        void Reset() noexcept
        {
            m_stats = PredictionStatistics{};
            m_correctionSum = 0;
        }

        // An immutable value copy of the current tallies. Mutating the returned value
        // never affects the collector; a snapshot taken earlier is unaffected by later
        // records.
        [[nodiscard]] PredictionStatistics Snapshot() const noexcept { return m_stats; }

        // Count `count` predictions run (default 1). Monotonic.
        void RecordPrediction(std::uint64_t count = 1) noexcept { m_stats.predictionsRun += count; }

        // Record one applied correction of the given magnitude (fixed-point mm).
        // Updates the count, the last/max magnitude, and the running sum used for the
        // deterministic average. Monotonic.
        void RecordCorrection(std::uint32_t errorMagnitude) noexcept
        {
            ++m_stats.corrections;
            m_stats.lastCorrectionMagnitude = errorMagnitude;
            if (errorMagnitude > m_stats.maxCorrectionMagnitude)
            {
                m_stats.maxCorrectionMagnitude = errorMagnitude;
            }
            m_correctionSum += errorMagnitude;
        }

        // Count `count` interpolation passes (default 1). Monotonic.
        void RecordInterpolation(std::uint64_t count = 1) noexcept { m_stats.interpolations += count; }

        // Count one bounded-buffer overflow. Monotonic.
        void RecordOverflow() noexcept { ++m_stats.overflows; }

        // Set the diagnostic wall-clock timestamp. DIAGNOSTIC ONLY — never used in
        // replay identity or control flow.
        void RecordTimestamp(std::uint64_t wallClock) noexcept { m_stats.timestampWallClock = wallClock; }

        // Deterministic average correction magnitude (integer mm); 0 when none.
        [[nodiscard]] std::uint32_t AverageCorrectionMagnitude() const noexcept
        {
            if (m_stats.corrections == 0)
            {
                return 0;
            }
            return static_cast<std::uint32_t>(m_correctionSum / m_stats.corrections);
        }

        // The running sum of correction magnitudes (fixed-point mm). Read-only.
        [[nodiscard]] std::uint64_t CorrectionMagnitudeSum() const noexcept { return m_correctionSum; }

    private:
        PredictionStatistics m_stats{};      // monotonic counters + last/max aggregates
        std::uint64_t m_correctionSum = 0;   // sum of magnitudes (for deterministic average)
    };
} // namespace stalkermp::prediction
