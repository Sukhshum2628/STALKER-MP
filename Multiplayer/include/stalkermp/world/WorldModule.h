// STALKER-MP — World module identity (Sprint-002, §7.1)
//
// Attaches to the "World" slot declared in Sprint-001's ModuleRegistry.
// The module object carries identity and version; the subsystem's
// behavior lives in WorldManager (registered as a service). Splitting
// identity from behavior keeps ModuleRegistry ownership (module identity)
// separate from ServiceRegistry ownership (service lifetime).

#pragma once

#include "stalkermp/core/interfaces/IModule.h"

namespace stalkermp::world
{
    class WorldModule final : public core::IModule
    {
    public:
        [[nodiscard]] std::string_view Name() const noexcept override { return "World"; }
        [[nodiscard]] core::SemanticVersion ModuleVersion() const noexcept override { return {0, 1, 0}; }

        [[nodiscard]] core::Expected<void> Initialize() override { return core::Success(); }
        void Shutdown() noexcept override {}
    };
} // namespace stalkermp::world
