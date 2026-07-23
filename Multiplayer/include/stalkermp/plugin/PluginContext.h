// STALKER-MP — Plugin context (Sprint-014, Step 4)
//
// The engine-free per-plugin value context: identity, descriptor (manifest), lifecycle
// state, and the events the plugin has bound. Value-oriented — it holds only value ids,
// value shapes, and value enums (no engine object, no loader handle, no live reference).
// It is a passive record; lifecycle transitions (Step-10) and orchestration (Step-13)
// are later steps. NOT the host-service handle — that is the reused `PluginHostServices`
// surface passed to `IPlugin::Initialize` (Step-03) and provided by `PluginHost`
// (Step-12).
//
// Engine-free / loader-free / OS-free. ADR-007: no exceptions, no RTTI, no iostream.

#pragma once

#include <vector>

#include "stalkermp/plugin/PluginTypes.h" // PluginId / PluginDescriptor / PluginState / PluginEvent

namespace stalkermp::plugin
{
    // The value context of a single plugin known to the framework.
    struct PluginContext
    {
        PluginId id{};                       // 0 = none
        PluginDescriptor descriptor{};       // manifest (id/version/api range/deps)
        PluginState state = PluginState::Discovered;
        std::vector<PluginEvent> hooks;      // events this plugin reacts to (value enums)
    };
} // namespace stalkermp::plugin
