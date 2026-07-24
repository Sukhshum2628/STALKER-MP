// STALKER-MP — Plugin manager implementation (Sprint-014, Step 13)
//
// See PluginManager.h. Engine-free / platform-free / OS-free; no exceptions, no RTTI; value
// outcomes. Deterministic dispatch order (ascending plugin id); per-plugin fault isolation
// delegated to FaultIsolation. Loading is delegated to PluginLoader and all runtime plugin
// interaction to PluginHost — the manager duplicates neither.

#include "stalkermp/plugin/PluginManager.h"

#include "stalkermp/plugin/PluginLoader.h" // PluginLoader (Step 11)

namespace stalkermp::plugin
{
    PluginStartupReport PluginManager::Startup()
    {
        PluginStartupReport report;

        // 1) Load every plugin from the source (deterministic; per-plugin faults isolated).
        PluginLoader loader(m_source, m_registry, m_availableApis, m_config);
        const PluginLoadReport load = loader.LoadAll();
        report.discovered = load.attempted;
        report.loaded = load.loaded;
        report.isolated = load.failed;

        // 2) Initialize/activate the loaded-and-bound plugins through the host.
        const PluginHostReport init = m_host.InitializeAll();
        report.initialized = init.invoked;
        report.isolated += init.isolated;
        return report;
    }

    PluginOutcome PluginManager::BindPlugin(const PluginId id, IPlugin& plugin)
    {
        return m_host.Bind(id, plugin);
    }

    PluginOutcome PluginManager::Subscribe(const PluginEvent event, const PluginId plugin)
    {
        return m_events.Bind(event, plugin);
    }

    PluginHostReport PluginManager::DispatchEvent(const PluginEvent event, const PluginArgs& args)
    {
        PluginHostReport report;
        // SubscribersFor is deterministic ascending by plugin id.
        for (const PluginId id : m_events.SubscribersFor(event))
        {
            const bool wasIsolated = m_faults.IsIsolated(id);
            const PluginOutcome outcome = m_host.Invoke(id, event, args);
            if (outcome == PluginOutcome::Ok)
            {
                ++report.invoked;
            }
            else if (!wasIsolated && m_faults.IsIsolated(id))
            {
                ++report.isolated; // this dispatch faulted and isolated the plugin
            }
            else
            {
                ++report.skipped; // already isolated / not Active / unbound instance
            }
        }
        return report;
    }

    std::vector<std::string> PluginManager::Dependencies() const
    {
        // Ordering-only edges: plugins run after the simulation producers (mirrors the
        // Sprint-013 scripting ordering). The concrete tick subscription at the reserved
        // kPlugins key is Step-17; these names only order registration.
        return {"World", "EntityRegistry", "PlayerManager", "BubbleManager", "TransitionManager"};
    }

    core::Expected<void> PluginManager::Initialize()
    {
        return core::Success();
    }

    void PluginManager::Shutdown() noexcept
    {
        m_host.ShutdownAll();
        m_faults.Clear();
    }

    void PluginManager::Tick(const double deltaSeconds)
    {
        (void)deltaSeconds; // DIAGNOSTIC only; never in control flow (deterministic)
        ++m_tick;
        PluginArgs args{};
        args.count = 1;
        args.words[0] = m_tick; // deterministic tick counter, not wall-clock
        (void)DispatchEvent(PluginEvent::OnTick, args);
    }
} // namespace stalkermp::plugin
