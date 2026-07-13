// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// IService: a subsystem registered with the ServiceRegistry (§7.9).
// Services declare their dependencies by service name so the registry can
// validate initialization order and detect missing prerequisites.

#pragma once

#include <string>
#include <vector>

#include "stalkermp/core/interfaces/ISubsystem.h"

namespace stalkermp::core
{
    class IService : public ISubsystem
    {
    public:
        // Names (ISubsystem::Name) of services that must be registered
        // before this service initializes. Empty when independent.
        [[nodiscard]] virtual std::vector<std::string> Dependencies() const = 0;
    };
} // namespace stalkermp::core
