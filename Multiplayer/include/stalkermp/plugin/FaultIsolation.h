// STALKER-MP — Plugin fault isolation (Sprint-014, Step 14)
//
// The deterministic fault-isolation policy for the plugin runtime. When a plugin fault is
// reported through the contract (a non-Ok value outcome from Initialize / OnEvent), this
// component ISOLATES that plugin: it records the plugin as disabled and transitions its
// lifecycle state to `Failed`, so subsequent processing skips it while the remaining
// plugins keep running. It returns ADR-007 value outcomes and never throws.
//
// This is the isolation POLICY only — it performs no diagnostics, crash reporting,
// recovery framework, watchdog, or threading. The host (Step-12) uses it during
// initialize/dispatch; the manager (Step-13) owns the instance.
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "stalkermp/plugin/PluginContext.h"  // PluginContext (state)
#include "stalkermp/plugin/PluginRegistry.h" // PluginRegistry
#include "stalkermp/plugin/PluginTypes.h"    // PluginId / PluginState / PluginOutcome

namespace stalkermp::plugin
{
    class FaultIsolation
    {
    public:
        // Isolate a plugin after a contract-reported fault: mark it Failed in the registry
        // (if present) and record it disabled. Returns PluginDisabled (the isolation
        // outcome). Idempotent — re-isolating an already-isolated plugin is a no-op that
        // still reports PluginDisabled.
        [[nodiscard]] PluginOutcome Isolate(const PluginId id, PluginRegistry& registry)
        {
            const auto it = std::lower_bound(m_disabled.begin(), m_disabled.end(), id);
            if (it == m_disabled.end() || *it != id)
            {
                m_disabled.insert(it, id); // keep sorted, dedup
            }
            if (PluginContext* c = registry.Find(id))
            {
                c->state = PluginState::Failed; // lifecycle failure transition
            }
            return PluginOutcome::PluginDisabled;
        }

        [[nodiscard]] bool IsIsolated(const PluginId id) const noexcept
        {
            const auto it = std::lower_bound(m_disabled.begin(), m_disabled.end(), id);
            return it != m_disabled.end() && *it == id;
        }

        [[nodiscard]] std::size_t Count() const noexcept { return m_disabled.size(); }

        void Clear() noexcept { m_disabled.clear(); }

    private:
        std::vector<PluginId> m_disabled; // sorted ascending
    };
} // namespace stalkermp::plugin
