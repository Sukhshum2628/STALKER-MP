// STALKER-MP — Platform plugin store / static registration (Sprint-014, Step 8)
//
// The single plugin-side platform-boundary translation unit (ADR-009). Per the frozen
// [AR-P1] Option B decision, Sprint-014 supports STATIC IN-PROCESS REGISTRATION ONLY:
// statically-linked plugin modules register their manifest here, and a read-only
// discovery source is created over the registered manifests. Dynamic library loading
// (shared libraries / OS loader APIs / filesystem scanning) is explicitly DEFERRED to a
// future sprint — this boundary performs no OS/loader access in Option B.
//
// The store is discovery-only (metadata): it opens no libraries, instantiates no plugins,
// validates nothing, manages no lifecycle, and owns no plugin runtime. The concrete impl
// lives ONLY in PlatformPluginStore.cpp; callers see the engine-free factory + the
// registration API.
//
// Engine-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <memory>

#include "stalkermp/plugin/IPluginSource.h"        // plugin::IPluginSource
#include "stalkermp/plugin/PluginConfiguration.h"  // plugin::PluginConfiguration
#include "stalkermp/plugin/PluginTypes.h"          // plugin::PluginDescriptor / PluginOutcome

namespace stalkermp::adapters
{
    // Register a statically-linked plugin's manifest for discovery. Called by
    // statically-linked plugin modules at registration time. A none (0) id is NotFound;
    // a duplicate id is DuplicatePlugin; otherwise Ok. Deterministic.
    plugin::PluginOutcome RegisterStaticPlugin(const plugin::PluginDescriptor& manifest);

    // Clear the static registration table (test support / re-initialization).
    void ClearStaticPlugins() noexcept;

    // Number of currently registered static plugin manifests.
    [[nodiscard]] std::size_t StaticPluginCount() noexcept;

    // Create a read-only discovery source over the CURRENT static registrations. The
    // source snapshots the registration table at construction (read-only thereafter) and
    // enumerates deterministically (ascending plugin id). Static in-process registration
    // only ([AR-P1] Option B); no dynamic loading. `config` is accepted for composition-
    // root signature parity; the directory token is unused in Option B.
    [[nodiscard]] std::unique_ptr<plugin::IPluginSource>
    CreatePlatformPluginSource(const plugin::PluginConfiguration& config);
} // namespace stalkermp::adapters
