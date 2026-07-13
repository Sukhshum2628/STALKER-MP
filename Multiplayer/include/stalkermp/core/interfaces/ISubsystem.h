// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// ISubsystem: a named engine component with an initialize/shutdown
// lifecycle. Every subsystem owns exactly one responsibility
// (02_Engine.md §8); the name identifies it in logs and diagnostics.

#pragma once

#include <string_view>

#include "stalkermp/core/interfaces/IInitializable.h"

namespace stalkermp::core
{
    class ISubsystem : public IInitializable
    {
    public:
        // Stable identifier used in logs, diagnostics and dependency
        // declarations. Must not change during the subsystem's lifetime.
        [[nodiscard]] virtual std::string_view Name() const noexcept = 0;
    };
} // namespace stalkermp::core
