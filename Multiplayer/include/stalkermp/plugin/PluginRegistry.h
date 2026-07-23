// STALKER-MP — Plugin registry (Sprint-014, Step 4)
//
// The engine-free registry of `PluginContext` records, keyed by `PluginId`. It keeps
// deterministic ascending order, detects duplicate ids, and supports lookup and
// enumeration. Pure value component — no engine, no loader, no OS; every fallible
// operation returns a `PluginOutcome` value outcome (ADR-007). Ownership is unambiguous:
// the registry owns its `PluginContext` value copies (not the plugin instances). Loading,
// validation, and orchestration are later steps; this holds registration state only.
//
// Engine-free / loader-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <vector>

#include "stalkermp/plugin/PluginContext.h" // PluginContext
#include "stalkermp/plugin/PluginTypes.h"   // PluginId / PluginOutcome

namespace stalkermp::plugin
{
    class PluginRegistry
    {
    public:
        // Register a plugin context. Duplicate id -> DuplicatePlugin. A none (0) id is
        // rejected as NotFound (a registry entry must be addressable).
        [[nodiscard]] PluginOutcome Register(const PluginContext& context);

        // Remove a plugin by id. Missing -> NotFound.
        [[nodiscard]] PluginOutcome Unregister(PluginId id);

        // Lookup by id; nullptr when absent. Valid until the next mutation.
        [[nodiscard]] const PluginContext* Find(PluginId id) const;
        [[nodiscard]] PluginContext* Find(PluginId id);

        // All contexts in ascending id order (value copies).
        [[nodiscard]] std::vector<PluginContext> Enumerate() const;

        [[nodiscard]] std::size_t Count() const noexcept { return m_plugins.size(); }
        [[nodiscard]] bool Contains(PluginId id) const noexcept;

    private:
        // Sorted ascending by id (deterministic enumeration; binary-search lookup).
        std::vector<PluginContext> m_plugins;
    };
} // namespace stalkermp::plugin
