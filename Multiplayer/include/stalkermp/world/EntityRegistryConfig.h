// STALKER-MP — Entity registry configuration (Sprint-003, §7.1)
//
// Values parsed from the [entity_registry] section of server.cfg (the host owns
// the registry; RFC-0001). Only keys consumed by current code exist; new keys
// arrive with the sprints that read them (same convention as WorldConfiguration).
//
// Ownership: value type, parsed once at the composition root and injected into
// EntityRegistryService. Engine-agnostic; no engine headers; ADR-007-clean.

#pragma once

#include "stalkermp/core/Config.h"
#include "stalkermp/core/Expected.h"

namespace stalkermp::world
{
    struct EntityRegistryConfig
    {
        // Verbose registry logging (development aid).
        bool debugLogging = false;

        // Parses the [entity_registry] section. Missing keys keep defaults.
        [[nodiscard]] static core::Expected<EntityRegistryConfig> FromConfig(const core::ConfigStore& config);
    };
} // namespace stalkermp::world
