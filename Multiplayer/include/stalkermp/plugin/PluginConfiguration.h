// STALKER-MP — Plugin configuration (Sprint-014, Step 2)
//
// Parsed from the [plugins] section of server.cfg. Missing section => all defaults;
// present-but-invalid value => InvalidArgument (value outcome). The plugin directory is
// an OPAQUE token — the engine-free core performs NO path logic; only the platform
// backend (later step) resolves it. Engine-free, loader-free, and OS-free. ADR-007: no
// exceptions, no RTTI, no iostream; Expected<T> / common::Format.
//
// This file introduces the configuration value + FromConfig only — no loader, discovery,
// registration, dispatch, filesystem, or wiring.
//
// Cross-field: maxPlugins >= 1; version >= 1; pluginDirectoryToken non-empty.

#pragma once

#include <cstdint>
#include <string>

#include "stalkermp/core/Config.h"   // core::ConfigStore
#include "stalkermp/core/Expected.h" // core::Expected

namespace stalkermp::plugin
{
    struct PluginConfiguration
    {
        // Opaque plugin-location token; resolved ONLY by the platform backend (no path
        // logic in the engine-free core). Must be non-empty.
        std::string pluginDirectoryToken = "plugins";

        // Maximum number of plugins the framework will register (bounds memory). Must be >= 1.
        std::uint32_t maxPlugins = 64;

        // Whether the plugin framework is enabled at all.
        bool enabled = true;

        // Whether each plugin is validated when loaded. Policy flag only.
        bool validateOnLoad = true;

        // Whether plugin API-version compatibility is enforced strictly. Policy flag only.
        bool strictApiVersion = true;

        // Plugin config-policy version stamped into diagnostics (forward-compatible).
        // Must be >= 1.
        std::uint32_t version = 1;

        // Parses [plugins]. Missing section => defaults; present-but-invalid =>
        // InvalidArgument. Enforces the cross-field rules documented above.
        [[nodiscard]] static core::Expected<PluginConfiguration> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::plugin
