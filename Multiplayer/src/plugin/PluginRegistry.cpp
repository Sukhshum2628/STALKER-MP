// STALKER-MP — Plugin registry (Sprint-014, Step 4)
//
// See PluginRegistry.h. Engine-free / loader-free / OS-free; no exceptions, no RTTI;
// value outcomes. The backing store is a vector kept sorted ascending by PluginId so
// enumeration is deterministic and lookup is a binary search.

#include "stalkermp/plugin/PluginRegistry.h"

#include <algorithm>

namespace stalkermp::plugin
{
    namespace
    {
        template <typename Vec>
        auto LowerBoundById(Vec& v, const PluginId id)
        {
            return std::lower_bound(v.begin(), v.end(), id,
                                    [](const PluginContext& c, const PluginId key) { return c.id < key; });
        }
    } // namespace

    PluginOutcome PluginRegistry::Register(const PluginContext& context)
    {
        if (context.id.IsNone())
        {
            return PluginOutcome::NotFound; // an entry must be addressable
        }
        const auto it = LowerBoundById(m_plugins, context.id);
        if (it != m_plugins.end() && it->id == context.id)
        {
            return PluginOutcome::DuplicatePlugin;
        }
        m_plugins.insert(it, context); // preserves ascending order
        return PluginOutcome::Ok;
    }

    PluginOutcome PluginRegistry::Unregister(const PluginId id)
    {
        const auto it = LowerBoundById(m_plugins, id);
        if (it == m_plugins.end() || it->id != id)
        {
            return PluginOutcome::NotFound;
        }
        m_plugins.erase(it);
        return PluginOutcome::Ok;
    }

    const PluginContext* PluginRegistry::Find(const PluginId id) const
    {
        const auto it = LowerBoundById(m_plugins, id);
        if (it == m_plugins.end() || it->id != id)
        {
            return nullptr;
        }
        return &*it;
    }

    PluginContext* PluginRegistry::Find(const PluginId id)
    {
        const auto it = LowerBoundById(m_plugins, id);
        if (it == m_plugins.end() || it->id != id)
        {
            return nullptr;
        }
        return &*it;
    }

    std::vector<PluginContext> PluginRegistry::Enumerate() const
    {
        return m_plugins; // already ascending by id
    }

    bool PluginRegistry::Contains(const PluginId id) const noexcept
    {
        const auto it = LowerBoundById(m_plugins, id);
        return it != m_plugins.end() && it->id == id;
    }
} // namespace stalkermp::plugin
