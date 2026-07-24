// STALKER-MP — Plugin diagnostics collector (Sprint-014, Step 15)
//
// A pure, header-only, NON-INVASIVE collector + inspector of plugin-subsystem metrics:
// monotonic counters (loaded / active plugins, event dispatches, API calls, errors), plus
// read-only inspectors that snapshot the registry's lifecycle states and isolated-plugin
// status and check registry/binding consistency. It references nothing in the runtime it
// cannot read `const`, mutates no subsystem state, schedules nothing, logs nothing, writes
// no file, and touches no network; `Snapshot()`/`Inspect*()` return immutable value copies
// and `Reset()` restores the initial counter state.
//
// Diagnostic timing/memory are EXPLICITLY diagnostic: values are supplied by the caller (no
// clock or engine access here) and NEVER participate in replay identity or control flow
// (Deterministic Simulation). Aggregates are deterministic integer computations.
//
// This step introduces the collector/inspector ONLY — no manager reference, no mutation,
// no integration (Step 17).
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "stalkermp/plugin/EventBinding.h"    // EventBinding (read-only: Count)
#include "stalkermp/plugin/FaultIsolation.h"  // FaultIsolation (read-only: IsIsolated / Count)
#include "stalkermp/plugin/PluginContext.h"   // PluginContext (state)
#include "stalkermp/plugin/PluginRegistry.h"  // PluginRegistry (read-only: Enumerate / Count)
#include "stalkermp/plugin/PluginTypes.h"     // PluginStatistics / PluginConsistency / PluginState

namespace stalkermp::plugin
{
    class PluginDiagnostics
    {
    public:
        // One plugin's lifecycle snapshot (value-only; deterministic fields).
        struct PluginStateInfo
        {
            PluginId id{};
            PluginState state = PluginState::Discovered;
            bool isolated = false; // registry state Failed and/or recorded in FaultIsolation
        };

        // --- Counters + statistics (Reset / Snapshot) ------------------------
        void Reset() noexcept
        {
            m_stats = PluginStatistics{};
            m_execSamples = 0;
            m_execDurationSum = 0;
        }

        [[nodiscard]] PluginStatistics Snapshot() const noexcept { return m_stats; }

        // Monotonic counters (never gate control flow).
        void RecordPluginLoaded() noexcept { ++m_stats.loadedPlugins; }
        void RecordPluginUnloaded() noexcept
        {
            if (m_stats.loadedPlugins > 0)
            {
                --m_stats.loadedPlugins;
            }
        }
        void RecordPluginActivated() noexcept { ++m_stats.activePlugins; }
        void RecordPluginDeactivated() noexcept
        {
            if (m_stats.activePlugins > 0)
            {
                --m_stats.activePlugins;
            }
        }
        void RecordEventDispatch() noexcept { ++m_stats.eventDispatches; }
        void RecordApiCall() noexcept { ++m_stats.apiCalls; }
        void RecordPluginError() noexcept { ++m_stats.pluginErrors; }

        // Diagnostic timing / memory (supplied by the caller; never gate control flow).
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

        // --- Read-only inspectors (no mutation of any argument) --------------
        // Lifecycle snapshot for every registered plugin, ascending by id. A plugin is
        // reported isolated when its registry state is Failed OR it is recorded in `faults`.
        [[nodiscard]] static std::vector<PluginStateInfo> InspectStates(const PluginRegistry& registry,
                                                                        const FaultIsolation& faults)
        {
            std::vector<PluginStateInfo> out;
            const std::vector<PluginContext> contexts = registry.Enumerate(); // ascending
            out.reserve(contexts.size());
            for (const PluginContext& c : contexts)
            {
                const bool isolated = (c.state == PluginState::Failed) || faults.IsIsolated(c.id);
                out.push_back(PluginStateInfo{c.id, c.state, isolated});
            }
            return out;
        }

        // Count of plugins currently in `state`, from the registry (read-only).
        [[nodiscard]] static std::size_t CountInState(const PluginRegistry& registry, const PluginState state)
        {
            std::size_t count = 0;
            for (const PluginContext& c : registry.Enumerate())
            {
                if (c.state == state)
                {
                    ++count;
                }
            }
            return count;
        }

        // Deterministic registry/binding consistency snapshot (read-only). `consistent` is
        // false if any registered plugin is simultaneously Active and isolated (an invariant
        // breach: an isolated plugin must not remain active).
        [[nodiscard]] static PluginConsistency InspectConsistency(const PluginRegistry& registry,
                                                                  const EventBinding& events,
                                                                  const FaultIsolation& faults)
        {
            PluginConsistency c{};
            c.registeredPlugins = registry.Count();
            c.boundHooks = events.Count();
            bool consistent = true;
            std::uint64_t active = 0;
            for (const PluginContext& ctx : registry.Enumerate())
            {
                const bool isolated = (ctx.state == PluginState::Failed) || faults.IsIsolated(ctx.id);
                if (ctx.state == PluginState::Active)
                {
                    ++active;
                    if (isolated)
                    {
                        consistent = false; // active-and-isolated is an invariant breach
                    }
                }
            }
            c.activePlugins = active;
            c.consistent = consistent;
            return c;
        }

    private:
        PluginStatistics m_stats{};
        std::uint64_t m_execSamples = 0;
        std::uint64_t m_execDurationSum = 0;
    };
} // namespace stalkermp::plugin
