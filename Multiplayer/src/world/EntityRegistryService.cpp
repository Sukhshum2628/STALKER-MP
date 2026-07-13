// STALKER-MP — Entity registry service (Sprint-003, §7.1)
//
// See EntityRegistryService.h. Engine-agnostic; no exceptions, no throwing STL.

#include "stalkermp/world/EntityRegistryService.h"

#include <utility>

#include "stalkermp/core/Log.h"

namespace stalkermp::world
{
    EntityRegistryService::EntityRegistryService(EntityRegistryConfig configuration)
        : m_configuration(std::move(configuration))
    {
    }

    core::Expected<void> EntityRegistryService::Initialize()
    {
        if (m_configuration.debugLogging)
        {
            core::Log().Info("EntityRegistry", "Entity registry initialized");
        }
        return core::Success();
    }

    void EntityRegistryService::Shutdown() noexcept
    {
        // Reverse-order shutdown (Design Review §5): release all records before the
        // registry is destroyed. Clear() is noexcept and engine-free.
        m_registry.Clear();
    }
} // namespace stalkermp::world
