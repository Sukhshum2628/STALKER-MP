// STALKER-MP — Save/Load diagnostics collector (Sprint-012, Step 15)
//
// A pure, header-only, NON-INVASIVE collector of save/load & recovery metrics. It
// holds only monotonic counters plus deterministic aggregates (recovery success
// rate, average durations); it references nothing in the pipeline, mutates no
// subsystem state, and produces no output. `Snapshot()` returns an immutable value
// copy of the tallies; `Reset()` restores the initial state.
//
// Timing is explicitly diagnostic: save/load/migration durations and the wall-clock
// are for human inspection ONLY and never participate in replay identity or control
// flow (Deterministic Simulation; Replay Determinism).
//
// This step introduces the collector ONLY — no pipeline reference, no mutation, no
// integration or hardening.
//
// Engine-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstdint>

#include "stalkermp/saveload/SaveLoadTypes.h" // RecoveryStatistics

namespace stalkermp::saveload
{
    class SaveLoadDiagnostics
    {
    public:
        void Reset() noexcept
        {
            m_stats = RecoveryStatistics{};
            m_saveSamples = 0;
            m_saveDurationSum = 0;
            m_loadSamples = 0;
            m_loadDurationSum = 0;
        }

        [[nodiscard]] RecoveryStatistics Snapshot() const noexcept { return m_stats; }

        // Save/load timing (DIAGNOSTIC microseconds): updates the last value and the
        // running sum used for the deterministic average.
        void RecordSaveDuration(std::uint64_t micros) noexcept
        {
            m_stats.saveDurationMicros = micros;
            m_saveDurationSum += micros;
            ++m_saveSamples;
        }
        void RecordLoadDuration(std::uint64_t micros) noexcept
        {
            m_stats.loadDurationMicros = micros;
            m_loadDurationSum += micros;
            ++m_loadSamples;
        }
        void RecordMigrationTime(std::uint64_t micros) noexcept { m_stats.migrationTimeMicros = micros; }

        void RecordEntitiesRestored(std::uint64_t count) noexcept { m_stats.entitiesRestored += count; }
        void RecordPlayersRestored(std::uint64_t count) noexcept { m_stats.playersRestored += count; }
        void RecordValidationFailure() noexcept { ++m_stats.validationFailures; }

        // Recovery outcome accounting.
        void RecordRecoveryAttempt() noexcept { ++m_stats.recoveryAttempts; }
        void RecordRecoverySuccess() noexcept { ++m_stats.recoverySuccesses; }

        // DIAGNOSTIC wall-clock; never used in control flow or replay identity.
        void RecordTimestamp(std::uint64_t wallClock) noexcept { m_stats.timestampWallClock = wallClock; }

        // Deterministic recovery success rate, 0..100 (integer percent); 0 when no
        // attempts.
        [[nodiscard]] std::uint32_t RecoverySuccessPercent() const noexcept
        {
            if (m_stats.recoveryAttempts == 0)
            {
                return 0;
            }
            return static_cast<std::uint32_t>((m_stats.recoverySuccesses * 100) / m_stats.recoveryAttempts);
        }

        // Deterministic average save/load duration (integer microseconds); 0 when none.
        [[nodiscard]] std::uint64_t AverageSaveDurationMicros() const noexcept
        {
            return m_saveSamples == 0 ? 0 : m_saveDurationSum / m_saveSamples;
        }
        [[nodiscard]] std::uint64_t AverageLoadDurationMicros() const noexcept
        {
            return m_loadSamples == 0 ? 0 : m_loadDurationSum / m_loadSamples;
        }

    private:
        RecoveryStatistics m_stats{};
        std::uint64_t m_saveSamples = 0;
        std::uint64_t m_saveDurationSum = 0;
        std::uint64_t m_loadSamples = 0;
        std::uint64_t m_loadDurationSum = 0;
    };
} // namespace stalkermp::saveload
