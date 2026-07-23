// STALKER-MP — Plugin event-binding registry (Sprint-014, Step 5)
//
// See EventBinding.h. Engine-free / loader-free / OS-free; no exceptions, no RTTI; value
// outcomes. Entries are kept in a vector sorted by (event, plugin) so per-event queries
// are a contiguous ascending range and lookups are binary searches.

#include "stalkermp/plugin/EventBinding.h"

#include <algorithm>
#include <cstdint>

namespace stalkermp::plugin
{
    bool EventBinding::Less(const Entry& a, const Entry& b) noexcept
    {
        if (a.event != b.event)
        {
            return static_cast<std::uint8_t>(a.event) < static_cast<std::uint8_t>(b.event);
        }
        return a.plugin.value < b.plugin.value;
    }

    PluginOutcome EventBinding::Bind(const PluginEvent event, const PluginId plugin)
    {
        if (plugin.IsNone())
        {
            return PluginOutcome::InvalidHook;
        }
        const Entry candidate{event, plugin};
        const auto it = std::lower_bound(m_entries.begin(), m_entries.end(), candidate, Less);
        if (it != m_entries.end() && !Less(candidate, *it) && !Less(*it, candidate))
        {
            return PluginOutcome::DuplicatePlugin; // exact duplicate binding
        }
        m_entries.insert(it, candidate); // preserves the sort order
        return PluginOutcome::Ok;
    }

    PluginOutcome EventBinding::Unbind(const PluginEvent event, const PluginId plugin)
    {
        const Entry key{event, plugin};
        const auto it = std::lower_bound(m_entries.begin(), m_entries.end(), key, Less);
        if (it == m_entries.end() || Less(key, *it) || Less(*it, key))
        {
            return PluginOutcome::NotFound;
        }
        m_entries.erase(it);
        return PluginOutcome::Ok;
    }

    std::vector<PluginId> EventBinding::SubscribersFor(const PluginEvent event) const
    {
        std::vector<PluginId> out;
        for (const Entry& e : m_entries) // sorted; filtered in ascending order
        {
            if (e.event == event)
            {
                out.push_back(e.plugin);
            }
        }
        return out;
    }

    std::size_t EventBinding::CountFor(const PluginEvent event) const noexcept
    {
        std::size_t n = 0;
        for (const Entry& e : m_entries)
        {
            if (e.event == event)
            {
                ++n;
            }
        }
        return n;
    }
} // namespace stalkermp::plugin
