// STALKER-MP — Plugin manager (Sprint-014, Step 13)
//
// The engine-free orchestrator of the plugin subsystem: it owns the PluginRegistry, the
// EventBinding table, the FaultIsolation policy, and the PluginHost, and it coordinates the
// already-established PluginLoader and PluginHost to run the subsystem end to end — startup
// (load every plugin, then initialize/activate the loaded ones), deterministic per-frame
// tick + event dispatch (ascending plugin id), and shutdown. It is a `core::IService` +
// `core::ITickable`, mirroring the Sprint-013 ScriptManager. It is NOT subscribed to the
// FrameDispatcher here — that wiring at the reserved kPlugins tick key is the Step-17
// composition gate.
//
// The manager DUPLICATES neither the loader nor the host: it delegates loading to
// PluginLoader and all runtime interaction to PluginHost. It performs no platform work
// (the source/instances are injected), no engine call, and creates no thread. Every
// fallible operation is a value outcome (ADR-007).
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/core/interfaces/ITickable.h"
#include "stalkermp/plugin/EventBinding.h"        // EventBinding (Step 5)
#include "stalkermp/plugin/FaultIsolation.h"      // FaultIsolation (Step 14)
#include "stalkermp/plugin/IPlugin.h"             // IPlugin / PluginArgs
#include "stalkermp/plugin/IPluginSource.h"       // IPluginSource (Step 7)
#include "stalkermp/plugin/PluginConfiguration.h" // PluginConfiguration (Step 2)
#include "stalkermp/plugin/PluginHost.h"          // PluginHost (Step 12)
#include "stalkermp/plugin/PluginHostSurface.h"   // IPluginHostSurface (Step 9)
#include "stalkermp/plugin/PluginRegistry.h"      // PluginRegistry (Step 4)
#include "stalkermp/plugin/PluginTypes.h"         // PluginId / PluginEvent / PluginOutcome

namespace stalkermp::plugin
{
    // The outcome of a subsystem startup: the loader's batch result folded together with the
    // host's initialize result.
    struct PluginStartupReport
    {
        std::size_t discovered = 0;   // plugins the source enumerated
        std::size_t loaded = 0;       // plugins that reached Loaded
        std::size_t initialized = 0;  // plugins that reached Active
        std::size_t isolated = 0;     // plugins isolated during load or initialize
    };

    class PluginManager final : public core::IService, public core::ITickable
    {
    public:
        PluginManager(IPluginSource& source, IPluginHostSurface& surface,
                      const std::vector<std::uint32_t>& availableApis,
                      const PluginConfiguration& config) noexcept
            : m_source(source), m_surface(surface), m_availableApis(availableApis), m_config(config),
              m_host(m_surface, m_registry, m_faults)
        {
        }

        // --- Orchestration ---------------------------------------------------
        // Load every plugin from the source (PluginLoader), then initialize/activate the
        // loaded ones (PluginHost). Deterministic; per-plugin faults isolated.
        [[nodiscard]] PluginStartupReport Startup();

        // Bind a loaded plugin's instance to its id so the host can drive it. Delegates to
        // PluginHost::Bind (none -> NotFound; duplicate -> DuplicatePlugin).
        [[nodiscard]] PluginOutcome BindPlugin(PluginId id, IPlugin& plugin);

        // Subscribe a plugin to a host event (delegates to EventBinding::Bind).
        [[nodiscard]] PluginOutcome Subscribe(PluginEvent event, PluginId plugin);

        // Dispatch an event to its subscribers in ascending id order (deterministic) through
        // the host. Faults isolate the offending plugin; siblings continue.
        PluginHostReport DispatchEvent(PluginEvent event, const PluginArgs& args);

        // --- Introspection ---------------------------------------------------
        [[nodiscard]] std::size_t PluginCount() const noexcept { return m_registry.Count(); }
        [[nodiscard]] bool IsIsolated(PluginId id) const noexcept { return m_faults.IsIsolated(id); }
        [[nodiscard]] std::size_t IsolatedCount() const noexcept { return m_faults.Count(); }
        [[nodiscard]] const PluginRegistry& Registry() const noexcept { return m_registry; }
        [[nodiscard]] const PluginHost& Host() const noexcept { return m_host; }
        [[nodiscard]] std::uint64_t Ticks() const noexcept { return m_tick; }

        // --- core::IService --------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Plugins"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override;
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- core::ITickable -------------------------------------------------
        // One per-frame OnTick dispatch. `deltaSeconds` is DIAGNOSTIC and never used in
        // control flow (the tick counter drives dispatch — deterministic). Wired at the
        // reserved kPlugins tick key in Step-17, not here.
        void Tick(double deltaSeconds) override;

    private:
        IPluginSource& m_source;
        IPluginHostSurface& m_surface;
        const std::vector<std::uint32_t>& m_availableApis;
        const PluginConfiguration& m_config;

        PluginRegistry m_registry; // owned
        EventBinding m_events;     // owned
        FaultIsolation m_faults;   // owned (policy) — referenced by the host
        PluginHost m_host;         // owned — references m_surface/m_registry/m_faults

        std::uint64_t m_tick = 0;
    };
} // namespace stalkermp::plugin
