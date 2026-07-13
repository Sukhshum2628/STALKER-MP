// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// IModule: a top-level STALKER-MP module (World, Player, Replication,
// Persistence, Networking, Plugins, Diagnostics). Modules aggregate
// subsystems and are registered with the ModuleRegistry (§7.10).
//
// Sprint-001 registers module *descriptors* only; concrete IModule
// implementations arrive with their respective sprints.

#pragma once

#include "stalkermp/core/Version.h"
#include "stalkermp/core/interfaces/ISubsystem.h"

namespace stalkermp::core
{
    class IModule : public ISubsystem
    {
    public:
        [[nodiscard]] virtual SemanticVersion ModuleVersion() const noexcept = 0;
    };
} // namespace stalkermp::core
