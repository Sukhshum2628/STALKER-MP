// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// IManager: a subsystem that owns the lifetime of a family of objects
// (e.g. future Bubble Manager, Connection Manager). Managers are the only
// components allowed to create and destroy the objects they manage
// (02_Engine.md §18 — explicit ownership).

#pragma once

#include "stalkermp/core/interfaces/ISubsystem.h"

namespace stalkermp::core
{
    class IManager : public ISubsystem
    {
    public:
        // Number of objects currently owned. Exposed for diagnostics and
        // shutdown validation (a manager must be empty after Shutdown).
        [[nodiscard]] virtual std::size_t ManagedObjectCount() const noexcept = 0;
    };
} // namespace stalkermp::core
