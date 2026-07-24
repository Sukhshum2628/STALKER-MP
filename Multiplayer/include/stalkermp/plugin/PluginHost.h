// STALKER-MP — Plugin host (Sprint-014, Step 12)
//
// Coordinates RUNTIME interaction with already-loaded plugins: it binds each loaded
// plugin id to its `IPlugin` instance, advances the lifecycle beyond `Loaded`
// (Initialize -> Active), invokes plugin callbacks ONLY through the `IPlugin` contract,
// and maintains deterministic plugin execution ordering (ascending id). It exposes the
// approved `IPluginHostSurface` (gameplay facades) and hands the facade bundle to
// `IPlugin::Initialize`. Faults are delegated to the injected `FaultIsolation` policy —
// the host does not own the policy.
//
// The host performs NO discovery, validation, or loading; owns no platform functionality;
// owns no runtime scheduling (the manager schedules); and owns no fault-isolation policy
// (it uses the injected one). Engine-free / platform-free.
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "stalkermp/plugin/FaultIsolation.h"    // FaultIsolation (Step 14)
#include "stalkermp/plugin/IPlugin.h"           // IPlugin / PluginArgs
#include "stalkermp/plugin/PluginHostSurface.h" // IPluginHostSurface (Step 9)
#include "stalkermp/plugin/PluginRegistry.h"    // PluginRegistry (Step 4)
#include "stalkermp/plugin/PluginTypes.h"       // PluginId / PluginEvent / PluginOutcome

namespace stalkermp::plugin
{
    // The result of a host operation over the bound plugins.
    struct PluginHostReport
    {
        std::size_t invoked = 0;  // plugins whose callback was invoked
        std::size_t isolated = 0; // plugins disabled this operation due to a fault
        std::size_t skipped = 0;  // plugins skipped (isolated / not in the required state)
    };

    class PluginHost
    {
    public:
        PluginHost(IPluginHostSurface& surface, PluginRegistry& registry, FaultIsolation& faults) noexcept
            : m_surface(surface), m_registry(registry), m_faults(faults)
        {
        }

        // Bind a loaded plugin's instance to its id (composition / static registration
        // provides the instance). A none id -> NotFound; a duplicate binding -> DuplicatePlugin.
        [[nodiscard]] PluginOutcome Bind(PluginId id, IPlugin& plugin);

        // Initialize all bound-and-Loaded plugins in ascending id order: Initialize(host)
        // -> lifecycle Loaded -> Initialized -> Active. A fault isolates the plugin.
        PluginHostReport InitializeAll();

        // Invoke one active plugin's OnEvent. Unbound -> NotFound; isolated/not-Active ->
        // skipped (PluginDisabled). A fault isolates the plugin and returns the fault.
        [[nodiscard]] PluginOutcome Invoke(PluginId id, PluginEvent event, const PluginArgs& args);

        // Dispatch an event to every Active bound plugin in ascending id order. Faults
        // isolate the offending plugin; siblings continue.
        PluginHostReport Dispatch(PluginEvent event, const PluginArgs& args);

        // Shut down all live plugins in ascending id order: Shutdown() -> lifecycle Shutdown.
        void ShutdownAll();

        // The approved gameplay host-service surface exposed to plugins.
        [[nodiscard]] IPluginHostSurface& Surface() noexcept { return m_surface; }

        [[nodiscard]] std::size_t BoundCount() const noexcept { return m_bindings.size(); }
        [[nodiscard]] std::size_t ActiveCount() const noexcept;
        [[nodiscard]] bool IsIsolated(PluginId id) const noexcept { return m_faults.IsIsolated(id); }

    private:
        [[nodiscard]] IPlugin* Find(PluginId id) const noexcept;

        IPluginHostSurface& m_surface;
        PluginRegistry& m_registry;
        FaultIsolation& m_faults;
        std::vector<std::pair<PluginId, IPlugin*>> m_bindings; // sorted ascending by id
    };
} // namespace stalkermp::plugin
