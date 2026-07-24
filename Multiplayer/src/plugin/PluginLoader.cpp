// STALKER-MP — Plugin loader (Sprint-014, Step 11)
//
// See PluginLoader.h. Engine-free / platform-free / OS-free; no exceptions, no RTTI;
// value outcomes. Deterministic ascending order; each plugin is loaded independently so a
// single failure isolates only that plugin. No plugin execution — the lifecycle advances
// only to LOADED.

#include "stalkermp/plugin/PluginLoader.h"

#include "stalkermp/plugin/PluginContext.h"    // PluginContext (Step 4)
#include "stalkermp/plugin/PluginLifecycle.h"  // PluginLifecycle (Step 10)
#include "stalkermp/plugin/PluginValidator.h"  // PluginValidator (Step 6)

namespace stalkermp::plugin
{
    PluginOutcome PluginLoader::LoadOne(const PluginId id)
    {
        // 1) Read the manifest from the discovery source.
        auto manifest = m_source.ReadManifest(id);
        if (!manifest.HasValue())
        {
            return PluginOutcome::LoadFailed;
        }
        const PluginDescriptor& descriptor = manifest.Value();

        // 2) Validate the manifest (config/descriptor/duplicate/version/APIs/deps, Step 6).
        if (const auto o = PluginValidator::Validate(descriptor, m_registry, m_availableApis, m_config);
            o != PluginOutcome::Ok)
        {
            return o;
        }

        // 3) Build the context and advance its lifecycle to LOADED: Discovered -> Validated
        //    -> Loaded (deterministic, Step 10). Initialize/Activate (plugin execution) are
        //    the manager's job (Step 13) — NEVER performed here.
        PluginContext context;
        context.id = descriptor.id;
        context.descriptor = descriptor;
        context.state = PluginState::Discovered;
        if (const auto o = PluginLifecycle::Apply(context.state, LifecycleAction::Validate); o != PluginOutcome::Ok)
        {
            return o;
        }
        if (const auto o = PluginLifecycle::Apply(context.state, LifecycleAction::Load); o != PluginOutcome::Ok)
        {
            return o;
        }

        // 4) Register the loaded context (state == Loaded).
        return m_registry.Register(context);
    }

    PluginLoadReport PluginLoader::LoadAll()
    {
        PluginLoadReport report;
        const std::vector<PluginDescriptor> manifests = m_source.Enumerate(); // ascending by id
        for (const PluginDescriptor& d : manifests)
        {
            ++report.attempted;
            const PluginOutcome outcome = LoadOne(d.id);
            if (outcome == PluginOutcome::Ok)
            {
                ++report.loaded;
            }
            else
            {
                ++report.failed;
                report.failures.emplace_back(d.id, outcome); // isolate; continue
            }
        }
        return report;
    }
} // namespace stalkermp::plugin
