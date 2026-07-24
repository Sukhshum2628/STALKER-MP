// STALKER-MP — Plugin host implementation (Sprint-014, Step 12)

#include "stalkermp/plugin/PluginHost.h"

#include <algorithm>

#include "stalkermp/plugin/PluginContext.h"   // PluginContext (state)
#include "stalkermp/plugin/PluginLifecycle.h" // PluginLifecycle (Step 10)

namespace stalkermp::plugin
{
    namespace
    {
        struct BindingLess
        {
            bool operator()(const std::pair<PluginId, IPlugin*>& lhs, const PluginId rhs) const noexcept
            {
                return lhs.first < rhs;
            }
        };
    } // namespace

    IPlugin* PluginHost::Find(const PluginId id) const noexcept
    {
        const auto it = std::lower_bound(m_bindings.begin(), m_bindings.end(), id, BindingLess{});
        if (it == m_bindings.end() || it->first != id)
        {
            return nullptr;
        }
        return it->second;
    }

    PluginOutcome PluginHost::Bind(const PluginId id, IPlugin& plugin)
    {
        if (id.IsNone())
        {
            return PluginOutcome::NotFound;
        }
        const auto it = std::lower_bound(m_bindings.begin(), m_bindings.end(), id, BindingLess{});
        if (it != m_bindings.end() && it->first == id)
        {
            return PluginOutcome::DuplicatePlugin;
        }
        m_bindings.insert(it, std::make_pair(id, &plugin));
        return PluginOutcome::Ok;
    }

    std::size_t PluginHost::ActiveCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto& binding : m_bindings)
        {
            if (m_faults.IsIsolated(binding.first))
            {
                continue;
            }
            const PluginContext* c = m_registry.Find(binding.first);
            if (c != nullptr && c->state == PluginState::Active)
            {
                ++count;
            }
        }
        return count;
    }

    PluginHostReport PluginHost::InitializeAll()
    {
        PluginHostReport report;
        for (const auto& binding : m_bindings) // ascending id — deterministic
        {
            const PluginId id = binding.first;
            IPlugin* plugin = binding.second;

            if (m_faults.IsIsolated(id))
            {
                ++report.skipped;
                continue;
            }
            PluginContext* c = m_registry.Find(id);
            if (c == nullptr || c->state != PluginState::Loaded)
            {
                ++report.skipped;
                continue;
            }

            const PluginOutcome outcome = plugin->Initialize(m_surface.Services());
            if (outcome != PluginOutcome::Ok)
            {
                (void)m_faults.Isolate(id, m_registry); // sets state = Failed
                ++report.isolated;
                continue;
            }

            // Loaded -> Initialized -> Active (both legal per the lifecycle table).
            (void)PluginLifecycle::Apply(c->state, LifecycleAction::Initialize);
            (void)PluginLifecycle::Apply(c->state, LifecycleAction::Activate);
            ++report.invoked;
        }
        return report;
    }

    PluginOutcome PluginHost::Invoke(const PluginId id, const PluginEvent event, const PluginArgs& args)
    {
        IPlugin* plugin = Find(id);
        if (plugin == nullptr)
        {
            return PluginOutcome::NotFound;
        }
        if (m_faults.IsIsolated(id))
        {
            return PluginOutcome::PluginDisabled;
        }
        const PluginContext* c = m_registry.Find(id);
        if (c == nullptr || c->state != PluginState::Active)
        {
            return PluginOutcome::PluginDisabled;
        }

        const PluginOutcome outcome = plugin->OnEvent(event, args);
        if (outcome != PluginOutcome::Ok)
        {
            return m_faults.Isolate(id, m_registry); // isolate; returns PluginDisabled
        }
        return PluginOutcome::Ok;
    }

    PluginHostReport PluginHost::Dispatch(const PluginEvent event, const PluginArgs& args)
    {
        PluginHostReport report;
        for (const auto& binding : m_bindings) // ascending id — deterministic
        {
            const PluginId id = binding.first;
            if (m_faults.IsIsolated(id))
            {
                ++report.skipped;
                continue;
            }
            const PluginContext* c = m_registry.Find(id);
            if (c == nullptr || c->state != PluginState::Active)
            {
                ++report.skipped;
                continue;
            }

            const PluginOutcome outcome = binding.second->OnEvent(event, args);
            if (outcome != PluginOutcome::Ok)
            {
                (void)m_faults.Isolate(id, m_registry);
                ++report.isolated;
                continue;
            }
            ++report.invoked;
        }
        return report;
    }

    void PluginHost::ShutdownAll()
    {
        for (const auto& binding : m_bindings) // ascending id — deterministic
        {
            const PluginId id = binding.first;
            PluginContext* c = m_registry.Find(id);
            if (c == nullptr)
            {
                continue;
            }
            // Shut down any plugin that ran (Initialized / Active / Suspended) or that was
            // isolated (Failed). The lifecycle table permits Shutdown from those states.
            if (PluginLifecycle::IsLegal(c->state, LifecycleAction::Shutdown))
            {
                binding.second->Shutdown();
                (void)PluginLifecycle::Apply(c->state, LifecycleAction::Shutdown);
            }
        }
    }
} // namespace stalkermp::plugin
