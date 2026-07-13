// STALKER-MP — Base interfaces (Sprint-001, §7.8)
//
// IInitializable: anything with an explicit initialize/shutdown lifecycle.
// Initialization is fallible; shutdown is not (failures during shutdown are
// logged, never thrown — the pipeline must always complete).

#pragma once

#include "stalkermp/core/Expected.h"

namespace stalkermp::core
{
    class IInitializable
    {
    public:
        virtual ~IInitializable() = default;

        [[nodiscard]] virtual Expected<void> Initialize() = 0;
        virtual void Shutdown() noexcept = 0;
    };
} // namespace stalkermp::core
