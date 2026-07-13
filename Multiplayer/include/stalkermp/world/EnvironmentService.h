// STALKER-MP — Environment service (Sprint-002, §7.8/7.10)
//
// Exposes read-only environmental state from the world's published
// context. The WorldManager owns environment sampling (one sample per
// tick, one source of truth); this service is the query surface.
//
// Ownership: owned by the ServiceRegistry; non-owning reference to IWorld.

#pragma once

#include "stalkermp/core/Expected.h"
#include "stalkermp/core/interfaces/IService.h"
#include "stalkermp/world/IEnvironmentService.h"
#include "stalkermp/world/IWorld.h"

namespace stalkermp::world
{
    class EnvironmentService final : public core::IService,
                                     public IEnvironmentService
    {
    public:
        explicit EnvironmentService(const IWorld& world)
            : m_world(world)
        {
        }

        // --- core::IService -------------------------------------------------
        [[nodiscard]] std::string_view Name() const noexcept override { return "Environment"; }
        [[nodiscard]] std::vector<std::string> Dependencies() const override { return {"World"}; }
        [[nodiscard]] core::Expected<void> Initialize() override { return core::Success(); }
        void Shutdown() noexcept override {}

        // --- IEnvironmentService ----------------------------------------------
        [[nodiscard]] WeatherId Weather() const noexcept override { return m_world.Context().weather; }
        [[nodiscard]] WorldTimeOfDay TimeOfDay() const noexcept override { return m_world.Context().timeOfDay; }
        [[nodiscard]] bool EmissionActive() const noexcept override { return m_world.Context().emissionActive; }

    private:
        const IWorld& m_world; // Non-owning.
    };
} // namespace stalkermp::world
