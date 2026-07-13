// STALKER-MP — Entity registry service (Sprint-003, §7.1; Design Review §5)
//
// Owns the authoritative EntityRegistry and exposes it to the composition root
// as a ServiceRegistry-managed service. Establishes registry ownership and
// lifetime (Design Review §5): created at the Bootstrap composition root, owned
// by the ServiceRegistry, initialized before WorldQueryService and shut down
// after it (registration order = initialization order).
//
// Engine-agnostic: no engine headers (invariant I2 / One Engine Boundary), no
// engine pointers (C2). The engine feed (reconciliation + relcase, C1-C4) is a
// separate step and lives in EngineAdapters.cpp, not here. ADR-007-clean.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/world/EntityRegistry.h"
#include "stalkermp/world/EntityRegistryConfig.h"

namespace stalkermp::world
{
    class EntityRegistryService final : public core::IService
    {
    public:
        explicit EntityRegistryService(EntityRegistryConfig configuration);

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "EntityRegistry"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {}; }
        [[nodiscard]] core::Expected<void> Initialize() override;
        void Shutdown() noexcept override;

        // --- Registry access ------------------------------------------------
        [[nodiscard]] EntityRegistry& Registry() noexcept { return m_registry; }
        [[nodiscard]] const EntityRegistry& Registry() const noexcept { return m_registry; }
        [[nodiscard]] const EntityRegistryConfig& Configuration() const noexcept { return m_configuration; }

    private:
        EntityRegistryConfig m_configuration;
        EntityRegistry m_registry;
    };
} // namespace stalkermp::world
