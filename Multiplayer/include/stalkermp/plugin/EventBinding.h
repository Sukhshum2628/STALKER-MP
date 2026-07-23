// STALKER-MP — Plugin event-binding registry (Sprint-014, Step 5)
//
// The engine-free table mapping each host-dispatched `PluginEvent` to the ordered set of
// plugins subscribed to it. Plugins REACT to events; they never own, emit, or reorder
// them — this registry exposes only Bind / Unbind / query, and has no event-emission or
// dispatch surface. Subscriber order is deterministic (ascending by plugin id). Pure
// value component: no engine, no loader, no OS; every fallible operation is a value
// outcome (ADR-007).
//
// This step introduces the registry ONLY — no plugin invocation, no dispatch (Step-13),
// no engine-raised events (Step-17).
//
// Engine-free / loader-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/plugin/PluginTypes.h" // PluginEvent / PluginId / PluginOutcome

namespace stalkermp::plugin
{
    class EventBinding
    {
    public:
        // Subscribe a plugin to an event. A none (0) plugin id is InvalidHook; an exact
        // duplicate (event, plugin) is DuplicatePlugin.
        [[nodiscard]] PluginOutcome Bind(PluginEvent event, PluginId plugin);

        // Remove a specific subscription. Missing -> NotFound.
        [[nodiscard]] PluginOutcome Unbind(PluginEvent event, PluginId plugin);

        // The plugins subscribed to `event`, ascending by plugin id (deterministic).
        [[nodiscard]] std::vector<PluginId> SubscribersFor(PluginEvent event) const;

        [[nodiscard]] std::size_t Count() const noexcept { return m_entries.size(); }
        [[nodiscard]] std::size_t CountFor(PluginEvent event) const noexcept;

    private:
        struct Entry
        {
            PluginEvent event = PluginEvent::OnServerStart;
            PluginId plugin{};
        };

        // Total order: event, then plugin id (deterministic).
        [[nodiscard]] static bool Less(const Entry& a, const Entry& b) noexcept;

        // Sorted ascending by (event, plugin).
        std::vector<Entry> m_entries;
    };
} // namespace stalkermp::plugin
