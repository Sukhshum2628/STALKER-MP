// STALKER-MP — Plugin loader (Sprint-014, Step 11)
//
// The engine-free deterministic loader that COORDINATES the previously-established
// components: discover manifests via `IPluginSource` (ascending), read the manifest,
// validate it via `PluginValidator` (config/descriptor/duplicate/version/APIs/deps),
// register a `PluginContext` into the `PluginRegistry`, and advance its lifecycle to
// LOADED via `PluginLifecycle` (Discovered → Validated → Loaded). A failure isolates the
// single offending plugin (recorded as a value outcome) and loading continues.
//
// The loader is NOT a runtime manager: it invokes NO plugin callback, executes no plugin,
// owns no plugin instance, performs no ticking / event dispatch / unloading / hot reload,
// and performs no lifecycle execution beyond LOADED (Initialize/Activate belong to the
// PluginManager, Step-13). Engine-free / platform-free.
//
// Engine-free / platform-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "stalkermp/plugin/IPluginSource.h"        // IPluginSource (Step 7)
#include "stalkermp/plugin/PluginConfiguration.h"  // PluginConfiguration (Step 2)
#include "stalkermp/plugin/PluginRegistry.h"       // PluginRegistry (Step 4)
#include "stalkermp/plugin/PluginTypes.h"          // PluginId / PluginOutcome / PluginDescriptor

namespace stalkermp::plugin
{
    // The outcome of a batch load. `failures` records the per-plugin isolation (id ->
    // outcome) for every plugin that did not load, in deterministic order.
    struct PluginLoadReport
    {
        std::size_t attempted = 0;
        std::size_t loaded = 0;
        std::size_t failed = 0;
        std::vector<std::pair<PluginId, PluginOutcome>> failures;
    };

    class PluginLoader
    {
    public:
        PluginLoader(IPluginSource& source, PluginRegistry& registry,
                     const std::vector<std::uint32_t>& availableApis, const PluginConfiguration& config) noexcept
            : m_source(source), m_registry(registry), m_availableApis(availableApis), m_config(config)
        {
        }

        // Load every plugin the source enumerates, in ascending id order. A per-plugin
        // failure is isolated (recorded) and does not abort the batch.
        [[nodiscard]] PluginLoadReport LoadAll();

        // Load a single plugin by id: read manifest -> validate -> register -> lifecycle to
        // LOADED. Returns Ok, or the value outcome of the first failing stage (nothing
        // registered on failure).
        [[nodiscard]] PluginOutcome LoadOne(PluginId id);

    private:
        IPluginSource& m_source;
        PluginRegistry& m_registry;
        const std::vector<std::uint32_t>& m_availableApis;
        const PluginConfiguration& m_config;
    };
} // namespace stalkermp::plugin
