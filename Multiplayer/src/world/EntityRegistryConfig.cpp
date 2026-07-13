// STALKER-MP — Entity registry configuration parsing (Sprint-003, §7.1)
//
// See EntityRegistryConfig.h. Engine-agnostic; no exceptions, no throwing STL.

#include "stalkermp/world/EntityRegistryConfig.h"

namespace stalkermp::world
{
    core::Expected<EntityRegistryConfig> EntityRegistryConfig::FromConfig(const core::ConfigStore& config)
    {
        EntityRegistryConfig result;

        if (const auto debugLogging = config.GetBool("entity_registry", "debug_logging"))
        {
            result.debugLogging = *debugLogging;
        }

        return result;
    }
} // namespace stalkermp::world
