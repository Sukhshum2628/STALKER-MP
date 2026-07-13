// STALKER-MP — World service interface (Sprint-002, §7.10)
//
// Primary read surface of the World subsystem. Lifecycle control is not
// part of this interface: only the composition root (Bootstrap) drives
// lifecycle through WorldManager directly.

#pragma once

#include "stalkermp/world/WorldContext.h"
#include "stalkermp/world/WorldTypes.h"

namespace stalkermp::world
{
    class IWorld
    {
    public:
        virtual ~IWorld() = default;

        [[nodiscard]] virtual WorldLifecycleState State() const noexcept = 0;

        // Latest published context, by value (immutable snapshot).
        // Valid from the first tick after Start(); before that, returns the
        // zero context (tick == 0).
        [[nodiscard]] virtual WorldContext Context() const noexcept = 0;
    };
} // namespace stalkermp::world
