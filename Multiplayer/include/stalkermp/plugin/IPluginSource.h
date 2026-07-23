// STALKER-MP — Plugin discovery read seam (Sprint-014, Step 7)
//
// The additive engine-free READ boundary for plugin DISCOVERY: enumerate available plugin
// manifests (descriptors), read one manifest by id, and test existence. It is engine-free
// and platform-free — it exposes DISCOVERY METADATA ONLY. The real platform backend is
// Step-08 (a single platform TU behind an engine-free factory); the default/test build
// uses the in-memory / null implementations here. Mirrors the Sprint-012 `ISaveSource`
// and Sprint-013 `IScriptSource` read-seam pattern.
//
// This seam opens no libraries, loads/instantiates no plugins, validates nothing,
// manages no lifecycle, and performs no platform access — those are later steps.
//
// This step introduces the interface + in-memory/null doubles ONLY.
//
// Engine-free / loader-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream;
// value outcomes / Expected<T>.

#pragma once

#include <vector>

#include "stalkermp/core/Expected.h"       // core::Expected
#include "stalkermp/plugin/PluginTypes.h"  // PluginId / PluginDescriptor

namespace stalkermp::plugin
{
    class IPluginSource
    {
    public:
        virtual ~IPluginSource() = default;

        // All available plugin manifests, ascending by plugin id (deterministic order).
        [[nodiscard]] virtual std::vector<PluginDescriptor> Enumerate() const = 0;

        // The manifest for `id`, or an error (NotFound) when absent.
        [[nodiscard]] virtual core::Expected<PluginDescriptor> ReadManifest(PluginId id) const = 0;

        // Whether a plugin manifest with `id` exists.
        [[nodiscard]] virtual bool Exists(PluginId id) const = 0;
    };
} // namespace stalkermp::plugin
