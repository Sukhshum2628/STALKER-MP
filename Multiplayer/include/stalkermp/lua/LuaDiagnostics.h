// STALKER-MP — Lua diagnostics collector (Sprint-013, Step 15)
//
// A pure, header-only, NON-INVASIVE collector of scripting metrics: monotonic counters
// (loaded scripts, API calls, callbacks, errors), diagnostic timing/memory samples, a
// bounded execution timeline, and a bounded console buffer. It references nothing in the
// pipeline, mutates no subsystem state, and produces no output; `Snapshot()` returns an
// immutable value copy and `Reset()` restores the initial state.
//
// Timing and memory are EXPLICITLY diagnostic: values are supplied by the caller (no
// clock or engine access here) and never participate in replay identity or control flow
// (Deterministic Simulation). Buffers are bounded (ADR-007 bounded memory), dropping the
// oldest entry when full — deterministic.
//
// This step introduces the collector ONLY — no pipeline reference, no mutation, no
// integration (Step 17).
//
// Engine-free / VM-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/lua/LuaScriptTypes.h" // ScriptStatistics / ScriptId / ScriptEvent / ScriptOutcome

namespace stalkermp::lua
{
    class LuaDiagnostics
    {
    public:
        // One recorded dispatch/execution sample (diagnostic; deterministic fields).
        struct TimelineEntry
        {
            std::uint64_t tick = 0;
            ScriptId script{};
            ScriptEvent event = ScriptEvent::OnTick;
            ScriptOutcome outcome = ScriptOutcome::Ok;
            std::uint64_t durationMicros = 0; // diagnostic
        };

        // Bounded buffer capacities (deterministic memory bound).
        static constexpr std::size_t kMaxTimeline = 128;
        static constexpr std::size_t kMaxConsole = 128;

        void Reset() noexcept
        {
            m_stats = ScriptStatistics{};
            m_execSamples = 0;
            m_execDurationSum = 0;
            m_timeline.clear();
            m_console.clear();
        }

        [[nodiscard]] ScriptStatistics Snapshot() const noexcept { return m_stats; }

        // --- Monotonic counters (§7.11) --------------------------------------
        void RecordScriptLoaded() noexcept { ++m_stats.loadedScripts; }
        void RecordScriptUnloaded() noexcept
        {
            if (m_stats.loadedScripts > 0)
            {
                --m_stats.loadedScripts;
            }
        }
        void RecordApiCall() noexcept { ++m_stats.apiCalls; }
        void RecordCallback() noexcept { ++m_stats.callbackCount; }
        void RecordScriptError() noexcept { ++m_stats.scriptErrors; }

        // --- Diagnostic timing / memory (never gate control flow) ------------
        void RecordExecutionTime(std::uint64_t micros) noexcept
        {
            m_stats.executionTimeMicros = micros;
            m_execDurationSum += micros;
            ++m_execSamples;
        }
        void RecordMemory(std::uint64_t bytes) noexcept { m_stats.memoryBytes = bytes; }

        // Deterministic average execution time (integer microseconds); 0 when no samples.
        [[nodiscard]] std::uint64_t AverageExecutionMicros() const noexcept
        {
            return m_execSamples == 0 ? 0 : m_execDurationSum / m_execSamples;
        }

        // --- Execution timeline (bounded; §7.10) -----------------------------
        void RecordTimeline(const TimelineEntry& entry)
        {
            if (m_timeline.size() >= kMaxTimeline)
            {
                m_timeline.erase(m_timeline.begin()); // drop oldest (bounded)
            }
            m_timeline.push_back(entry);
        }
        [[nodiscard]] const std::vector<TimelineEntry>& Timeline() const noexcept { return m_timeline; }

        // --- Console buffer (bounded; §7.10) ---------------------------------
        void RecordConsole(std::string_view line)
        {
            if (m_console.size() >= kMaxConsole)
            {
                m_console.erase(m_console.begin());
            }
            m_console.emplace_back(line);
        }
        [[nodiscard]] const std::vector<std::string>& Console() const noexcept { return m_console; }

    private:
        ScriptStatistics m_stats{};
        std::uint64_t m_execSamples = 0;
        std::uint64_t m_execDurationSum = 0;
        std::vector<TimelineEntry> m_timeline;
        std::vector<std::string> m_console;
    };
} // namespace stalkermp::lua
